// M7 step 1 (transmit: SSB voice, PTT, drive, TUNE), round 1, 2026-11-25.
// Randal's answers: one PTT/MOX chip, hold to transmit; mic source HL2+ /
// phone / Bluetooth; TX timeout default 2 min; VOX off at start. See
// docs/M7-INVENTORY.md. Pictures only.
'use strict';
(() => {
const F0 = 14.100e6, F1 = 14.160e6, V = 14.145e6, FL = [100, 2900];
const sh = 'text-shadow:0 1px 3px #000,0 0 8px rgba(0,0,0,.9)';
const RED = '255,77,77';
const note = (x, y, w, t) => `<div class="tip" style="left:${x}px;top:${y}px;width:${w}px;white-space:normal;line-height:1.35">${t}</div>`;
const lbl = (t, m = '8px 0 4px') => `<div class="lbl" style="margin:${m}">${t}</div>`;
const seg = (items, on, h = 28) => `<div class="seg" style="height:${h}px;align-items:center">${items.map(t => `<b class="${t === on ? 'on' : ''}" style="font-size:11px">${t}</b>`).join('')}</div>`;
const tog = (t, on, sub = '', st = '') => `<div class="tog ${on ? 'on' : ''}" style="flex:1;height:32px;${st}"><span style="font-size:12px">${t}${sub ? `<span class="dim" style="font-size:9.5px;margin-left:6px">${sub}</span>` : ''}</span><s></s></div>`;
const sl = (l, v, p, w, m = '6px 0 0') => `<div style="margin:${m}">${slider(l, v, p, w)}</div>`;
const dim = () => abs(0, 0, 832, 384, '', '', 'background:rgba(0,0,0,.42)');
const panel = (title, sub, body, x = 96, w = 640, y = 12, h = 362) => dim() + abs(x, y, w, h, `<div style="padding:12px 16px">
  <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">${title}</span><span class="dim" style="font-size:10px">${sub}</span></div>${body}</div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.96)');

// ---- the receive-side pieces, as in the app today ----
const meterCard = () => abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px');
const tBtns = () => abs(720, 12, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass" style="font-weight:600;font-size:13px">2D</div><div class="ico glass">${icon('menu')}</div></div>`);
const pill = (tx = false) => abs(266, 8, 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%">
  ${tx ? `<span class="tag" style="background:rgb(${RED});color:#fff">TX</span>` : '<span class="tag vfoA">A</span>'}${fq(V, 34)}
  <div class="col" style="gap:3px"><span class="tag acc">USB</span><span class="tag">2.8k</span></div></div>`, 'glass',
  tx ? `border-color:rgba(${RED},.85);box-shadow:0 0 20px rgba(${RED},.45)` : '');
const info = () => abs(180, 14, 84, 54, `<div class="col" style="gap:2px;align-items:center;font-size:10.5px;${sh}"><span style="font-size:15px;font-weight:500">18:42</span><span class="dim">UTC</span></div>`) +
  abs(570, 14, 148, 54, `<div class="col" style="gap:3px;align-items:center;font-size:10.5px;${sh}"><span class="dim">SFI 142 · A 6 · K 2</span><span style="color:var(--grn)">ADC OK</span><span class="dim" style="font-size:9.5px">HL2+ 169.254.19.221</span></div>`);

// ---- the new chips ----
// state: 'rx' (ready), 'tx' (held, on the air), 'locked' (transmit not enabled yet)
function ptt(state = 'rx', left = '1:52', w = 132) {
  if (state === 'tx') return `<div class="chip" style="min-width:${w}px;height:46px;background:rgba(${RED},.92);color:#fff;box-shadow:0 0 22px rgba(${RED},.7);border:1px solid rgba(255,200,200,.8)">ON AIR<b style="color:#fff;font-size:15px">${left}</b></div>`;
  if (state === 'locked') return `<div class="chip glass" style="min-width:${w}px;opacity:.55">PTT<b class="row" style="gap:5px;font-size:13px">${icon('lock', 13, 2)} TX off</b></div>`;
  return `<div class="chip glass" style="min-width:${w}px;border:1.5px solid rgba(${RED},.8);color:#ff9d9d">HOLD<b style="color:#ff6b6b;font-size:17px;font-weight:600;letter-spacing:1px">PTT</b></div>`;
}
// one TX chip (Randal 2026-11-25: not three chips that all open the same panel); shows the drive
const CH_RX = [['BAND', '20m'], ['AF', '73'], ['NR', 'NR2', true], ['FILTER', '2.8k'], ['STEP', '10 Hz'], ['TX', '40 %']];
const CH_TX = CH_RX;
const chipRow = (state = 'rx', chips = CH_RX, left) => abs(10, 328, 812, 46, `<div class="row" style="gap:7px">${chips.map(([l, v, on]) => chip(l, v, on)).join('')}<div style="flex:1"></div>${ptt(state, left)}</div>`);

// ---- TX meter: big PWR bar (Randal's X6100 pick), SWR, ALC always shown, MIC ----
function bar(w, h, frac, cols, n = 24) {
  const gap = 2, sw = (w - gap * (n - 1)) / n;
  let s = '';
  for (let i = 0; i < n; i++) {
    const f = Math.max(0, Math.min(1, frac * n - i)), c = cols(i / (n - 1));
    s += `<u style="width:${sw}px;height:${h}px;${f > 0 ? `background:linear-gradient(90deg, rgba(${c},.95) ${f * 100}%, rgba(255,255,255,.08) ${f * 100}%)` : ''}"></u>`;
  }
  return `<div class="meter">${s}</div>`;
}
const cPwr = p => p < 0.8 ? '62,224,122' : p < 0.92 ? '255,179,65' : RED;
const cSwr = p => p < 0.4 ? '62,224,122' : p < 0.6 ? '255,179,65' : RED;
const cAlc = p => p < 0.7 ? '63,208,255' : RED;
const scale = (w, items) => `<div class="mscale" style="width:${w}px;margin-top:2px">${items.map(t => `<span>${t}</span>`).join('')}</div>`;
function txMeter(w, big = 1) {
  const bw = w - 2 * 11;
  const row = (name, val, frac, cols, h, sc) => `<div style="margin-top:${6 * big}px"><div class="row" style="justify-content:space-between;margin-bottom:3px"><span class="lbl">${name}</span><span style="font-size:${12 * big}px;font-weight:500">${val}</span></div>${bar(bw, h, frac, cols)}${sc ? scale(bw, sc) : ''}</div>`;
  return `<div class="row" style="justify-content:space-between"><span style="font-size:${22 * big}px;font-weight:500">4.1 W</span><span class="dim" style="font-size:${10.5 * big}px">HL2 out · 5 W scale</span></div>
    <div style="margin-top:4px">${bar(bw, 15 * big, 0.82, cPwr)}${scale(bw, ['0', '1', '2', '3', '4', '5 W'])}</div>
    ${row('SWR', '1.3', 0.12, cSwr, 7 * big)}
    ${row('ALC', '−2 dB', 0.55, cAlc, 7 * big)}
    ${row('MIC', '−8 dB', 0.66, p => p < 0.85 ? '63,208,255' : RED, 7 * big)}`;
}

// wide and short: PWR across the top, SWR / ALC / MIC side by side under it
function txWide(w) {
  const bw = w - 28, cw = (bw - 2 * 18) / 3;
  const mini = (name, val, frac, cols) => `<div style="width:${cw}px"><div class="row" style="justify-content:space-between;margin-bottom:3px"><span class="lbl">${name}</span><span style="font-size:13px;font-weight:500">${val}</span></div>${bar(cw, 8, frac, cols, 14)}</div>`;
  return `<div class="row" style="gap:12px;align-items:baseline"><span style="font-size:26px;font-weight:500">4.1 W</span><span class="dim" style="font-size:10.5px">HL2 out · 5 W scale</span></div>
    <div style="margin-top:3px">${bar(bw, 18, 0.82, cPwr, 30)}${scale(bw, ['0', '1', '2', '3', '4', '5 W'])}</div>
    <div class="row" style="gap:18px;margin-top:7px">${mini('SWR', '1.3', 0.12, cSwr)}${mini('ALC', '−2 dB', 0.55, cAlc)}${mini('MIC', '−8 dB', 0.66, p => p < 0.85 ? '63,208,255' : RED)}</div>`;
}

// ---- the TX spectrum: our own SSB signal, drawn over the top half ----
function txSpectrum(ctx, x, y, w, h, o) {
  const lo = -120, hi = 20, nb = Math.round(w * 2), X = f => x + (f - o.f0) / (o.f1 - o.f0) * w;
  const g0 = ctx.createLinearGradient(0, y, 0, y + h);
  g0.addColorStop(0, '#0b0405'); g0.addColorStop(1, '#14080a');
  ctx.fillStyle = g0; ctx.fillRect(x, y, w, h);
  ctx.strokeStyle = 'rgba(255,255,255,0.06)'; ctx.fillStyle = 'rgba(255,255,255,0.35)'; ctx.font = '9px Roboto';
  for (let d = -100; d <= 0; d += 20) {
    const yy = y + h - (d - lo) / (hi - lo) * h;
    ctx.beginPath(); ctx.moveTo(x, yy); ctx.lineTo(x + w, yy); ctx.stroke(); ctx.fillText(d, x + 4, yy - 2);
  }
  let seed = 7; const r = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
  const lev = new Float32Array(nb);
  for (let j = 0; j < nb; j++) {
    const f = o.f0 + (j + 0.5) / nb * (o.f1 - o.f0), a = f - o.vfo;
    let d = -106 + r() * 5;                                        // floor
    if (a > o.filt[0] && a < o.filt[1]) {                          // voice: strong 300-1200 Hz, tapering to 2.9 kHz
      const t = (a - o.filt[0]) / (o.filt[1] - o.filt[0]);
      d = -30 - 16 * Math.pow(t, 1.3) - (t < 0.06 ? (0.06 - t) * 300 : 0) + (r() - 0.5) * 7;
    } else if (a >= o.filt[1] && a < o.filt[1] + 900) d = Math.max(d, -50 - (a - o.filt[1]) / 900 * 52);   // skirt
    else if (a <= o.filt[0] && a > -o.filt[1]) d = Math.max(d, -94 + (r() - 0.5) * 5);                     // the other sideband, ~−60 dB
    if (Math.abs(a) < 25) d = Math.max(d, -80);                    // carrier leak
    lev[j] = d;
  }
  const Y = d => y + h - Math.max(0, Math.min(1, (d - lo) / (hi - lo))) * h;
  ctx.beginPath(); ctx.moveTo(x, y + h);
  for (let j = 0; j < nb; j++) ctx.lineTo(x + (j + 0.5) / nb * w, Y(lev[j]));
  ctx.lineTo(x + w, y + h); ctx.closePath();
  const g = ctx.createLinearGradient(0, y, 0, y + h);
  g.addColorStop(0, 'rgba(255,140,60,0.6)'); g.addColorStop(1, 'rgba(255,90,60,0.03)');
  ctx.fillStyle = g; ctx.fill();
  ctx.beginPath(); for (let j = 0; j < nb; j++) { const xx = x + (j + 0.5) / nb * w; j ? ctx.lineTo(xx, Y(lev[j])) : ctx.moveTo(xx, Y(lev[j])); }
  ctx.save(); ctx.strokeStyle = '#ffd2a8'; ctx.lineWidth = 1.2; ctx.shadowBlur = 6; ctx.shadowColor = '#ff8a3c'; ctx.stroke(); ctx.restore();
  // TX filter edges and the TX frequency
  const xa = X(o.vfo + o.filt[0]), xb = X(o.vfo + o.filt[1]);
  ctx.fillStyle = 'rgba(255,90,90,0.08)'; ctx.fillRect(xa, y, xb - xa, h);
  ctx.strokeStyle = 'rgba(255,120,120,0.55)'; ctx.beginPath(); ctx.moveTo(xa, y); ctx.lineTo(xa, y + h); ctx.moveTo(xb, y); ctx.lineTo(xb, y + h); ctx.stroke();
  ctx.strokeStyle = 'rgb(' + RED + ')'; ctx.lineWidth = 1.4; ctx.beginPath(); ctx.moveTo(X(o.vfo), y); ctx.lineTo(X(o.vfo), y + h); ctx.stroke();
}

// canvas specs; `custom` is called instead of R.paint (mock.html)
const rxSpec = () => ({
  f0: F0, f1: F1, vfo: V, filt: FL, want: 7, bandplan: true,
  parts: [{ kind: 'spec2d', h: 0.5, dbl: true, line: '#cfe9ff', fillTop: 'rgba(80,140,255,0.45)' }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }],
});
// split: fraction of the height the TX spectrum takes; frozen: dim the waterfall
const txSpec = (split = 0.5, frozen = true) => ({
  ...rxSpec(), custom: (cv, sp) => {
    sp.parts[0].h = split; sp.parts[1].h = 1 - split;
    R.paint(cv, sp);
    const ctx = cv.getContext('2d'), W = cv.clientWidth, H = cv.clientHeight;
    txSpectrum(ctx, 0, 0, W, H * split - 8, sp);
    if (frozen) {
      ctx.fillStyle = 'rgba(0,0,0,0.5)'; ctx.fillRect(0, H * split + 22, W, H);
    }
  },
});
const frozenNote = (y = 226) => abs(0, y, 832, 20, `<div style="text-align:center;font-size:11px;color:#c9d2e2;${sh}">receiver paused while transmitting · waterfall resumes on release</div>`);

Object.assign(LAYOUTS, {
  // M1: receive, with the new chips. PTT at the right end of the bottom row (thumb).
  M1: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + info() + pill() + tBtns() + chipRow('rx') +
      note(20, 150, 330, '<b style="color:#ffd27a">Receive, with the transmit chips</b><br><span class="dim">PTT sits alone at the right end of the bottom row, under your right thumb: <b>hold</b> to transmit, let go to receive. One TX chip (shows the drive) opens the transmit panel (M5b). Nothing transmits on a tap.</span>'),
    canvases: [['#c', rxSpec()]],
  }),

  // M2: transmitting, A: the S-meter card turns into the TX meter (same corner, bigger)
  M2: () => ({
    html: cv('c', 0, 0, 832, 384) + abs(10, 10, 236, null, txMeter(236), 'glass', `padding:9px 11px;border-color:rgba(${RED},.5)`) +
      pill(true) + tBtns() + frozenNote(206) + chipRow('tx', CH_TX) +
      note(260, 238, 400, '<b style="color:#ffd27a">A: the meter card becomes the TX meter</b><br><span class="dim">same top-left corner, wider while you transmit: thick PWR bar, SWR, ALC and MIC. The spectrum shows your own signal (orange) with the TX filter; the waterfall stops. The PTT chip counts down the 2-minute timeout.</span>'),
    canvases: [['#c', txSpec(0.5)]],
  }),

  // M3: transmitting, B: a big TX meter in the middle, over the stopped waterfall (the X6100 way:
  // the meter takes the space that isn't useful during TX)
  M3: () => ({
    html: cv('c', 0, 0, 832, 384) + pill(true) + tBtns() + info() +
      abs(136, 208, 560, 112, txWide(560), 'glass', `padding:9px 14px;border-color:rgba(${RED},.5);background:rgba(14,8,12,.84)`) +
      chipRow('tx', CH_TX) +
      note(20, 96, 330, '<b style="color:#ffd27a">B: big meter in the middle</b><br><span class="dim">the S-meter card hides; a large TX meter covers the paused waterfall, where nothing happens during TX anyway (your X6100 pick: the dial\'s space during TX).</span>'),
    canvases: [['#c', txSpec(0.5)]],
  }),

  // M4: transmitting, C: nothing moves; a red frame, and three tall meters on the right edge
  M4: () => ({
    html: cv('c', 0, 0, 832, 384) + abs(0, 0, 832, 384, '', '', `box-shadow:inset 0 0 0 3px rgba(${RED},.9),inset 0 0 26px rgba(${RED},.35);pointer-events:none`) +
      pill(true) + tBtns() + info() +
      abs(686, 66, 136, 252, `<div style="padding:8px 10px"><div style="font-size:20px;font-weight:500;text-align:center">4.1 W</div>
        <div class="row" style="gap:8px;justify-content:center;margin-top:6px">
        ${[['PWR', 82, '4.1'], ['SWR', 12, '1.3'], ['ALC', 55, '−2']].map(([l, p, v]) => `<div class="col" style="align-items:center;gap:3px"><div class="vsl" style="height:168px;width:30px"><i style="height:${p}%;background:linear-gradient(0deg,rgba(62,224,122,.5),rgba(${p > 70 ? '255,179,65' : '62,224,122'},.9))"></i></div><span class="lbl">${l}</span><span style="font-size:11px">${v}</span></div>`).join('')}</div></div>`, 'glass', `border-color:rgba(${RED},.5)`) +
      chipRow('tx', CH_TX) +
      note(20, 96, 330, '<b style="color:#ffd27a">C: nothing moves</b><br><span class="dim">a red frame round the whole screen says "on the air"; PWR, SWR and ALC as three tall bars on the right edge. MIC level shows in the meters panel.</span>'),
    canvases: [['#c', txSpec(0.5, true)]],
  }),

  // M5: the drive / mic panel (from the DRIVE, MIC or TUNE chip), the LEVELS panel's way (pick A)
  M5: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + pill() + chipRow('rx', [['BAND', '20m'], ['AF', '73'], ['NR', 'NR2', true], ['FILTER', '2.8k'], ['STEP', '10 Hz'], ['TX', '40 %', true]]) +
      abs(536, 8, 288, 314, `<div style="padding:10px 12px">
        <div class="row" style="justify-content:space-between"><span style="font-size:15px;font-weight:500">Transmit</span><span class="dim" style="font-size:16px">✕</span></div>
        ${lbl('DRIVE', '6px 0 3px')}${sl('DRIVE', '40 % · ≈2.0 W', 40, 264, '0')}
        <div class="row" style="gap:6px;margin-top:5px">${tog('Per band', true)}</div>
        ${lbl('MICROPHONE', '9px 0 3px')}${seg(['HL2+ jack', 'Phone', 'Bluetooth'], 'HL2+ jack')}
        ${sl('MIC GAIN', '+6 dB', 55, 264)}
        <div class="row" style="gap:6px;margin-top:5px">${tog('Mic Boost', false, '+20 dB')}</div>
        ${lbl('TUNE', '9px 0 3px')}<div class="row" style="gap:6px">
          <div class="btn" style="flex:1;height:34px;border-color:rgba(${RED},.6);color:#ff8a8a">TUNE · 10 %</div>
          <div class="btn" style="flex:1;height:34px">XPA125B tune</div></div></div>`, 'glass', 'border-radius:16px;background:rgba(10,14,26,.92)') +
      note(20, 150, 300, '<b style="color:#ffd27a">Drive, mic and TUNE in one panel</b><br><span class="dim">opens from the DRIVE, MIC or TUNE chip, beside the spectrum like LEVELS (no dimming). Drive shows % and the HL2\'s watts. TUNE: tap to start, tap to stop (the timeout still applies); "XPA125B tune" starts the amplifier\'s tuner through the IO board.</span>'),
    canvases: [['#c', rxSpec()]],
  }),

  // M5b (Randal picked A, asked how tune % is changed): TUNE DRIVE slider in the panel
  M5b: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + pill() + chipRow('rx', [['BAND', '20m'], ['AF', '73'], ['NR', 'NR2', true], ['FILTER', '2.8k'], ['STEP', '10 Hz'], ['TX', '40 %', true]]) +
      abs(536, 8, 288, 314, `<div style="padding:9px 12px">
        <div class="row" style="justify-content:space-between"><span style="font-size:15px;font-weight:500">Transmit</span><span class="dim" style="font-size:16px">✕</span></div>
        ${lbl('DRIVE', '4px 0 3px')}${sl('DRIVE', '40 % · ≈2.0 W', 40, 264, '0')}
        <div class="row" style="gap:6px;margin-top:4px">${tog('Per band', true)}</div>
        ${lbl('MICROPHONE', '7px 0 3px')}${seg(['HL2+ jack', 'Phone', 'Bluetooth'], 'HL2+ jack', 26)}
        <div class="row" style="gap:6px;margin-top:4px"><div style="flex:1.5">${slider('MIC GAIN', '+6 dB', 55, 150)}</div>${tog('Boost', false)}</div>
        <div style="margin-top:7px;padding:6px 8px;border-radius:12px;border:1px dashed rgba(255,210,122,.6)">
          ${lbl('TUNE', '0 0 3px')}${slider('TUNE DRIVE', '10 % · ≈0.5 W', 10, 246)}
          <div class="row" style="gap:6px;margin-top:5px">
          <div class="btn" style="flex:1;height:30px;border-color:rgba(${RED},.6);color:#ff8a8a">TUNE</div></div></div></div>`, 'glass', 'border-radius:16px;background:rgba(10,14,26,.92)') +
      note(20, 150, 300, '<b style="color:#ffd27a">Tune drive in the panel</b><br><span class="dim">slide TUNE DRIVE to set how hard TUNE drives (% and the HL2\'s watts), then tap TUNE: it sends the carrier and starts the XPA125B\'s tuner through your Pico (as deskHPSDR does). Saved like deskHPSDR\'s "Tune Drive level"; "Tune uses TX drive" (TX page) makes it follow the DRIVE slider instead. Mic gain and Boost share a line to make room.</span>'),
    canvases: [['#c', rxSpec()]],
  }),

  // M6: ☰ → TX: the settings page (deskHPSDR's TX menu, step 1 lines)
  M6: () => ({
    html: cv('c', 0, 0, 832, 384) + panel('TX', "deskHPSDR's TX menu · step 1 (voice, drive, TUNE)", `
      <div class="row" style="gap:16px;margin-top:6px;align-items:flex-start">
        <div style="flex:1">${lbl('TRANSMIT')}
          <div class="row">${tog('Transmit enabled', true, 'safety')}</div>
          <div class="row" style="margin-top:6px">${tog('HL2 5 W PA', true)}</div>
          ${lbl('TX TIMEOUT', '10px 0 4px')}${seg(['1', '2', '3', '5', '10', 'Off'], '2')}
          <div class="dim" style="font-size:9.5px;margin-top:4px">minutes; then back to receive with a message</div>
          ${lbl('VOX', '10px 0 4px')}<div class="dim" style="font-size:10.5px">always off when the app starts</div></div>
        <div style="flex:1">${lbl('PROTECTION')}
          <div class="row">${tog('SWR protection', true, 'drive → 0')}</div>
          ${sl('SWR ALARM AT', '2.5', 38, 286)}
          <div class="row" style="margin-top:6px">${tog('Transmit out of band', false)}</div>
          <div class="row" style="margin-top:6px">${tog('Mute RX when TX', true)}</div></div>
        <div style="flex:1">${lbl('TX FILTER')}
          ${sl('LOW', '100 Hz', 10, 172, '0')}${sl('HIGH', '2900 Hz', 72, 172)}
          <div class="row" style="margin-top:6px">${tog('TX uses RX filter', false)}</div>
          ${lbl('TUNE', '10px 0 4px')}${sl('TUNE DRIVE', '10 %', 10, 172, '0')}
          <div class="row" style="margin-top:6px">${tog('Tune uses TX drive', false)}</div></div>
      </div>`, 64, 704),
    canvases: [['#c', rxSpec()]],
  }),

  // M7: ☰ → PA: per-band calibration, meter scale, watt meter calibration
  M7: () => ({
    html: cv('c', 0, 0, 832, 384) + panel('PA', "deskHPSDR's PA menu", `
      <div class="row" style="gap:18px;margin-top:6px;align-items:flex-start">
        <div style="flex:1.4">${lbl('PA CALIBRATION PER BAND (dB · 38.8 = full 5 W)')}
          <div style="display:grid;grid-template-columns:repeat(2,1fr);gap:5px 14px">
          ${['160m', '80m', '60m', '40m', '30m', '20m', '17m', '15m', '12m', '10m', '6m'].map(b => `<div class="row" style="gap:8px"><span style="width:36px;font-size:12px">${b}</span><div class="sl" style="flex:1;height:24px"><i style="width:2%"></i><em style="line-height:24px;font-size:12px">38.8</em></div></div>`).join('')}
          </div></div>
        <div style="flex:1">${lbl('METER FULL SCALE')}${seg(['1 W', '5 W', '10 W', '100 W'], '5 W')}
          <div class="dim" style="font-size:10px;margin-top:4px;line-height:1.35">the HL2 measures its own output (the XPA125B's input), so 5 W. The amplifier shows its own power and SWR.</div>
          <div class="btn" style="height:34px;margin-top:10px">Watt meter calibrate…</div>
          <div class="row" style="gap:6px;margin-top:10px"><div class="btn" style="flex:1;height:32px">Save</div><div class="btn" style="flex:1;height:32px">Import</div><div class="btn" style="flex:1;height:32px">Reset</div></div></div>
      </div>`, 64, 704),
    canvases: [['#c', rxSpec()]],
  }),

  // M8: the warnings: TX not enabled yet, high SWR, timeout
  M8: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + pill() + tBtns() +
      abs(570, 14, 148, 54, `<div class="col" style="gap:4px;align-items:center;font-size:10.5px;${sh}"><span style="color:#fff;background:rgba(220,40,40,.92);border-radius:8px;padding:2px 8px;font-weight:600">HIGH SWR 3.1</span><span style="color:#ffb3b3">drive set to 0</span></div>`) +
      abs(236, 150, 360, 44, `<div class="row" style="gap:10px;height:100%;padding:0 14px"><span style="font-size:18px">⏱</span><span style="font-size:12.5px">TX timeout (2 min) · back to receive</span></div>`, 'glass', `border-color:rgba(255,179,65,.7);background:rgba(30,20,6,.9)`) +
      abs(10, 328, 812, 46, `<div class="row" style="gap:7px">${CH_RX.map(([l, v, on]) => chip(l, v, on)).join('')}<div style="flex:1"></div>${ptt('locked')}</div>`) +
      note(20, 240, 380, '<b style="color:#ffd27a">Warnings</b><br><span class="dim">top right: SWR above the alarm, drive dropped to 0 (upstream\'s protection; not while tuning). Middle: the timeout ended a transmission. Bottom right: the PTT chip while "Transmit enabled" is off (until the first on-air check): greyed, with a lock.</span>'),
    canvases: [['#c', rxSpec()]],
  }),
});
// shared with layouts12.js (M7 step 2)
window.M7H = { txSpec, rxSpec, txMeter, pill, ptt, tBtns, meterCard, info, chipRow, note, lbl, seg, tog, sl, panel, dim, RED, frozenNote };
})();
