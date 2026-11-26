"""Convert an IQ WAV (16-bit stereo, I=left Q=right) to an HL2 sample rate.
usage: iq_convert.py in.wav out.wav rate seconds gain"""
import sys, struct, numpy as np
from math import gcd
from scipy.signal import resample_poly
src, dst, rate, secs, gain = sys.argv[1], sys.argv[2], int(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
# walk the chunks (SDR# / SDR++ put big auxi / LIST chunks before 'data')
f = open(src, 'rb'); hdr = f.read(12); sr = tag = bits = None
assert hdr[:4] == b'RIFF' and hdr[8:12] == b'WAVE', 'not a WAV file'
while True:
    c = f.read(8)
    if len(c) < 8: sys.exit('no data chunk')
    cid, n = c[:4], struct.unpack('<I', c[4:8])[0]
    if cid == b'fmt ':
        fm = f.read(n); tag, ch, sr = struct.unpack('<HHI', fm[:8]); bits = struct.unpack('<H', fm[14:16])[0]
        if tag == 0xFFFE and len(fm) >= 26: tag = struct.unpack('<H', fm[24:26])[0]   # extensible: SubFormat
        if n & 1: f.read(1)
        if ch != 2: sys.exit('needs 2 channels (I/Q)')
    elif cid == b'data':
        off = f.tell(); dlen = n; break
    else:
        f.seek(n + (n & 1), 1)
if tag == 1 and bits == 16:
    x = np.memmap(src, dtype=np.int16, mode='r', offset=off, shape=(dlen // 4, 2))   # only the data chunk
    x = x[:int(secs * sr)].astype(np.float64)
elif tag == 3 and bits == 32:
    x = np.memmap(src, dtype=np.float32, mode='r', offset=off, shape=(dlen // 8, 2))
    x = x[:int(secs * sr)].astype(np.float64) * 32768.0   # to the 16-bit scale the gain is given in
else:
    sys.exit(f'unsupported format: tag {tag}, {bits} bits (16-bit PCM or 32-bit float)')
z = x[:, 0] + 1j * x[:, 1]
g = gcd(rate, sr); up, down = rate // g, sr // g
y = resample_poly(z, up, down) * gain
peak = max(np.abs(y.real).max(), np.abs(y.imag).max())
print(f"in {sr} Hz -> {rate} Hz (x{up}/{down}), {len(y)/rate:.1f} s, peak {peak:.0f}")
out = np.empty((len(y), 2), dtype=np.int16)
out[:, 0] = np.clip(np.round(y.real), -32767, 32767); out[:, 1] = np.clip(np.round(y.imag), -32767, 32767)
data = out.tobytes()
hdr = b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 2, rate, rate * 4, 4, 16) + b'data' + struct.pack('<I', len(data))
open(dst, 'wb').write(hdr + data)
