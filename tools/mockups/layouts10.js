// M6 rest (Randal 2026-11-25: finish before the radio test): RX antenna per
// band (Ant menu), RIT/XIT step + split option (VFO panel), 60 m channel
// markers and the region-1 overlays (SDR Device). See docs/M6-INVENTORY.md.
'use strict';
(() => {
const F0 = 5.326e6, F1 = 5.412e6, V = 5.3585e6;
const bg = (f0 = 14.044e6, f1 = 14.104e6, vfo = VFO) => ({
  f0, f1, vfo, filt: [100, 2900], want: 7, bandplan: true,
  parts: [{ kind: 'spec2d', h: 0.5, dbl: true, line: '#cfe9ff', fillTop: 'rgba(80,140,255,0.45)' }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }],
});
const seg = (items, on, h = 28) => `<div class="seg" style="height:${h}px;align-items:center">${items.map(t => `<b class="${t === on ? 'on' : ''}" style="font-size:11px">${t}</b>`).join('')}</div>`;
const tog = (t, on, sub = '', off = false) => `<div class="tog ${on ? 'on' : ''}" style="flex:1;height:32px;${off ? 'opacity:.4' : ''}"><span style="font-size:12px">${t}${sub ? `<span class="dim" style="font-size:9.5px;margin-left:6px">${sub}</span>` : ''}</span><s></s></div>`;
const lbl = (t, m = '8px 0 4px') => `<div class="lbl" style="margin:${m}">${t}</div>`;
const dim = () => abs(0, 0, 832, 384, '', '', 'background:rgba(0,0,0,.42)');
const panel = (title, sub, body, x = 116, w = 600) => dim() + abs(x, 14, w, 360, `<div style="padding:12px 16px">
  <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">${title}</span><span class="dim" style="font-size:10px">${sub}</span></div>${body}</div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.96)');
const tip = (x, y, w, t) => `<div class="tip" style="left:${x}px;top:${y}px;width:${w}px;white-space:normal;line-height:1.35">${t}</div>`;
const bands = ['160m', '80m', '60m', '40m', '30m', '20m', '17m', '15m', '12m', '10m', '6m'];

Object.assign(LAYOUTS, {
  // H1: deskHPSDR's Ant menu, the HL2 + IO board way: per band, main antenna or the IO board's RX input
  H1: () => ({
    html: cv('c', 0, 0, 832, 384) + panel('Ant', "deskHPSDR's ANT menu · RX antenna per band", `
      <div class="dim" style="font-size:10.5px;margin:6px 0 6px">Your IO board switches receive to its separate RX input on the bands set to "RX in". TX antenna comes with M7.</div>
      <div style="display:grid;grid-template-columns:repeat(2,1fr);gap:6px 18px">
        ${bands.map((b, i) => `<div class="row" style="gap:8px;align-items:center"><span style="width:38px;font-size:12px">${b}</span><div style="flex:1">${seg(['ANT1', 'RX in'], i === 1 || i === 0 ? 'RX in' : 'ANT1', 26)}</div></div>`).join('')}
      </div>`),
    canvases: [['#c', bg()]],
  }),

  // H2: the VFO panel gets the RIT/XIT step and "disable split on band change"
  H2: () => ({
    html: cv('c', 0, 0, 832, 384) + panel('VFO', 'the VFO panel (tap the pill), new lines marked', `
      <div class="row" style="gap:16px;margin-top:8px;align-items:flex-start">
        <div style="flex:1">${lbl('A / B')}<div class="row" style="gap:6px">${['A>B', 'B>A', 'A⇄B', 'Split'].map(t => `<div class="btn" style="flex:1;height:32px;display:flex;align-items:center;justify-content:center">${t}</div>`).join('')}</div>
          ${lbl('RIT', '10px 0 4px')}<div class="row" style="gap:6px"><div class="btn" style="flex:1;height:32px;display:flex;align-items:center;justify-content:center">RIT On</div><div class="btn" style="flex:1;height:32px;display:flex;align-items:center;justify-content:center">RIT 0</div></div>
          <div style="margin-top:8px;padding:6px 8px;border-radius:10px;border:1px dashed rgba(255,210,122,.6)">${lbl('RIT / XIT STEP (new)', '0 0 4px')}${seg(['1 Hz', '10 Hz', '100 Hz'], '10 Hz')}</div></div>
        <div style="flex:1">${lbl('Tuning')}<div class="row" style="gap:6px">${tog('CTUN', false)}${tog('Lock', false)}</div>
          ${lbl('Step', '10px 0 4px')}${seg(['1', '10', '25', '50', '100', '1k'], '10')}
          <div style="margin-top:8px;padding:6px 8px;border-radius:10px;border:1px dashed rgba(255,210,122,.6)"><div class="row">${tog('Split off on band change', false, 'new')}</div></div></div>
      </div>`),
    canvases: [['#c', bg()]],
  }),

  // H3: 60 m with the Canadian channels marked, and where the setting lives
  H3: () => ({
    html: cv('c', 0, 0, 832, 384) +
      [5332000, 5348000, 5358500, 5373000, 5405000].map(f => {
        const x = (f - 1500 - F0) / (F1 - F0) * 832, w = 3000 / (F1 - F0) * 832;
        return abs(x, 0, Math.max(w, 3), 192, '', '', 'background:rgba(255,190,60,.28);border-left:1px solid rgba(255,190,60,.8);border-right:1px solid rgba(255,190,60,.8)');
      }).join('') +
      abs(480, 210, 336, 160, `<div style="padding:10px 12px">
        <div class="row" style="justify-content:space-between"><span style="font-size:14px;font-weight:500">SDR Device · band plan</span></div>
        ${lbl('60 m CHANNEL MARKERS')}${seg(['None', 'UK', 'US', 'WRC-15', 'CA'], 'CA')}
        <div class="row" style="gap:6px;margin-top:6px">${tog('Beacon areas', false, 'Region 1', true)}${tog('Contest-free', false, 'Region 1', true)}</div></div>`, 'glass', 'border-radius:16px;background:rgba(10,14,26,.92)') +
      tip(30, 120, 300, '<b style="color:#ffd27a">60 m channels marked</b><br><span class="dim">amber bands on the spectrum at Canada\'s five 60 m channels (deskHPSDR\'s table); the 60 m band stack becomes those channels too. Beacon / contest-free areas only exist for Region 1, so they stay greyed in Region 2.</span>'),
    canvases: [['#c', bg(F0, F1, V)]],
  }),
});
})();
