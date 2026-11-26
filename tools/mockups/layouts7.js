// M5 round 3: the info bar (Randal picked E4's strip, 2026-11-25) without
// making the spectrum smaller or covering it. Three ways.
'use strict';
(() => {
const F0 = 14.044e6, F1 = 14.104e6;
const sh = 'text-shadow:0 1px 3px #000,0 0 8px rgba(0,0,0,.9)';
const meterCard = y => abs(10, y, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px');
const tBtns = y => abs(720, y, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass" style="font-weight:600;font-size:13px">2D</div><div class="ico glass">${icon('menu')}</div></div>`);
const vPill = y => abs(266, y, 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%"><span class="tag vfoA">A</span>${fq(VFO, 34)}<div class="col" style="gap:3px"><span class="tag acc">USB</span><span class="tag">2.8k</span></div></div>`, 'glass');
const CH = [['NR', 'NR2', true], ['BAND', '20m'], ['AF', '73'], ['LNA', '+20 dB'], ['PHONE', 'Off'], ['FILTER', '2.8k'], ['STEP', '10 Hz'], ['SPAN', '60 kHz']];
const chipRow = (y = 328) => abs(10, y, 812, 46, `<div class="row" style="gap:7px">${CH.map(([l, v, on]) => chip(l, v, on)).join('')}</div>`);
const bg = () => ({
  f0: F0, f1: F1, vfo: VFO, filt: [100, 2900], want: 7, bandplan: true,
  parts: [{ kind: 'spec2d', h: 0.5, dbl: true, line: '#cfe9ff', fillTop: 'rgba(80,140,255,0.45)' }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }],
});
const items = (adc = 'ok') => `<span>UTC 18:42</span><span class="dim">SFI 142 · A 6 · K 2</span>${adc === 'ok'
  ? '<span style="color:var(--grn)">ADC OK</span>'
  : '<span style="color:#fff;background:rgba(220,40,40,.9);border-radius:8px;padding:0 7px;font-weight:600">ADC OVERLOAD</span>'}<span class="dim">HL2+ 192.168.88.30</span>`;
const note = (x, y, w, t) => `<div class="tip" style="left:${x}px;top:${y}px;width:${w}px;white-space:normal;line-height:1.35">${t}</div>`;

Object.assign(LAYOUTS, {
  // A — E4's strip, but see-through and laid over the top edge of the spectrum
  // (that top ~10 dB is almost always empty). The spectrum keeps its full
  // height; only the floating cards move down a little.
  E7: () => ({
    html: cv('c', 0, 0, 832, 384) +
      abs(0, 0, 832, 18, `<div class="row" style="gap:16px;justify-content:center;height:100%;font-size:10.5px;${sh}">${items()}</div>`) +
      meterCard(22) + tBtns(22) + vPill(20) + chipRow() +
      note(540, 96, 230, '<b>A: see-through strip on top</b><br><span class="dim">no background: the spectrum shows through. Spectrum the same size; the meter, VFO and buttons sit 12 dp lower.</span>'),
    canvases: [['#c', bg()]],
  }),

  // B — no strip at all: the same items fill the empty gaps between the
  // floating cards that are already there. Nothing moves.
  E8: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard(10) + tBtns(12) + vPill(8) + chipRow() +
      abs(180, 14, 84, 54, `<div class="col" style="gap:2px;align-items:center;font-size:10.5px;${sh}"><span style="font-size:15px;font-weight:500">18:42</span><span class="dim">UTC</span></div>`) +
      abs(570, 14, 148, 54, `<div class="col" style="gap:3px;align-items:center;font-size:10.5px;${sh}"><span class="dim">SFI 142 · A 6 · K 2</span><span style="color:var(--grn)">ADC OK</span><span class="dim" style="font-size:9.5px">HL2+ 192.168.88.30</span></div>`) +
      note(540, 110, 230, '<b>B: in the gaps</b><br><span class="dim">clock left of the VFO, solar / ADC / radio right of it. No new row, nothing moves.</span>'),
    canvases: [['#c', bg()]],
  }),

  // C — the strip along the bottom edge, under the bottom row, over the
  // oldest waterfall rows (the least useful part of the screen).
  E9: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard(10) + tBtns(12) + vPill(8) + chipRow(318) +
      abs(0, 368, 832, 16, `<div class="row" style="gap:16px;justify-content:center;height:100%;font-size:10.5px;${sh}">${items('ovl')}</div>`) +
      note(540, 96, 230, '<b>C: see-through strip at the bottom</b><br><span class="dim">over the oldest waterfall rows; the bottom row moves up 10 dp. Shown here with the ADC alarm on.</span>'),
    canvases: [['#c', bg()]],
  }),
});
})();
