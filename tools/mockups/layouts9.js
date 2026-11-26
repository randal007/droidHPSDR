// Levels panel (replaces E2, Randal 2026-11-25: the dB scale on the spectrum
// was too hard to use). A LEVELS chip opens a panel that leaves the spectrum
// and waterfall in view, so the change shows while you make it.
'use strict';
(() => {
const F0 = 14.044e6, F1 = 14.104e6;
const mCard = () => abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px');
const tBtns = () => abs(720, 12, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass" style="font-weight:600;font-size:13px">2D</div><div class="ico glass">${icon('menu')}</div></div>`);
const vPill = () => abs(266, 8, 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%"><span class="tag vfoA">A</span>${fq(VFO, 34)}<div class="col" style="gap:3px"><span class="tag acc">USB</span><span class="tag">2.8k</span></div></div>`, 'glass');
const CH = [['NR', 'NR2', true], ['BAND', '20m'], ['AF', '73'], ['LNA', '+20 dB'], ['LEVELS', 'Manual', true], ['FILTER', '2.8k'], ['STEP', '10 Hz'], ['SPAN', '60 kHz']];
const chipRow = (w = 812) => abs(10, 328, w, 46, `<div class="row" style="gap:7px">${CH.map(([l, v, on]) => chip(l, v, on)).join('')}</div>`);
const bg = () => ({
  f0: F0, f1: F1, vfo: VFO, filt: [100, 2900], want: 7, bandplan: true,
  parts: [{ kind: 'spec2d', h: 0.5, dbl: true, line: '#cfe9ff', fillTop: 'rgba(80,140,255,0.45)' }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }],
});
const tog = (t, on) => `<div class="tog ${on ? 'on' : ''}" style="flex:1;height:30px"><span style="font-size:12px">${t}</span><s></s></div>`;
const sl = (l, v, p, w) => `<div style="margin-top:5px">${slider(l, v, p, w)}</div>`;
const lbl = (t, m = '6px 0 3px') => `<div class="lbl" style="margin:${m}">${t}</div>`;
const panelBg = 'border-radius:16px;background:rgba(10,14,26,.9)';
const tip = (x, y, w, t) => `<div class="tip" style="left:${x}px;top:${y}px;width:${w}px;white-space:normal;line-height:1.35">${t}</div>`;

Object.assign(LAYOUTS, {
  // A: a side panel on the right, like the Noise drawer but narrower and not dimming the screen
  G1: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + vPill() + chipRow() +
      abs(572, 8, 252, 312, `<div style="padding:10px 12px">
        <div class="row" style="justify-content:space-between"><span style="font-size:15px;font-weight:500">Levels</span><span class="dim" style="font-size:16px">✕</span></div>
        ${lbl('SPECTRUM')}<div class="row">${tog('Automatic', false)}</div>
        ${sl('HIGH', '−40 dBm', 70, 228)}${sl('LOW', '−140 dBm', 22, 228)}
        ${lbl('WATERFALL', '9px 0 3px')}<div class="row">${tog('Automatic', false)}</div>
        ${sl('HIGH', '−65 dBm', 58, 228)}${sl('LOW', '−128 dBm', 26, 228)}</div>`, 'glass', panelBg) +
      tip(250, 100, 280, '<b style="color:#ffd27a">A: side panel</b><br><span class="dim">opens from the LEVELS chip; the spectrum and waterfall on the left change as you slide. No dimming. ✕ or tap outside closes it.</span>'),
    canvases: [['#c', bg()]],
  }),

  // B: a low strip along the bottom, over the oldest waterfall lines; the whole spectrum stays clear
  G2: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill() +
      abs(10, 252, 812, 124, `<div style="padding:8px 12px">
        <div class="row" style="justify-content:space-between"><span style="font-size:14px;font-weight:500">Levels</span><span class="dim" style="font-size:16px">✕</span></div>
        <div class="row" style="gap:16px;align-items:flex-start;margin-top:2px">
          <div style="flex:1">${lbl('SPECTRUM')}<div class="row" style="gap:8px">${tog('Auto', false)}</div>${sl('HIGH', '−40', 70, 180)}</div>
          <div style="flex:1">${lbl('&nbsp;')}<div style="height:30px"></div>${sl('LOW', '−140', 22, 180)}</div>
          <div style="flex:1">${lbl('WATERFALL')}<div class="row" style="gap:8px">${tog('Auto', false)}</div>${sl('HIGH', '−65', 58, 180)}</div>
          <div style="flex:1">${lbl('&nbsp;')}<div style="height:30px"></div>${sl('LOW', '−128', 26, 180)}</div>
        </div></div>`, 'glass', panelBg) +
      tip(470, 92, 280, '<b style="color:#ffd27a">B: bottom strip</b><br><span class="dim">takes the bottom row\'s place while open; the whole spectrum and most of the waterfall stay in view.</span>'),
    canvases: [['#c', bg()]],
  }),

  // C: four faders on the right edge, thumb-sized; nothing else covered
  G3: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + vPill() + chipRow(600) +
      abs(612, 8, 212, 368, `<div style="padding:9px 10px">
        <div class="row" style="justify-content:space-between"><span style="font-size:14px;font-weight:500">Levels</span><span class="dim" style="font-size:16px">✕</span></div>
        <div class="row" style="gap:8px;margin-top:6px"><span class="lbl" style="flex:1;text-align:center">SPECTRUM</span><span class="lbl" style="flex:1;text-align:center">WATERFALL</span></div>
        <div class="row" style="gap:6px;margin-top:4px;justify-content:space-between">${vslider('HI', '−40', 70, 236)}${vslider('LO', '−140', 22, 236)}${vslider('HI', '−65', 58, 236)}${vslider('LO', '−128', 26, 236)}</div>
        <div class="row" style="gap:6px;margin-top:8px">${tog('Auto', false)}${tog('Auto', false)}</div></div>`, 'glass', panelBg) +
      tip(250, 100, 300, '<b style="color:#ffd27a">C: faders</b><br><span class="dim">four up/down faders like a mixing desk: push HI up to see stronger signals, LO down for more noise floor. Spectrum and waterfall on the left change live.</span>'),
    canvases: [['#c', bg()]],
  }),
});
})();
