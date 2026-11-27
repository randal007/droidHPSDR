// M7 step 3 (CW), round 1, 2026-11-27: deskHPSDR's CW menu (cw_menu.c) on the HL2+'s own
// keyer (paddle in the KEY jack, Randal's answer 7): speed, paddle mode, weight, letter
// spacing, keys reversed, hang time, PTT delay, sidetone level and pitch, zero beat.
// Three places for it (C1-C3), the keying view (C4), the locked paddle (C5), settings (C6).
// Pictures only. See docs/M7-INVENTORY.md "Step 3: CW".
'use strict';
(() => {
const H = window.M7H;
const { lbl, seg, sl, note, dim, RED } = H;
const GRN = '62,224,122', CYA = '63,208,255', AMB = '255,179,65';
const V = 14.0231e6, FL = [-250, 250];
const sh = 'text-shadow:0 1px 3px #000,0 0 8px rgba(0,0,0,.9)';
const CH = [['BAND', '20m'], ['AF', '73'], ['NR', 'NR2', true], ['FILTER', '500'], ['PS', 'Off'], ['TX', '40 %']];
const CHW = [['BAND', '20m'], ['AF', '73'], ['FILTER', '500'], ['CW', '18 wpm', true], ['TX', '40 %']];
const btn = (t, w = 'flex:1', extra = '') => `<div class="btn" style="${w};height:30px;font-size:11.5px;${extra}">${t}</div>`;
const tog = (t, on, sub = '') => `<div class="tog ${on ? 'on' : ''}" style="flex:1;height:32px"><span style="font-size:12px">${t}${sub ? `<span class="dim" style="font-size:9.5px;margin-left:6px">${sub}</span>` : ''}</span><s></s></div>`;
const dot = (c, t) => `<span class="row" style="gap:5px;font-size:11.5px"><i style="width:9px;height:9px;border-radius:50%;background:rgb(${c});box-shadow:0 0 8px rgb(${c})"></i>${t}</span>`;
const lamp = (t, on, c = GRN) => `<span style="padding:3px 8px;border-radius:6px;font-size:10.5px;font-weight:600;letter-spacing:.5px;${on ? `background:rgba(${c},.9);color:#04100a;box-shadow:0 0 10px rgba(${c},.8)` : 'background:rgba(255,255,255,.07);color:#8b97ad'}">${t}</span>`;
// a big −/value/+ stepper (thumb-sized)
const step = (val, unit, h = 40, fs = 22) => `<div class="row" style="gap:6px;align-items:center">
  <div class="btn" style="width:${h + 6}px;height:${h}px;font-size:20px">−</div>
  <div class="col" style="flex:1;align-items:center;line-height:1"><span style="font-size:${fs}px;font-weight:600">${val}</span><span class="dim" style="font-size:9.5px;margin-top:2px">${unit}</span></div>
  <div class="btn" style="width:${h + 6}px;height:${h}px;font-size:20px">+</div></div>`;

const pill = (tx = false) => abs(266, 8, 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%">
  ${tx ? `<span class="tag" style="background:rgb(${RED});color:#fff">TX</span>` : '<span class="tag vfoA">A</span>'}${fq(V, 34)}
  <div class="col" style="gap:3px"><span class="tag acc">CWU</span><span class="tag">500</span></div></div>`, 'glass',
  tx ? `border-color:rgba(${RED},.85);box-shadow:0 0 20px rgba(${RED},.45)` : '');
const meterCard = () => abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S8</span><span class="dim" style="font-size:11px">−71 dBm</span></div>${meter(146, 9, 8.1, true)}`, 'glass', 'padding:9px 11px');
const chips = (list, pttState = 'rx', left) => abs(10, 328, 812, 46, `<div class="row" style="gap:7px">${list.map(([l, v, on]) => chip(l, v, on)).join('')}<div style="flex:1"></div>${pttChip(pttState, left)}</div>`);
// the PTT chip in CW: question 2 decides what it does; shown here as deskHPSDR's (holds TX)
function pttChip(state, left = '0:04') {
  if (state === 'key') return `<div class="chip" style="min-width:132px;height:46px;background:rgba(${RED},.92);color:#fff;box-shadow:0 0 22px rgba(${RED},.7);border:1px solid rgba(255,200,200,.8)">PADDLE<b style="color:#fff;font-size:15px">${left}</b></div>`;
  if (state === 'locked') return `<div class="chip glass" style="min-width:132px;opacity:.55">PTT<b class="row" style="gap:5px;font-size:13px">${icon('lock', 13, 2)} TX off</b></div>`;
  return `<div class="chip glass" style="min-width:132px;border:1.5px solid rgba(${RED},.8);color:#ff9d9d">HOLD<b style="color:#ff6b6b;font-size:17px;font-weight:600;letter-spacing:1px">PTT</b></div>`;
}

const rxSpec = () => ({ ...H.rxSpec(), f0: 14.005e6, f1: 14.045e6, vfo: V, filt: FL });
// our own CW carrier on the TX spectrum: one narrow line with keying sidebands
const txSpec = () => ({
  ...rxSpec(), custom: (cvs, sp) => {
    const split = 0.5;
    sp.parts[0].h = split; sp.parts[1].h = 1 - split;
    R.paint(cvs, sp);
    const ctx = cvs.getContext('2d'), W = cvs.clientWidth, Hh = cvs.clientHeight * split - 8;
    const g0 = ctx.createLinearGradient(0, 0, 0, Hh); g0.addColorStop(0, '#0b0405'); g0.addColorStop(1, '#14080a');
    ctx.fillStyle = g0; ctx.fillRect(0, 0, W, Hh);
    const lo = -120, hi = 20, X = f => (f - sp.f0) / (sp.f1 - sp.f0) * W, Y = d => Hh - Math.max(0, Math.min(1, (d - lo) / (hi - lo))) * Hh;
    ctx.strokeStyle = 'rgba(255,255,255,0.06)'; ctx.fillStyle = 'rgba(255,255,255,0.35)'; ctx.font = '9px Roboto';
    for (let d = -100; d <= 0; d += 20) { ctx.beginPath(); ctx.moveTo(0, Y(d)); ctx.lineTo(W, Y(d)); ctx.stroke(); ctx.fillText(d, 4, Y(d) - 2); }
    let seed = 3; const r = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
    ctx.beginPath(); ctx.moveTo(0, Hh);
    for (let j = 0; j <= W * 2; j++) {
      const f = sp.f0 + j / (W * 2) * (sp.f1 - sp.f0), a = Math.abs(f - V);
      const d = Math.max(-106 + r() * 5, -8 - a / 9, -62 - a / 60);
      ctx.lineTo(j / 2, Y(d));
    }
    ctx.lineTo(W, Hh); ctx.closePath();
    const g = ctx.createLinearGradient(0, 0, 0, Hh); g.addColorStop(0, 'rgba(255,140,60,0.6)'); g.addColorStop(1, 'rgba(255,90,60,0.03)');
    ctx.fillStyle = g; ctx.fill();
    ctx.strokeStyle = 'rgb(' + RED + ')'; ctx.lineWidth = 1.4; ctx.beginPath(); ctx.moveTo(X(V), 0); ctx.lineTo(X(V), Hh); ctx.stroke();
    ctx.fillStyle = 'rgba(0,0,0,0.5)'; ctx.fillRect(0, cvs.clientHeight * split + 22, W, cvs.clientHeight);
  },
});

// the CW settings shared by C1 and C6
const modeSeg = (h = 30) => seg(['Straight', 'Iambic A', 'Iambic B'], 'Iambic B', h);
const zeroBeat = (w = 'flex:1') => btn(`${icon('target', 14, 2)}&nbsp;Zero beat`, w, `border-color:rgba(${CYA},.6)`);

Object.assign(LAYOUTS, {
  // C1 — A: a CW drawer from the right edge, like PureSignal's
  C1: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + pill() + chips(CHW) +
      abs(0, 0, 512, 384, '', '', 'background:linear-gradient(90deg,rgba(0,0,0,0),rgba(0,0,0,.4))') +
      abs(492, 0, 340, 384, `<div style="padding:11px 14px">
        <div class="row" style="justify-content:space-between"><span style="font-size:17px;font-weight:500">CW keyer</span>${dot(GRN, 'Paddle ready')}</div>
        ${lbl('SPEED', '8px 0 4px')}${step('18', 'WPM')}
        ${lbl('PADDLE', '9px 0 4px')}${modeSeg()}
        ${sl('PITCH · sidetone + receive', '600 Hz', 55, 312)}
        ${sl('SIDETONE LEVEL', '50', 39, 312)}
        ${sl('HANG TIME · back to receive after', '500 ms', 50, 312)}
        <div class="row" style="gap:6px;margin-top:9px">${zeroBeat()}${btn('Settings ›')}</div>
        </div>`, 'glass', 'border-radius:18px 0 0 18px;background:rgba(9,12,22,.92)') +
      note(20, 112, 330, '<b style="color:#ffd27a">A: CW drawer</b><br><span class="dim">from a CW chip (shows the speed; there only in CWU / CWL) or ☰ → CW. The things you touch on the air: speed, paddle mode, pitch, sidetone, hang time, zero beat. Weight, spacing, keys reversed, PTT delay behind Settings ›.</span>'),
    canvases: [['#c', rxSpec()]],
  }),

  // C2 — B: the CW chip opens a small pop-up over itself; everything else on a page
  C2: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + pill() + chips(CHW) +
      abs(300, 150, 250, 172, `<div style="padding:10px 12px">
        <div class="row" style="justify-content:space-between"><span class="lbl">CW · IAMBIC B</span>${dot(GRN, 'ready')}</div>
        <div style="margin-top:6px">${step('18', 'WPM', 44, 26)}</div>
        <div style="margin-top:6px">${step('600', 'Hz pitch', 34, 17)}</div>
        <div class="row" style="gap:6px;margin-top:8px">${zeroBeat()}${btn('All CW ›', 'width:80px')}</div></div>`, 'glass', 'border-radius:16px;background:rgba(9,12,22,.95);border-color:rgba(255,255,255,.25)') +
      abs(408, 320, 16, 10, '', '', 'background:rgba(9,12,22,.95);clip-path:polygon(0 0,100% 0,50% 100%)') +
      note(20, 112, 260, '<b style="color:#ffd27a">B: chip + pop-up</b><br><span class="dim">tap the CW chip: speed and pitch with big −/+, zero beat. Swipe the chip left/right to change the speed without opening anything. "All CW ›" opens the page (C6). The spectrum stays in view.</span>'),
    canvases: [['#c', rxSpec()]],
  }),

  // C3 — C: in a CW mode a CW strip sits over the bottom of the spectrum, always there
  C3: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + pill() + chips(CH) +
      abs(10, 276, 812, 44, `<div class="row" style="gap:10px;height:100%;padding:0 10px">
        <span class="lbl" style="margin:0">CW</span>
        <div class="row" style="gap:4px"><div class="btn" style="width:34px;height:32px;font-size:18px">−</div><span style="font-size:18px;font-weight:600;width:62px;text-align:center">18<span class="dim" style="font-size:10px"> wpm</span></span><div class="btn" style="width:34px;height:32px;font-size:18px">+</div></div>
        <span class="row" style="gap:4px;font-size:12px"><span class="dim">pitch</span>600 Hz</span>
        <span class="tag">Iambic B</span>
        ${zeroBeat('width:104px')}
        <div style="flex:1"></div>
        ${lamp('DOT', false)}${lamp('DASH', false)}${lamp('PTT', false, RED)}
        <div class="ico glass" style="width:32px;height:32px">${icon('gear', 16)}</div></div>`, 'glass', 'border-radius:12px;background:rgba(9,12,22,.85)') +
      note(20, 112, 330, '<b style="color:#ffd27a">C: a CW strip</b><br><span class="dim">only in CWU / CWL, over the bottom of the spectrum: speed −/+, pitch, mode, zero beat, and the paddle lights the radio reports (dot, dash, PTT). ⚙ opens the page (C6). No chip, no drawer to open.</span>'),
    canvases: [['#c', rxSpec()]],
  }),

  // C4 — sending: the paddle keys the HL2+, the app follows its PTT
  C4: () => ({
    html: cv('c', 0, 0, 832, 384) + abs(10, 10, 236, null, H.txMeter(236).replace('ALC', 'ALC').replace(/<div style="margin-top:6px"><div class="row" style="justify-content:space-between;margin-bottom:3px"><span class="lbl">MIC<\/span>[\s\S]*$/, '') +
      `<div style="margin-top:8px;padding-top:7px;border-top:1px solid rgba(255,255,255,.1)">
        <div class="row" style="justify-content:space-between"><span class="lbl">CW · 18 WPM · IAMBIC B</span>${lamp('KEY', true, RED)}</div>
        <div class="row" style="gap:6px;margin-top:6px;align-items:center"><span class="dim" style="font-size:9.5px;width:34px">hang</span>
          <div style="flex:1;height:6px;border-radius:3px;background:rgba(255,255,255,.08)"><div style="width:100%;height:6px;border-radius:3px;background:rgba(${AMB},.85)"></div></div><span style="font-size:10px">500 ms</span></div></div>`,
      'glass', `padding:9px 11px;border-color:rgba(${RED},.5)`) +
      pill(true) + H.tBtns() + chips(CHW, 'key', '0:04') + H.frozenNote() +
      note(470, 92, 340, '<b style="color:#ffd27a">Sending</b><br><span class="dim">the HL2+ keys itself from the paddle; the app sees the radio\'s PTT and switches over: TX meter (PWR, SWR, ALC), KEY lamp while the key is down, the hang bar running out after the last element, then back to receive. The PTT chip shows PADDLE and the time on air (the 2 min timeout counts here too). Sidetone from the HL2+ speaker / phones.</span>'),
    canvases: [['#c', txSpec()]],
  }),

  // C5 — the paddle locked: CW with Transmit enabled off, and after the timeout
  C5: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + pill() + chips([['BAND', '20m'], ['AF', '73'], ['FILTER', '500'], ['CW', `${'🔒'} 18 wpm`], ['TX', '40 %']], 'locked') +
      abs(220, 96, 392, 64, `<div class="row" style="gap:10px;padding:9px 14px">${icon('lock', 22, 2)}<div><div style="font-size:13px;font-weight:600">Paddle off: Transmit enabled is off</div>
        <div class="dim" style="font-size:10.5px;margin-top:2px">the radio's keyer isn't armed, so the paddle can't key the HL2 or the XPA125B</div></div></div>`, 'glass', `border-radius:14px;background:rgba(20,14,10,.95);border-color:rgba(${AMB},.6)`) +
      abs(220, 170, 392, 64, `<div class="row" style="gap:10px;padding:9px 14px"><span style="font-size:20px">⏱</span><div><div style="font-size:13px;font-weight:600;color:rgb(${RED})">TX timeout (2 min): keyer off</div>
        <div class="dim" style="font-size:10.5px;margin-top:2px">let go of the paddle; it works again from the next key-down</div></div></div>`, 'glass', `border-radius:14px;background:rgba(20,10,14,.97);border-color:rgba(${RED},.6)`) +
      note(20, 246, 300, '<b style="color:#ffd27a">Locked paddle</b> <span class="dim">· two messages (one at a time on the radio): the same reasons as PTT (Transmit enabled, demo band, inhibit, out of band) keep the keyer bit off. The CW chip carries a lock.</span>'),
    canvases: [['#c', rxSpec()]],
  }),

  // C6 — the settings page: all of deskHPSDR's CW menu, plus PTT delay
  C6: () => ({
    html: cv('c', 0, 0, 832, 384) + H.panel('CW settings', 'deskHPSDR\'s CW menu · the HL2+ keyer', `
      <div class="row" style="gap:18px;margin-top:6px;align-items:flex-start">
        <div style="flex:1">${lbl('KEYER')}${modeSeg(28)}
          ${sl('SPEED', '18 WPM', 29, 290)}
          ${sl('WEIGHT', '50', 50, 290)}
          <div class="row" style="gap:6px;margin-top:8px">${tog('Letter spacing', false)}${tog('Keys reversed', false)}</div>
          ${lbl('SIDETONE', '10px 0 2px')}
          ${sl('PITCH · also the receive offset', '600 Hz', 55, 290, '2px 0 0')}
          ${sl('LEVEL', '50', 39, 290)}</div>
        <div style="flex:1">${lbl('TIMING · with the XPA125B')}
          ${sl('PTT DELAY · RF after the amp\'s PTT', '30 ms', 12, 290, '2px 0 0')}
          <div class="dim" style="font-size:9.5px;margin-top:3px">capped at 900 / speed: 50 ms at 18 WPM (deskHPSDR)</div>
          ${sl('HANG TIME · break-in delay', '500 ms', 50, 290)}
          ${lbl('ZERO BEAT', '10px 0 2px')}
          ${sl('CORRECTION', '+15 Hz', 65, 290, '2px 0 0')}
          <div class="row" style="gap:8px;margin-top:8px;align-items:center">${zeroBeat('width:120px')}<span class="dim" style="font-size:10.5px">last: moved +42 Hz</span></div></div>
      </div>`, 96, 640),
    canvases: [['#c', rxSpec()]],
  }),
});
})();
