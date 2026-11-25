// M5 round 1: receive DSP on layout 1 (NR/NB/ANF/SNB/MNF, squelch, EQ,
// the fine settings, a configurable chip row). See docs/M5-INVENTORY.md.
'use strict';
(() => {
const F0 = 14.044e6, F1 = 14.104e6, X = f => (f - F0) / (F1 - F0) * 832;
const mCard = () => abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px');
const tBtns = () => abs(720, 12, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass" style="font-weight:600;font-size:13px">3D</div><div class="ico glass">${icon('menu')}</div></div>`);
const vPill = (ind = [], x = null) => abs(x ?? (ind.length ? 246 : 266), 8, ind.length ? 340 : 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%">
  <span class="tag vfoA">A</span>${fq(VFO, 34)}
  <div class="col" style="gap:3px"><span class="tag acc">USB</span><span class="tag">2.8k</span></div>
  ${ind.length ? `<div class="col" style="gap:2px;padding-left:8px;border-left:1px solid var(--line)">${ind.map(([t, c]) => `<b style="font-size:10px;color:${c}">${t}</b>`).join('')}</div>` : ''}</div>`, 'glass');
const CH = [['BAND', '20m'], ['AF', '73'], ['LNA', '+20 dB'], ['PHONE', 'Off'], ['FILTER', '2.8k'], ['AGC', 'Med'], ['STEP', '10 Hz'], ['SPAN', '60 kHz']];
const chipRow = (list, y = 328) => abs(10, y, 812, 46, `<div class="row" style="gap:7px">${list.map(([l, v, on]) => chip(l, v, on)).join('')}</div>`);
const bg = (spec = {}, kind = 'stack3d') => ({
  f0: F0, f1: F1, vfo: VFO, filt: [100, 2900], want: 7, bandplan: true,
  parts: [kind === 'spec2d' ? { kind: 'spec2d', h: 0.5, dbl: true, line: '#cfe9ff', fillTop: 'rgba(80,140,255,0.45)' }
    : { kind: 'stack3d', h: 0.5, line: 'x6100line', fog: true }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }], ...spec,
});
const seg = (items, on, h = 34) => `<div class="seg" style="height:${h}px;align-items:center">${items.map(t => `<b class="${t === on ? 'on' : ''}">${t}</b>`).join('')}</div>`;
const tog = (t, on, sub = '') => `<div class="tog ${on ? 'on' : ''}" style="flex:1"><span>${t}${sub ? `<span class="dim" style="font-size:9.5px;margin-left:6px">${sub}</span>` : ''}</span><s></s></div>`;
const sl = (l, v, p) => `<div style="margin-top:6px">${slider(l, v, p, 268)}</div>`;
const NRS = ['Off', 'NR', 'NR2', 'NR3', 'NR4', 'NNR'];
const dim = (y0 = 0) => abs(0, y0, 832, 384 - y0, '', '', 'background:rgba(0,0,0,.42)');

Object.assign(LAYOUTS, {
  // A — a DSP drawer from the right edge (round 1, L5), opened by an NR chip
  D1: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + vPill([], 186) + chipRow([['NR', 'NR2', true], ...CH.slice(0, 5)]) +
      abs(0, 0, 512, 384, '', '', 'background:linear-gradient(90deg,rgba(0,0,0,0),rgba(0,0,0,.4))') +
      abs(512, 0, 320, 384, `<div style="padding:12px 14px">
        <div class="row" style="justify-content:space-between"><span style="font-size:17px;font-weight:500">Noise</span><span class="dim" style="font-size:10px">USB · kept per mode</span></div>
        <div class="lbl" style="margin:10px 0 5px">Noise reduction</div>${seg(NRS, 'NR2')}
        <div class="lbl" style="margin:9px 0 5px">Noise blanker</div>${seg(['Off', 'NB', 'NB2'], 'NB')}
        <div class="row" style="gap:6px;margin-top:9px">${tog('ANF', false)}${tog('SNB', true)}${tog('MNF', false)}</div>
        <div class="lbl" style="margin:10px 0 5px">Position</div>${seg(['Pre AGC', 'Post AGC'], 'Post AGC', 30)}
        <div class="row" style="gap:6px;margin-top:8px"><div class="btn" style="flex:1;height:32px;font-size:11.5px">NR2 settings ›</div><div class="btn" style="flex:1;height:32px;font-size:11.5px">NB settings ›</div></div>
        </div>`, 'glass', 'border-radius:18px 0 0 18px;background:rgba(9,12,22,.9)'),
    canvases: [['#c', bg()]],
  }),

  // B — a bottom sheet from an NR chip: one row each, hold a button for its settings
  D2: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill() + dim(150) +
      abs(136, 168, 560, 216, `<div style="padding:12px 16px">
        <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">Noise · USB</span><span class="dim" style="font-size:10.5px">hold a button for its settings</span></div>
        <div class="row" style="gap:10px;margin-top:10px"><span class="lbl" style="width:28px">NR</span><div style="flex:1">${seg(NRS, 'NR2', 36)}</div></div>
        <div class="row" style="gap:10px;margin-top:7px"><span class="lbl" style="width:28px">NB</span><div style="flex:1">${seg(['Off', 'NB', 'NB2'], 'Off', 36)}</div></div>
        <div class="row" style="gap:7px;margin-top:9px">${tog('ANF', false)}${tog('SNB', true)}${tog('MNF', false, '1.2 kHz')}${tog('Post AGC', true)}</div>
        </div>`, 'glass', 'border-radius:18px 18px 0 0;background:rgba(10,14,26,.95)'),
    canvases: [['#c', bg()]],
  }),

  // C — what's on shows in the pill (like the RIT/split indicators); tap a chip-row button to toggle
  D3: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill([['NR2', 'var(--acc)'], ['NB', 'var(--acc)'], ['SNB', 'var(--acc)']]) +
      abs(10, 276, 812, 44, `<div class="row" style="gap:7px">${[['NR2', 1], ['NB', 1], ['ANF', 0], ['SNB', 1], ['MNF', 0], ['SQL', 0], ['EQ', 0], ['BIN', 0]].map(([t, on]) => `<div class="btn ${on ? 'on' : ''}" style="width:64px;height:38px">${t}</div>`).join('')}<span class="dim" style="font-size:10px;margin-left:8px">tap: on/off · hold: settings</span></div>`) +
      chipRow(CH),
    canvases: [['#c', bg()]],
  }),

  // D — the bottom row has pages; swipe to the DSP page (the X6100's way)
  D4: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill() +
      abs(10, 306, 400, 18, `<div class="row" style="gap:14px;font-size:10.5px;letter-spacing:.6px"><span class="dim">RADIO</span><b style="color:var(--acc)">DSP</b><span class="dim">VFO</span><span class="dim" style="margin-left:6px">‹ swipe ›</span></div>`, '', 'padding-left:4px') +
      chipRow([['NR', 'NR2', true], ['NB', 'Off'], ['ANF', 'Off'], ['SNB', 'On', true], ['NOTCH', 'Off'], ['SQL', 'Off'], ['EQ', 'Off'], ['BINAURAL', 'Off']]),
    canvases: [['#c', bg()]],
  }),

  // E — the manual notch on the spectrum: double-tap to put it there, drag its edges for width
  D5: () => {
    const nf = 14.0758e6, w = 0.00045e6;
    return {
      html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill([['MNF', '#ff7b7b']]) + chipRow(CH) +
        abs(X(nf - w / 2), 40, X(nf + w / 2) - X(nf - w / 2), 344, '', '', 'background:rgba(255,77,77,.22);border-left:2px solid rgba(255,90,90,.85);border-right:2px solid rgba(255,90,90,.85)') +
        `<div class="finger" style="left:${X(nf + w / 2) - 23}px;top:150px"></div>` +
        `<div class="tip" style="left:${X(nf + w / 2) + 30}px;top:120px"><b style="color:#ff7b7b">Notch 14.0758 · 450 Hz</b><br><span class="dim">double-tap: notch here · drag an edge: width<br>flick it off the top: remove</span></div>`,
      canvases: [['#c', bg({}, 'spec2d')]],
    };
  },

  // F — squelch as a line on the spectrum, like the AGC line (M4 pick F)
  D6: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill([['SQL', 'var(--grn)']]) + chipRow([['SQL', 'On', true], ...CH.slice(0, 6)]) +
      abs(X(VFO) + 4, 112, X(VFO + 2900) - X(VFO) - 4, 2, '', '', 'background:repeating-linear-gradient(90deg,#3ee07a 0 10px,transparent 10px 18px)') +
      abs(X(VFO + 2900) + 10, 98, 176, 30, `<div class="row" style="gap:6px;padding:0 10px;height:100%;font-size:11px"><b style="color:var(--grn)">SQL</b><span>−105 dBm</span><span class="dim">muted · drag ↕</span></div>`, 'glass', 'border-radius:15px;border-color:rgba(62,224,122,.5)') +
      `<div class="finger" style="left:${X(VFO + 1400) - 23}px;top:90px"></div>`,
    canvases: [['#c', bg({}, 'spec2d')]],
  }),

  // G — the RX equalizer as a graph you drag points on (WDSP's continuous-gain EQ)
  D7: () => {
    const fr = [50, 100, 200, 500, 1000, 1500, 2000, 2500, 3000, 5000, 6000, 8000];
    const g = [-12, -8, -3, 0, 2, 3, 4, 4, 3, 0, -6, -12];
    const gx = f => 40 + Math.log10(f / 50) / Math.log10(8000 / 50) * 560, gy = d => 90 - d * 3.6;
    const pts = fr.map((f, i) => [gx(f), gy(g[i])]);
    const path = 'M' + pts.map(p => p.join(',')).join(' L');
    const svg = `<svg width="640" height="180" style="display:block">
      ${[-20, -10, 0, 10, 20].map(d => `<line x1="40" x2="600" y1="${gy(d)}" y2="${gy(d)}" stroke="rgba(255,255,255,${d ? .07 : .2})"/><text x="8" y="${gy(d) + 4}" fill="#8b97ad" font-size="10">${d > 0 ? '+' : ''}${d}</text>`).join('')}
      ${[100, 300, 1000, 3000, 8000].map(f => `<text x="${gx(f) - 12}" y="176" fill="#8b97ad" font-size="10">${f >= 1000 ? f / 1000 + 'k' : f}</text>`).join('')}
      <path d="${path} L600,${gy(-12)} L600,90 L40,90 Z" fill="rgba(63,208,255,.10)"/>
      <path d="${path}" fill="none" stroke="#3fd0ff" stroke-width="2.4"/>
      ${pts.map(([x, y], i) => `<circle cx="${x}" cy="${y}" r="${i === 6 ? 9 : 6}" fill="${i === 6 ? '#3fd0ff' : '#0b1220'}" stroke="#3fd0ff" stroke-width="2"/>`).join('')}
      <rect x="${gx(2500) - 42}" y="${gy(4) - 36}" width="84" height="22" rx="7" fill="rgba(5,8,16,.9)" stroke="rgba(255,255,255,.2)"/><text x="${gx(2000) - 4}" y="${gy(4) - 21}" fill="#e9eef7" font-size="11">2.0 kHz +4 dB</text>
    </svg>`;
    return {
      html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill() + dim(0) +
        abs(86, 70, 660, 300, `<div style="padding:12px 16px">
          <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">RX equalizer</span>
          <div class="row" style="gap:7px"><div class="tog on" style="width:110px;height:32px"><span>Enable</span><s></s></div><div class="btn" style="width:70px;height:32px">Flat</div></div></div>
          ${svg}
          <div class="row" style="gap:10px;margin-top:2px">${slider('PREAMP', '+2 dB', 55, 300)}<span class="dim" style="font-size:10px">drag a point ↕ gain, ↔ frequency · tap the line to add one</span></div>
          </div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.95)'),
      canvases: [['#c', bg()]],
    };
  },

  // H — hold NR2: every deskHPSDR NR2 setting, the phone's way
  D8: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + tBtns() + vPill() + dim(0) +
      abs(116, 70, 600, 306, `<div style="padding:12px 16px">
        <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">NR2 settings · USB</span><div class="btn" style="width:84px;height:30px">Defaults</div></div>
        <div class="row" style="gap:14px;margin-top:8px;align-items:flex-start">
          <div style="flex:1"><div class="lbl" style="margin-bottom:4px">Gain method</div>${seg(['Linear', 'Log', 'Gamma', 'Trained'], 'Gamma', 30)}
            <div class="lbl" style="margin:8px 0 4px">Noise estimate</div>${seg(['OSMS', 'MMSE', 'NSTAT'], 'OSMS', 30)}
            <div class="row" style="gap:6px;margin-top:8px">${tog('Artifact elim.', true)}</div>
            <div class="lbl" style="margin:8px 0 2px">Trained (Gain method Trained)</div>${sl('THRESH', '−0.5', 45)}${sl('T2', '0.20', 30)}</div>
          <div style="flex:1"><div class="row" style="gap:6px">${tog('Post-processing', true)}</div>
            ${sl('POST LEVEL', '15', 40)}${sl('POST RATE', '5', 25)}${sl('POST FACTOR', '15', 40)}${sl('POST TAPER', '12', 33)}</div>
        </div></div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.95)'),
    canvases: [['#c', bg()]],
  }),

  // I — deskHPSDR's Toolbar menu, the phone's way: edit the bottom chip row
  D9: () => ({
    html: cv('c', 0, 0, 832, 384) + mCard() + vPill() + dim(0) +
      abs(86, 110, 660, 196, `<div style="padding:12px 16px">
        <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">Bottom row</span><div class="btn on" style="width:70px;height:30px">Done</div></div>
        <div class="dim" style="font-size:10.5px;margin-top:4px">Drag a button down into the row, or out of it to remove it. deskHPSDR's actions, all of them.</div>
        <div class="row" style="gap:6px;flex-wrap:wrap;margin-top:10px">${['NR', 'NB', 'ANF', 'SNB', 'NOTCH', 'SQL', 'EQ', 'BINAURAL', 'RIT', 'CTUN', 'LOCK', 'A⇄B', 'MODE', 'BAND +', 'BAND −', 'ZOOM', 'MUTE', 'CW PEAK', 'AGC GAIN', '…'].map(t => `<div class="btn" style="height:30px;padding:0 10px;font-size:11px">${t}</div>`).join('')}</div>
        </div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.95)') +
      abs(10, 328, 812, 46, `<div class="row" style="gap:7px">${[['BAND', '20m'], ['AF', '73'], ['NR', 'NR2'], ['FILTER', '2.8k'], ['AGC', 'Med'], ['SQL', 'Off']].map(([l, v]) => `<div style="position:relative">${chip(l, v, false)}<span style="position:absolute;right:-5px;top:-6px;width:16px;height:16px;border-radius:8px;background:#ff4d4d;color:#fff;font-size:11px;line-height:16px;text-align:center">×</span></div>`).join('')}<div class="chip glass" style="border:1px dashed rgba(63,208,255,.6)"><b style="color:var(--acc)">＋</b></div></div>`) +
      `<div class="finger" style="left:520px;top:190px"></div>`,
    canvases: [['#c', bg()]],
  }),
});
})();
