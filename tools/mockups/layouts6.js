// M5 round 2: deskHPSDR's Display-menu leftovers (levels, averaging, peaks &
// hold, info bar) and Screen menu (FFT size, strips, save zoom, narrow peaks).
// See docs/M5-INVENTORY.md.
'use strict';
(() => {
const F0 = 14.044e6, F1 = 14.104e6;
const mCard = () => abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px');
const tBtns = () => abs(720, 12, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass" style="font-weight:600;font-size:13px">2D</div><div class="ico glass">${icon('menu')}</div></div>`);
const vPill = () => abs(266, 8, 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%"><span class="tag vfoA">A</span>${fq(VFO, 34)}<div class="col" style="gap:3px"><span class="tag acc">USB</span><span class="tag">2.8k</span></div></div>`, 'glass');
const CH = [['NR', 'NR2', true], ['BAND', '20m'], ['AF', '73'], ['LNA', '+20 dB'], ['PHONE', 'Off'], ['FILTER', '2.8k'], ['STEP', '10 Hz'], ['SPAN', '60 kHz']];
const chipRow = (y = 328) => abs(10, y, 812, 46, `<div class="row" style="gap:7px">${CH.map(([l, v, on]) => chip(l, v, on)).join('')}</div>`);
const bg = (spec = {}, kind = 'spec2d') => ({
  f0: F0, f1: F1, vfo: VFO, filt: [100, 2900], want: 7, bandplan: true,
  parts: [kind === 'spec2d' ? { kind: 'spec2d', h: 0.5, dbl: true, line: '#cfe9ff', fillTop: 'rgba(80,140,255,0.45)' }
    : { kind: 'stack3d', h: 0.5, line: 'x6100line', fog: true }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }], ...spec,
});
const seg = (items, on, h = 32) => `<div class="seg" style="height:${h}px;align-items:center">${items.map(t => `<b class="${t === on ? 'on' : ''}">${t}</b>`).join('')}</div>`;
const tog = (t, on, sub = '') => `<div class="tog ${on ? 'on' : ''}" style="flex:1"><span>${t}${sub ? `<span class="dim" style="font-size:9.5px;margin-left:6px">${sub}</span>` : ''}</span><s></s></div>`;
const sl = (l, v, p, w = 268) => `<div style="margin-top:6px">${slider(l, v, p, w)}</div>`;
const lbl = (t, m = '9px 0 4px') => `<div class="lbl" style="margin:${m}">${t}</div>`;
const dim = () => abs(0, 0, 832, 384, '', '', 'background:rgba(0,0,0,.42)');
// the Display sheet as it is now, but with tabs along the top (it's grown long)
const sheet = (tab, body, w = 600) => dim() + abs(Math.round((832 - w) / 2), 22, w, 350, `<div style="padding:12px 16px">
  <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">Display</span><span class="dim" style="font-size:10px">deskHPSDR's Display menu</span></div>
  <div style="margin-top:8px">${seg(['Look', 'Levels', 'Peaks', 'Averaging', 'Info bar', 'Speed'], tab, 30)}</div>${body}</div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.95)');
// a dB scale down the left edge of the 2D spectrum (top half: y 0..192)
const scale = (hi, lo, hl = false) => {
  const n = Math.round((hi - lo) / 10), out = [];
  for (let i = 0; i <= n; i++) {
    const d = hi - i * 10, y = 66 + i * (122 / n);
    out.push(abs(790, y - 7, 38, 14, `<span style="font-size:10px;color:${hl ? '#ffd27a' : '#8b97ad'}">${d}</span>`));
  }
  return out.join('');
};

Object.assign(LAYOUTS, {
  // A — the Display sheet gets tabs; Levels tab: auto on/off and the manual levels
  E1: () => ({
    html: cv('c', 0, 0, 832, 384) + sheet('Levels', `
      <div class="row" style="gap:16px;margin-top:6px;align-items:flex-start">
        <div style="flex:1">${lbl('Spectrum')}<div class="row" style="gap:6px">${tog('Automatic', false, 'follows the noise floor')}</div>
          ${sl('HIGH', '−40 dBm', 70)}${sl('LOW', '−140 dBm', 20)}${sl('STEP', '10 dB', 30)}${sl('NOISE MARGIN', '10 dB', 40)}</div>
        <div style="flex:1">${lbl('Waterfall')}<div class="row" style="gap:6px">${tog('Automatic', true)}</div>
          ${sl('HIGH', '−60 dBm', 60)}${sl('LOW', '−130 dBm', 25)}
          <div class="dim" style="font-size:10px;margin-top:10px;line-height:1.35">Automatic greys out HIGH / LOW, like deskHPSDR.<br>Or drag the dB scale on the spectrum (next picture).</div></div>
      </div>`),
    canvases: [['#c', bg()]],
  }),

  // B — levels by finger: drag the dB scale up/down to move, pinch it to stretch; double-tap: automatic
  E2: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill() + chipRow() +
      abs(782, 60, 50, 132, '', '', 'background:linear-gradient(270deg,rgba(255,210,122,.18),rgba(255,210,122,0));border-left:1px solid rgba(255,210,122,.35)') +
      scale(-40, -140, true) +
      `<div class="finger" style="left:784px;top:104px"></div>` +
      `<div class="tip" style="left:540px;top:96px"><b style="color:#ffd27a">Levels −40 … −140 dBm · manual</b><br><span class="dim">drag the scale ↕: move · pinch it: range<br>double-tap the scale: back to automatic</span></div>`,
    canvases: [['#c', bg()]],
  }),

  // C — Averaging tab: deskHPSDR's four averaging kinds + Av. time; Peaks tab's hold settings beside it
  E3: () => ({
    html: cv('c', 0, 0, 832, 384) + sheet('Averaging', `
      <div class="row" style="gap:16px;margin-top:6px;align-items:flex-start">
        <div style="flex:1">${lbl('Averaging')}${seg(['None', 'Recursive', 'Time window', 'Log recursive'], 'Log recursive', 30)}
          ${sl('AV. TIME', '250 ms', 35)}
          ${lbl('Detector', '10px 0 4px')}${seg(['Peak', 'Rosenfell', 'Average', 'Sample'], 'Average', 30)}
          <div class="dim" style="font-size:10px;margin-top:8px">Log recursive: steady noise floor, signals still jump out (deskHPSDR's default).</div></div>
        <div style="flex:1">${lbl('Peaks & hold (2D line)')}${seg(['Off', 'Hold', 'Decay'], 'Decay', 30)}
          ${sl('HOLD TIME', '2.0 s', 30)}${sl('DROP', '6 dB/s', 25)}
          ${lbl('Line colour', '10px 0 4px')}<div class="row" style="gap:8px">${['#ffd27a', '#ff7b7b', '#3ee07a', '#3fd0ff', '#ffffff'].map((c, i) => `<span style="width:26px;height:26px;border-radius:13px;background:${c};${i === 0 ? 'outline:2px solid #fff;outline-offset:2px' : 'opacity:.8'}"></span>`).join('')}</div></div>
      </div>`),
    canvases: [['#c', bg()]],
  }),

  // D — info bar, way 1: a thin strip along the top edge (clock, solar, ADC alarm)
  E4: () => ({
    html: cv('c', 0, 18, 832, 366) + abs(0, 0, 832, 18, `<div class="row" style="gap:16px;justify-content:center;height:100%;font-size:10.5px">
        <span>UTC 18:42</span><span class="dim">SFI 142 · A 6 · K 2</span><span style="color:var(--grn)">ADC OK</span><span class="dim">HL2+ 192.168.88.30</span></div>`, '', 'background:rgba(2,5,12,.85);border-bottom:1px solid var(--line)') +
      abs(10, 28, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px') +
      abs(720, 28, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass" style="font-weight:600;font-size:13px">2D</div><div class="ico glass">${icon('menu')}</div></div>`) +
      abs(266, 26, 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%"><span class="tag vfoA">A</span>${fq(VFO, 34)}<div class="col" style="gap:3px"><span class="tag acc">USB</span><span class="tag">2.8k</span></div></div>`, 'glass') + chipRow(),
    canvases: [['#c', bg()]],
  }),

  // E — info bar, way 2: no strip; clock + solar as a second line in the meter card, and the ADC alarm only when it happens
  E5: () => ({
    html: cv('c', 0, 0, 832, 384) + tBtns() + vPill() + chipRow() +
      abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}
        <div class="row" style="justify-content:space-between;margin-top:7px;font-size:10px"><span>18:42 UTC</span><span class="dim">SFI 142 K 2</span></div>`, 'glass', 'padding:9px 11px') +
      abs(316, 84, 200, 30, `<div class="row" style="gap:7px;justify-content:center;height:100%;font-size:11.5px"><b style="color:#fff">ADC OVERLOAD</b><span style="color:#ffd0d0">lower LNA</span></div>`, '', 'background:rgba(220,40,40,.88);border-radius:15px'),
    canvases: [['#c', bg()]],
  }),

  // F — the Screen page: FFT size, which strips show, save zoom, preserve narrow peaks
  E6: () => ({
    html: cv('c', 0, 0, 832, 384) + dim() + abs(116, 22, 600, 350, `<div style="padding:12px 16px">
      <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">Screen</span><span class="dim" style="font-size:10px">deskHPSDR's Screen menu</span></div>
      <div class="row" style="gap:16px;margin-top:4px;align-items:flex-start">
        <div style="flex:1">${lbl('FFT size (finer = narrower bins, slower)')}
          <div class="col" style="gap:5px">${seg(['Auto', '16k', '32k', '64k'], 'Auto', 30)}${seg(['128k', '256k'], '', 30)}</div>
          <div class="dim" style="font-size:10px;margin-top:5px">384 kHz ÷ 16k = 23.4 Hz per bin</div>
          ${lbl('Zoom', '10px 0 4px')}<div class="row" style="gap:6px">${tog('Save zoom level', true)}</div>
          <div class="row" style="gap:6px;margin-top:6px">${tog('Preserve narrow peaks', false)}</div>
          <div class="dim" style="font-size:10px;margin-top:4px">keeps a thin carrier visible when zoomed out</div></div>
        <div style="flex:1">${lbl('Show on screen')}
          <div class="col" style="gap:6px">${[['Meter', 1], ['VFO pill', 1], ['Bottom row', 1], ['Band plan bar', 1], ['Info bar', 0], ['Frequency axis', 1]].map(([t, on]) => `<div class="row">${tog(t, on)}</div>`).join('')}</div>
          <div class="dim" style="font-size:10px;margin-top:6px">Full screen: always (M3)</div></div>
      </div></div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.95)'),
    canvases: [['#c', bg()]],
  }),
});
})();
