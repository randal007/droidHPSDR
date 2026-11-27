// M7 step 4 (PureSignal), round 1, 2026-11-26: deskHPSDR's PS menu (ps_menu.c):
// Enable / OFF / Restart, Two Tone or Noise as the calibration source, Auto
// Attenuate + TX ATT (-29..+31 dB on an HL2, kept per band), the feedback level
// and Correcting status, MON, OneShot, stability, TX delay, SetPk, AmpView.
// Three ways to place it (S1-S3), AmpView two ways (S4, S5), settings (S6). Pictures only.
'use strict';
(() => {
const H = window.M7H;
const { lbl, seg, sl, note, dim, RED } = H;
const GRN = '62,224,122', YEL = '255,210,80', BLU = '70,140,255';
const CH = [['BAND', '20m'], ['AF', '73'], ['NR', 'NR2', true], ['FILTER', '2.8k'], ['PROC', '4 dB', true], ['PS', '152 ✓', true], ['TX', '40 %']];
const btn = (t, w = 'flex:1', extra = '') => `<div class="btn" style="${w};height:30px;font-size:11.5px;${extra}">${t}</div>`;
const togS = (t, on, sub = '') => `<div class="tog ${on ? 'on' : ''}" style="flex:1;height:32px"><span style="font-size:12px">${t}${sub ? `<span class="dim" style="font-size:9.5px;margin-left:6px">${sub}</span>` : ''}</span><s></s></div>`;
const dot = (c, t) => `<span class="row" style="gap:5px;font-size:11.5px"><i style="width:9px;height:9px;border-radius:50%;background:rgb(${c});box-shadow:0 0 8px rgb(${c})"></i>${t}</span>`;

// the feedback level on deskHPSDR's colours (red < 90, yellow, green 128..181, blue > 181), 0..256,
// with the "optimal 140..165" band marked
function fbBar(w, level, h = 14) {
  const X = v => v / 256 * w;
  const zones = [[0, 90, RED], [90, 128, YEL], [128, 181, GRN], [181, 256, BLU]];
  const c = level > 181 ? BLU : level > 128 ? GRN : level > 90 ? YEL : RED;
  return `<div style="position:relative;width:${w}px;height:${h + 16}px">
    ${zones.map(([a, b, z]) => `<div style="position:absolute;left:${X(a)}px;width:${X(b) - X(a)}px;top:0;height:${h}px;background:rgba(${z},.16)"></div>`).join('')}
    <div style="position:absolute;left:0;top:0;width:${X(level)}px;height:${h}px;border-radius:4px 0 0 4px;background:linear-gradient(90deg,rgba(${c},.35),rgba(${c},.95))"></div>
    <div style="position:absolute;left:${X(140)}px;width:${X(165) - X(140)}px;top:-3px;height:${h + 6}px;border:1.5px solid rgba(255,255,255,.75);border-radius:3px"></div>
    <div class="row" style="position:absolute;left:0;top:${h + 2}px;width:${w}px;justify-content:space-between;font-size:8.5px;color:var(--dim)"><span>0</span><span style="margin-left:${X(120)}px">140-165</span><span>256</span></div></div>`;
}

// AmpView: amplifier AM/AM (blue dots) and AM/PM (yellow dots), PS's correction (red, green)
function ampSvg(w, h, split = false) {
  const pad = 34, gw = w - 2 * pad, top = 14;
  const amp = x => Math.tanh(1.7 * x) / Math.tanh(1.7);          // a compressing PA
  const ph = x => -22 * x * x;                                     // AM/PM in degrees
  let s = 1;
  const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647 - 0.5;
  const pts = [];
  for (let i = 0; i < 420; i++) { const x = Math.pow(i / 420, 0.7); pts.push([x, amp(x) + rnd() * 0.025, ph(x) + rnd() * 2.2]); }
  const graph = (y0, gh, kind) => {
    const X = v => pad + v * gw, Ym = v => y0 + gh - v * gh, Yp = d => y0 + gh / 2 - d / 30 * (gh / 2);
    let g = `<rect x="${pad}" y="${y0}" width="${gw}" height="${gh}" fill="rgba(0,0,0,.35)" stroke="rgba(255,255,255,.25)"/>`;
    for (let i = 1; i < 5; i++) g += `<line x1="${X(i / 5)}" x2="${X(i / 5)}" y1="${y0}" y2="${y0 + gh}" stroke="rgba(255,255,255,.07)"/><line x1="${pad}" x2="${pad + gw}" y1="${y0 + gh * i / 5}" y2="${y0 + gh * i / 5}" stroke="rgba(255,255,255,.07)"/>`;
    if (kind !== 'p') {
      g += `<line x1="${X(0)}" y1="${Ym(0)}" x2="${X(1)}" y2="${Ym(1)}" stroke="rgba(255,255,255,.35)" stroke-dasharray="4 4"/>`;
      g += pts.map(([x, y]) => `<rect x="${X(x)}" y="${Ym(y)}" width="1.6" height="1.6" fill="#3fa6ff"/>`).join('');
      let d = ''; for (let i = 0; i <= 60; i++) { const x = i / 60; d += `${i ? 'L' : 'M'}${X(x)},${Ym(Math.min(1, x * x / amp(x) || 0))}`; }
      g += `<path d="${d}" fill="none" stroke="#f2335a" stroke-width="2"/>`;
      g += [0, .5, 1].map(v => `<text x="${pad - 20}" y="${Ym(v) + 4}" fill="#8b97ad" font-size="9">${v.toFixed(1)}</text>`).join('');
    }
    if (kind !== 'm') {
      g += `<line x1="${pad}" x2="${pad + gw}" y1="${Yp(0)}" y2="${Yp(0)}" stroke="rgba(255,255,255,.3)" stroke-dasharray="4 4"/>`;
      g += pts.map(([x, , p]) => `<rect x="${X(x)}" y="${Yp(p)}" width="1.6" height="1.6" fill="#ffcc26"/>`).join('');
      let d = ''; for (let i = 0; i <= 60; i++) { const x = i / 60; d += `${i ? 'L' : 'M'}${X(x)},${Yp(-ph(x))}`; }
      g += `<path d="${d}" fill="none" stroke="#34d94d" stroke-width="2"/>`;
      g += [-30, 0, 30].map(v => `<text x="${pad + gw + 4}" y="${Yp(v) + 4}" fill="#8b97ad" font-size="9">${v > 0 ? '+' : ''}${v}°</text>`).join('');
    }
    return g;
  };
  const legend = (x, y, items) => items.map(([c, t], i) => `<rect x="${x + i * 92}" y="${y - 7}" width="14" height="3" fill="${c}"/><text x="${x + i * 92 + 18}" y="${y - 3}" fill="#c9d2e2" font-size="10">${t}</text>`).join('');
  let body;
  if (split) {
    const gh = (h - top - 46) / 2;
    body = graph(top, gh, 'm') + graph(top + gh + 24, gh, 'p') +
      `<text x="${pad + 4}" y="${top + 12}" fill="#c9d2e2" font-size="10">AM/AM · output vs input</text>` +
      `<text x="${pad + 4}" y="${top + gh + 36}" fill="#c9d2e2" font-size="10">AM/PM · phase vs input</text>` +
      legend(pad + gw - 190, top + gh - 8, [['#3fa6ff', 'amplifier'], ['#f2335a', 'correction']]) +
      legend(pad + 8, top + 2 * gh + 16, [['#ffcc26', 'amplifier'], ['#34d94d', 'correction']]);
  } else {
    body = graph(top, h - top - 22, 'b') + legend(pad + 8, top + 16, [['#3fa6ff', 'Mag amp'], ['#f2335a', 'Mag corr'], ['#ffcc26', 'Phs amp'], ['#34d94d', 'Phs corr']]);
  }
  return `<svg width="${w}" height="${h}" style="display:block">${body}
    <text x="${pad + gw / 2 - 40}" y="${h - 4}" fill="#8b97ad" font-size="10">input magnitude</text></svg>`;
}

// a half-circle gauge for the feedback level (the calibration page)
function gauge(r, level) {
  const a = v => Math.PI * (1 - v / 256), P = v => [r + 8 + r * Math.cos(a(v)), r + 8 - r * Math.sin(a(v))];
  const arc = (v0, v1, c, wd = 14) => { const [x0, y0] = P(v0), [x1, y1] = P(v1); return `<path d="M${x0},${y0} A${r},${r} 0 0 1 ${x1},${y1}" fill="none" stroke="${c}" stroke-width="${wd}"/>`; };
  const [nx, ny] = P(level);
  return `<svg width="${2 * r + 16}" height="${r + 24}" style="display:block">
    ${arc(0, 90, `rgba(${RED},.55)`)}${arc(90, 128, `rgba(${YEL},.55)`)}${arc(128, 181, `rgba(${GRN},.7)`)}${arc(181, 256, `rgba(${BLU},.55)`)}
    ${arc(140, 165, 'rgba(255,255,255,.9)', 3)}
    <line x1="${r + 8}" y1="${r + 8}" x2="${nx}" y2="${ny}" stroke="#fff" stroke-width="3" stroke-linecap="round"/>
    <circle cx="${r + 8}" cy="${r + 8}" r="6" fill="#fff"/></svg>`;
}

// the TX meter card with a PureSignal row (B)
const psCard = () => abs(10, 10, 236, null, H.txMeter(236) + `<div style="margin-top:8px;padding-top:7px;border-top:1px solid rgba(255,255,255,.1)">
  <div class="row" style="justify-content:space-between;margin-bottom:4px"><span class="lbl">PURESIGNAL</span>${dot(GRN, 'Correcting')}</div>
  ${fbBar(214, 152, 9)}<div class="row dim" style="justify-content:space-between;font-size:9.5px;margin-top:-4px"><span>feedback 152</span><span>TX ATT 8 dB</span><span>cal. 47</span></div></div>`,
  'glass', `padding:9px 11px;border-color:rgba(${RED},.5)`);

Object.assign(LAYOUTS, {
  // S1 — A: a PureSignal drawer from the right edge, like the TX audio drawer (M7 pick A)
  S1: () => ({
    html: cv('c', 0, 0, 832, 384) + H.meterCard() + H.pill() + H.chipRow('rx', CH) +
      abs(0, 0, 512, 384, '', '', 'background:linear-gradient(90deg,rgba(0,0,0,0),rgba(0,0,0,.4))') +
      abs(492, 0, 340, 384, `<div style="padding:11px 14px">
        <div class="row" style="justify-content:space-between"><span style="font-size:17px;font-weight:500">PureSignal</span>${dot(GRN, 'Correcting')}</div>
        <div class="row" style="gap:6px;margin-top:8px">${togS('PureSignal', true)}${btn('OFF', 'width:56px')}${btn('Restart', 'width:70px')}</div>
        ${lbl('FEEDBACK LEVEL · 152', '9px 0 4px')}${fbBar(310, 152)}
        <div class="row dim" style="gap:12px;font-size:10px;margin-top:-2px"><span>state COLLECT</span><span>cal. 47</span><span>GetPk 0.239</span></div>
        ${lbl('CALIBRATE · keys the radio until switched off', '9px 0 4px')}
        <div class="row" style="gap:6px">${btn('Two Tone', 'flex:1', `border-color:rgba(${RED},.6)`)}${btn('Noise', 'flex:1', `border-color:rgba(${RED},.6)`)}${togS('Auto att.', true)}</div>
        ${sl('TX ATT · 20 m', '8 dB', 62)}
        <div class="row" style="gap:6px;margin-top:8px">${togS('MON', false, 'feedback')}${togS('OneShot', false)}</div>
        <div class="row" style="gap:6px;margin-top:7px">${btn('AmpView ›')}${btn('Settings ›')}</div>
        </div>`, 'glass', 'border-radius:18px 0 0 18px;background:rgba(9,12,22,.92)') +
      note(20, 112, 330, '<b style="color:#ffd27a">A: PureSignal drawer</b><br><span class="dim">from a new PS chip (shows the feedback level and ✓ while correcting) or ☰ → PureSignal. Everything of deskHPSDR\'s PS menu on one panel; AmpView and the rarely-touched settings behind the › buttons.</span>'),
    canvases: [['#c', H.rxSpec()]],
  }),

  // S2 — B: no drawer to watch: PureSignal lives in the TX meter card while transmitting
  S2: () => ({
    html: cv('c', 0, 0, 832, 384) + psCard() + H.pill(true) + H.tBtns() +
      H.chipRow('tx', CH, '0:24') +
      note(470, 92, 340, '<b style="color:#ffd27a">B: PS status in the TX meter card</b><br><span class="dim">while you transmit the card (pick A from step 1) gets a PureSignal row: feedback level on deskHPSDR\'s colours, Correcting, TX ATT. The PS chip opens a short page (Enable, calibrate, AmpView) — the card is where you watch it. MON here: the orange TX spectrum shows the feedback (what the XPA125B really sends).</span>'),
    canvases: [['#c', H.txSpec(0.5)]],
  }),

  // S3 — C: a calibration page that walks through it: big gauge, big STOP
  S3: () => ({
    html: cv('c', 0, 0, 832, 384) + dim() + abs(66, 14, 700, 356, `<div style="padding:12px 18px">
      <div class="row" style="justify-content:space-between"><span style="font-size:17px;font-weight:500">PureSignal · calibrate</span><span class="dim" style="font-size:10px">20 m · 14.145 MHz · drive 40 %</span></div>
      <div class="row" style="gap:22px;margin-top:10px;align-items:flex-start">
        <div class="col" style="align-items:center;width:280px">${gauge(120, 152)}
          <span style="font-size:30px;font-weight:600;margin-top:-6px">152</span><span class="dim" style="font-size:11px">feedback level · aim 140-165</span>
          <div class="row" style="gap:14px;margin-top:8px">${dot(GRN, 'Correcting')}<span class="dim" style="font-size:11px">cal. 47</span></div></div>
        <div style="flex:1">
          ${['Two Tone on: the radio transmits 700 + 1900 Hz', 'Auto attenuate: TX ATT 0 → 14 → 8 dB', 'Feedback in range, PS correcting'].map((t, i) => `<div class="row" style="gap:10px;margin:7px 0"><span style="width:24px;height:24px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:12px;background:rgba(${GRN},.25);border:1px solid rgba(${GRN},.8)">✓</span><span style="font-size:12.5px">${t}</span></div>`).join('')}
          <div class="row" style="gap:10px;margin:7px 0"><span style="width:24px;height:24px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:12px;border:1px solid rgba(255,255,255,.4)">4</span><span style="font-size:12.5px" class="dim">Stop: PS keeps the correction for your voice</span></div>
          <div class="row" style="gap:8px;margin-top:10px">${btn('Two Tone', 'width:96px', 'background:rgba(63,208,255,.18);border-color:rgba(63,208,255,.7)')}${btn('Noise', 'width:70px')}${togS('Auto att.', true)}</div>
          ${sl('TX ATT', '8 dB', 62, 340)}
          <div class="btn" style="margin-top:12px;height:46px;font-size:16px;font-weight:600;background:rgba(${RED},.9);color:#fff;border-color:rgba(255,200,200,.8);box-shadow:0 0 18px rgba(${RED},.55)">STOP · on air 0:07</div>
        </div></div></div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.96)') +
      note(560, 6, 250, '<b style="color:#ffd27a">C: a calibration page</b> <span class="dim">· the steps deskHPSDR\'s timer goes through, shown; one big STOP</span>'),
    canvases: [['#c', H.txSpec(0.5)]],
  }),

  // S4 — AmpView as in deskHPSDR / Thetis: one graph, magnitude and phase together
  S4: () => ({
    html: cv('c', 0, 0, 832, 384) + dim() + abs(56, 12, 720, 360, `<div style="padding:10px 14px">
      <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">AmpView</span>
        <div class="row" style="gap:14px">${dot(GRN, 'Correcting')}<span style="font-size:11.5px">feedback <b style="color:rgb(${GRN})">152</b></span><span class="dim" style="font-size:11px">GetPk 0.239</span></div></div>
      ${ampSvg(692, 300)}
      <div class="dim" style="font-size:9.5px;margin-top:2px">4096 samples · 512 correction points · 10 times a second, as deskHPSDR</div></div>`, 'glass', 'border-radius:18px;background:rgba(6,8,14,.97)'),
    canvases: [['#c', H.rxSpec()]],
  }),

  // S5 — AmpView split: magnitude above, phase below, each with its own scale; status beside
  S5: () => ({
    html: cv('c', 0, 0, 832, 384) + dim() + abs(36, 12, 760, 360, `<div class="row" style="padding:10px 14px;gap:14px;align-items:flex-start">
      <div>${ampSvg(560, 338, true)}</div>
      <div class="col" style="flex:1;gap:6px">
        <span style="font-size:16px;font-weight:500">AmpView</span>${dot(GRN, 'Correcting')}
        ${lbl('FEEDBACK', '6px 0 2px')}${fbBar(150, 152, 10)}
        ${[['state', 'COLLECT'], ['cal. count', '47'], ['sln. check', '0'], ['dg. count', '0'], ['GetPk', '0.239'], ['SetPk', '0.240'], ['TX ATT', '8 dB']].map(([a, b]) => `<div class="row" style="justify-content:space-between;font-size:11px"><span class="dim">${a}</span><span>${b}</span></div>`).join('')}
        <div class="btn" style="height:30px;font-size:11.5px;margin-top:4px">Restart</div></div></div>`, 'glass', 'border-radius:18px;background:rgba(6,8,14,.97)'),
    canvases: [['#c', H.rxSpec()]],
  }),

  // S6 — the settings behind "Settings ›", and the TX ATT 0 warning
  S6: () => ({
    html: cv('c', 0, 0, 832, 384) + H.panel('PureSignal settings', 'deskHPSDR\'s PS menu, the rarely-changed part', `
      <div class="row" style="gap:16px;margin-top:8px;align-items:flex-start">
        <div style="flex:1">${lbl('CALIBRATION')}<div class="row" style="gap:6px">${togS('OneShot', false, 'calibrate once, hold')}</div>
          ${lbl('STABILITY', '10px 0 4px')}${seg(['Strict', 'Medium', 'Relaxed'], 'Relaxed', 30)}
          <div class="dim" style="font-size:9.5px;margin-top:3px">Relaxed suits a hard-driven amplifier (deskHPSDR default)</div>
          ${sl('NOISE LEVEL', '−3 dB', 60, 290)}</div>
        <div style="flex:1">${sl('TX DELAY', '150 ns', 51, 290, '4px 0 0')}
          ${lbl('SETPK · the HL2\'s peak', '10px 0 4px')}<div class="row" style="gap:6px">${btn('0.240', 'width:90px')}<span class="dim" style="font-size:10px">GetPk now 0.239 · HL2 default 0.240</span></div>
          ${lbl('SAMPLE RATE WITH PS · proposed, not in deskHPSDR', '10px 0 4px')}${seg(['384 kHz', '192 kHz'], '192 kHz', 30)}
          <div class="dim" style="font-size:9.5px;margin-top:3px">PS adds 2 streams (4 in all); 192 kHz is the usual advice</div></div>
      </div>`, 96, 640) +
      abs(250, 230, 330, 120, `<div style="padding:10px 14px"><div style="font-size:13px;font-weight:600;color:rgb(${RED})">TX ATT is 0 dB</div>
        <div class="dim" style="font-size:10.5px;margin-top:4px;line-height:1.35">Check the feedback level or calibrate again: too much feedback overloads the HL2's ADC.</div>
        <div class="row" style="gap:6px;margin-top:8px">${togS('Don\'t show again', false)}${btn('OK', 'width:60px')}</div></div>`, 'glass', `border-radius:14px;background:rgba(20,10,14,.97);border-color:rgba(${RED},.6)`),
    canvases: [['#c', H.rxSpec()]],
  }),
});
})();
