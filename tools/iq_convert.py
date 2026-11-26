"""Convert an IQ WAV (16-bit stereo, I=left Q=right) to an HL2 sample rate.
usage: iq_convert.py in.wav out.wav rate seconds gain"""
import sys, struct, numpy as np
from math import gcd
from scipy.signal import resample_poly
src, dst, rate, secs, gain = sys.argv[1], sys.argv[2], int(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
f = open(src, 'rb'); d = f.read(4096); i = 12; sr = None
while True:
    cid = d[i:i+4]; n = struct.unpack('<I', d[i+4:i+8])[0]
    if cid == b'fmt ': sr = struct.unpack('<I', d[i+12:i+16])[0]
    if cid == b'data': off = i + 8; break
    i += 8 + n + (n & 1)
x = np.memmap(src, dtype=np.int16, mode='r', offset=off).reshape(-1, 2)
x = x[:int(secs * sr)].astype(np.float64)
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
