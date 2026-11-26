// M6 step 2 (HL2 settings, Randal picked way B: an HL2 section in SDR Device,
// setup behind its own warning page) and the two E2 fixes (the dB scale is
// too narrow to pinch). See docs/M6-INVENTORY.md.
'use strict';
(() => {
const F0 = 14.044e6, F1 = 14.104e6;
const mCard = () => abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px');
const tBtns = () => abs(720, 12, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass" style="font-weight:600;font-size:13px">2D</div><div class="ico glass">${icon('menu')}</div></div>`);
const vPill = () => abs(266, 8, 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%"><span class="tag vfoA">A</span>${fq(VFO, 34)}<div class="col" style="gap:3px"><span class="tag acc">USB</span><span class="tag">2.8k</span></div></div>`, 'glass');
const CH = [['NR', 'NR2', true], ['BAND', '20m'], ['AF', '73'], ['LNA', '+20 dB'], ['PHONE', 'Off'], ['FILTER', '2.8k'], ['STEP', '10 Hz'], ['SPAN', '60 kHz']];
const chipRow = () => abs(10, 328, 812, 46, `<div class="row" style="gap:7px">${CH.map(([l, v, on]) => chip(l, v, on)).join('')}</div>`);
const bg = () => ({
  f0: F0, f1: F1, vfo: VFO, filt: [100, 2900], want: 7, bandplan: true,
  parts: [{ kind: 'spec2d', h: 0.5, dbl: true, line: '#cfe9ff', fillTop: 'rgba(80,140,255,0.45)' }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }],
});
const seg = (items, on, h = 30) => `<div class="seg" style="height:${h}px;align-items:center">${items.map(t => `<b class="${t === on ? 'on' : ''}">${t}</b>`).join('')}</div>`;
const tog = (t, on, sub = '', off = false) => `<div class="tog ${on ? 'on' : ''}" style="flex:1;${off ? 'opacity:.4' : ''}"><span>${t}${sub ? `<span class="dim" style="font-size:9.5px;margin-left:6px">${sub}</span>` : ''}</span><s></s></div>`;
const sl = (l, v, p, w = 268) => `<div style="margin-top:6px">${slider(l, v, p, w)}</div>`;
const lbl = (t, m = '9px 0 4px') => `<div class="lbl" style="margin:${m}">${t}</div>`;
const dim = () => abs(0, 0, 832, 384, '', '', 'background:rgba(0,0,0,.42)');
const panel = (title, sub, body) => dim() + abs(116, 14, 600, 360, `<div style="padding:12px 16px">
  <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">${title}</span><span class="dim" style="font-size:10px">${sub}</span></div>${body}</div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.96)');
const btn = (t, st = '') => `<div class="btn" style="height:32px;display:flex;align-items:center;justify-content:center;${st}">${t}</div>`;
const sh = 'text-shadow:0 1px 3px #000';
// dB labels from hi to lo down the strip x..x+w, y 66..188
const labels = (x, w, hi, lo, col = '#ffd27a') => {
  const n = Math.round((hi - lo) / 10), out = [];
  for (let i = 0; i <= n; i++) {
    const y = 66 + i * (122 / n);
    out.push(abs(x, y - 7, w - 6, 14, `<div style="text-align:right;font-size:10px;color:${col};${sh}">${hi - i * 10}</div>`));
  }
  return out.join('');
};

Object.assign(LAYOUTS, {
  // B: SDR Device with an HL2 section; the risky part behind its own page
  F1: () => ({
    html: cv('c', 0, 0, 832, 384) + panel('SDR Device', "deskHPSDR's Radio menu, the HL2's lines", `
      <div class="row" style="gap:16px;margin-top:6px;align-items:flex-start">
        <div style="flex:1">${lbl('Band plan region')}${seg(['Region 1', 'Region 2', 'Region 3'], 'Region 2')}
          ${lbl('HL2 · IO board', '12px 0 4px')}
          <div class="row" style="gap:8px;align-items:center;padding:7px 10px;border-radius:11px;background:rgba(62,224,122,.08);border:1px solid rgba(62,224,122,.3)">
            <span style="width:8px;height:8px;border-radius:4px;background:var(--grn)"></span>
            <span style="font-size:12px">Pico found <span class="dim">(custom XPA125B firmware)</span></span><span class="dim" style="margin-left:auto;font-size:10.5px">LPF 20m</span></div>
          <div class="row" style="gap:6px;margin-top:6px">${tog('Force IO board', false, 'if it isn\'t found')}</div>
          ${lbl('HL2+ audio codec', '10px 0 4px')}${seg(['HL2+ (AK4951)', 'None'], 'HL2+ (AK4951)')}</div>
        <div style="flex:1">${lbl('Filter board')}${seg(['RX + TX + HPF', 'TX only'], 'RX + TX + HPF')}
          <div class="row" style="gap:6px;margin-top:6px">${tog('3 MHz HPF on receive', true, 'TX only mode', true)}</div>
          ${lbl('Calibration', '10px 0 2px')}${sl('FREQUENCY', '+0.00 ppm', 50, 268)}${sl('ADC GAIN (S-meter)', '+0 dB', 50, 268)}
          <div style="margin-top:10px">${btn('HL2 setup: reboot, gateware, network&nbsp;&nbsp;›', 'border-color:rgba(255,179,65,.5);color:var(--amb)')}</div>
          <div class="dim" style="font-size:9.5px;margin-top:4px">Ethernet only · behind a warning (next picture)</div></div>
      </div>`),
    canvases: [['#c', bg()]],
  }),

  // the HL2 setup page: Ethernet only, warnings first
  F2: () => ({
    html: cv('c', 0, 0, 832, 384) + panel('HL2 setup', 'deskHPSDR\'s P1 Setup (p2_programmer.c)', `
      <div style="margin-top:8px;padding:8px 11px;border-radius:11px;background:rgba(255,77,77,.12);border:1px solid rgba(255,77,77,.45);font-size:11px;line-height:1.4">
        <b style="color:#ff9a9a">These change the HL2 itself.</b> Only over the USB Ethernet link, never WiFi.
        Don't unplug or power off during a gateware update: a failed flash needs a JTAG recovery.</div>
      <div class="row" style="gap:16px;margin-top:6px;align-items:flex-start">
        <div style="flex:1">${lbl('Connected')}<div style="font-size:12px"><span style="color:var(--grn)">● Ethernet</span> <span class="dim">169.254.19.221 · gateware 75.0</span></div>
          ${lbl('Reboot', '10px 0 4px')}${btn('Reboot HL2, then find it again')}
          ${lbl('Gateware', '10px 0 4px')}${btn('Choose .rbf file…')}
          <div class="dim" style="font-size:9.5px;margin-top:4px">asks again before erasing: "Erase and flash 75.1?"</div></div>
        <div style="flex:1">${lbl('Network (EEPROM, after a reboot)')}<div class="row" style="gap:6px">${tog('Prefer DHCP', true)}</div>
          ${lbl('Alternate MAC (last two bytes)', '10px 0 4px')}<div class="row" style="gap:6px"><div class="btn" style="flex:1;height:32px;display:flex;align-items:center;padding:0 10px">13 : DD</div>${btn('Save', 'width:70px')}</div>
          ${lbl('Static IP', '10px 0 4px')}<div class="dim" style="font-size:10.5px;line-height:1.35">Not offered: it switches DHCP off, so the HL2 can't be found on WiFi any more.</div></div>
      </div>`) +
      abs(470, 352, 240, 18, '<div class="dim" style="font-size:9.5px;text-align:right">on WiFi: everything greyed, "connect the USB adapter"</div>'),
    canvases: [['#c', bg()]],
  }),

  // E2 fix A: one finger, three zones on the thin scale
  F3: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill() + chipRow() +
      abs(782, 60, 50, 44, '', '', 'background:linear-gradient(270deg,rgba(255,210,122,.34),rgba(255,210,122,.04));border-left:1px solid rgba(255,210,122,.6)') +
      abs(782, 104, 50, 44, '', '', 'background:linear-gradient(270deg,rgba(255,210,122,.14),rgba(255,210,122,0));border-left:1px solid rgba(255,210,122,.35)') +
      abs(782, 148, 50, 44, '', '', 'background:linear-gradient(270deg,rgba(63,208,255,.2),rgba(63,208,255,0));border-left:1px solid rgba(63,208,255,.45)') +
      labels(782, 50, -40, -140) +
      abs(728, 74, 52, 14, `<div style="text-align:right;font-size:9.5px;font-weight:600;color:#ffd27a;${sh}">HIGH ↕</div>`) +
      abs(728, 118, 52, 14, `<div style="text-align:right;font-size:9.5px;font-weight:600;color:#ffd27a;${sh}">both ↕</div>`) +
      abs(728, 162, 52, 14, `<div style="text-align:right;font-size:9.5px;font-weight:600;color:#3fd0ff;${sh}">LOW ↕</div>`) +
      `<div class="finger" style="left:784px;top:58px"></div>` +
      `<div class="tip" style="left:430px;top:92px;width:290px;white-space:normal;line-height:1.4"><b style="color:#ffd27a">A: one finger, three zones</b><br><span class="dim">top third: moves HIGH only (the range grows or shrinks) · middle: moves both · bottom third: moves LOW only · double-tap: automatic. No pinch.</span></div>`,
    canvases: [['#c', bg()]],
  }),

  // E2 fix B: hold the scale and it widens to room for two fingers
  F4: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill() + chipRow() +
      abs(700, 60, 132, 132, '', '', 'background:linear-gradient(270deg,rgba(255,210,122,.3),rgba(255,210,122,.06));border-left:1px solid rgba(255,210,122,.6);border-radius:12px 0 0 12px') +
      labels(700, 132, -50, -130) +
      `<div class="finger" style="left:740px;top:62px"></div><div class="finger" style="left:752px;top:146px"></div>` +
      abs(726, 104, 40, 40, `<div style="text-align:center;font-size:22px;color:#ffd27a;${sh}">↕</div>`) +
      `<div class="tip" style="left:400px;top:92px;width:290px;white-space:normal;line-height:1.4"><b style="color:#ffd27a">B: hold it and it widens</b><br><span class="dim">touch the thin scale: it opens to 3× the width. Pinch: range · drag: move · let go: thin again · double-tap: automatic.</span></div>`,
    canvases: [['#c', bg()]],
  }),
});
})();
