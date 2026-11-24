// Layouts 2-12 (landscape) and P1-P4 (portrait). Helpers in layouts.js.
'use strict';
const bandChips = (cur, w = 54, h = 40, cls = 'btn') =>
  ['160', '80', '60', '40', '30', '20', '17', '15', '12', '10', '6', 'GEN']
    .map(b => `<div class="${cls} ${b === cur ? 'on' : ''}" style="width:${w}px;height:${h}px;font-size:14px">${b}</div>`).join('');

Object.assign(LAYOUTS, {
  // 2 — Side rail + right column
  L2: () => {
    const rail = [['grid', 'BAND', '20 m'], ['mode', 'MODE', 'USB-D'], ['filt', 'FILTER', '3.0k'], ['dsp', 'DSP', 'NR2'],
      ['wave', 'AGC', 'Med'], ['layers', 'VIEW', '3D'], ['menu', 'MENU', '']];
    const rb = rail.map(([ic, l, v], i) => `<div class="col" style="align-items:center;justify-content:center;height:50px;gap:1px;border-radius:12px;${i === 3 ? 'background:rgba(63,208,255,.12);color:var(--acc)' : 'color:var(--txt)'}">${icon(ic, 19)}<span style="font-size:9px;letter-spacing:.6px;${i === 3 ? '' : 'color:var(--dim)'}">${v || l}</span></div>`).join('');
    return {
      html: abs(0, 0, 70, 384, `<div class="col" style="gap:2px;padding:6px 6px">${rb}</div>`, '', 'background:var(--panel);border-right:1px solid var(--line)') +
        cv('c', 70, 0, 542, 384) +
        abs(612, 0, 220, 384, `
          <div style="padding:12px 14px 8px">
            <div class="row" style="justify-content:space-between"><span class="tag vfoA">VFO A</span><span class="row dim" style="gap:6px">${icon('lock', 15)}<span style="font-size:11px">Step 10 Hz</span></span></div>
            <div style="margin-top:8px">${fq(VFO, 33)}</div>
            <div class="row" style="gap:5px;margin-top:7px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span><span class="tag">AGC M</span></div>
            <div class="row dim" style="margin-top:9px;gap:6px;font-size:12px"><span class="tag" style="background:none;border:1px solid var(--line);color:var(--dim)">B</span>${fq(VFOB, 15, 'dim')}<span style="margin-left:auto">${icon('swap', 16)}</span></div>
          </div>
          <div style="border-top:1px solid var(--line);padding:6px 14px 0;display:flex;flex-direction:column;align-items:center">${arcMeter(180, 0.42)}
            <div class="row" style="justify-content:space-between;width:100%;margin-top:-4px"><span style="font-size:16px;font-weight:500">S7</span><span class="dim">−87 dBm</span></div></div>
          <div class="col" style="gap:7px;padding:10px 14px">${slider('AF', '42', 42, 192)}${slider('LNA', '+20 dB', 55, 192)}${slider('SQL', 'Off', 0, 192)}</div>`, '', 'background:var(--panel);border-left:1px solid var(--line)'),
      canvases: [['#c', { f0: 14.062e6, f1: 14.090e6, vfo: VFO, filt: USB, want: 6, vfoCol: '#ffb341', filtCol: 'rgba(255,179,65,0.10)', filtEdge: 'rgba(255,179,65,0.4)',
        parts: [{ kind: 'spec2d', h: 0.4, dbl: true, line: '#9ff0ff', fillTop: 'rgba(47,224,230,0.45)' }, { kind: 'wf2d', h: 0.6, pal: 'aurora', lo: -120, hi: -75 }],
        labels: [{ f: 14.0705e6 + 900, y: 0.07, t: 'PSK31' }, { f: 14.0755e6, y: 0.07, t: 'FT8' }, { f: 14.0792e6, y: 0.07, t: 'JS8' }, { f: 14.0836e6, y: 0.07, t: 'RTTY' }] }]],
    };
  },

  // 3 — deskHPSDR, refined: three layers like deskHPSDR
  L3: () => ({
    html: abs(0, 0, 832, 62, `<div class="row" style="height:100%;padding:0 10px;gap:12px">
        <div class="row" style="gap:8px;padding:4px 12px;border-radius:10px;background:linear-gradient(180deg,#ff9a2a,#d96a10);color:#1d0d00;height:50px">
          <span style="font-size:16px;font-weight:700">A</span>${fq(VFO, 32, 'ink')}</div>
        <div style="display:grid;grid-template-columns:repeat(4,auto);gap:3px 10px;font-size:10.5px;font-weight:600;letter-spacing:.3px">
          <span style="color:var(--amb)">USB-D 3.0k</span><span style="color:var(--grn)">NR2</span><span class="dim2">NB</span><span style="color:var(--grn)">SNB</span>
          <span style="color:var(--amb)">AGC MED</span><span class="dim2">ANF</span><span class="dim2">SPLIT</span><span class="dim">RIT 0</span>
          <span class="dim">Step 10 Hz</span><span class="dim">LNA +20</span><span class="dim2">LOCK</span><span class="dim">CTUN</span></div>
        <div class="row dim" style="gap:6px;padding:0 10px;height:40px;border-radius:9px;background:rgba(255,154,42,.07);border:1px solid rgba(255,154,42,.25)"><span style="color:#d99a5a;font-weight:700">B</span>${fq(VFOB, 18)}</div>
        <div style="margin-left:auto">${meter(150, 10, 7.4, true)}</div>
        <div class="btn" style="width:56px;height:44px">MENU</div></div>`, '', 'background:#090c14;border-bottom:1px solid var(--line)') +
      cv('c', 0, 62, 832, 240) +
      abs(0, 302, 832, 38, `<div class="row" style="gap:10px;padding:4px 10px">${slider('AF', '42', 42, 150)}${slider('AGC', '80', 70, 150)}${slider('LNA', '+20 dB', 55, 150)}${slider('SQL', 'Off', 0, 150)}${slider('ZOOM', '×1', 5, 150)}</div>`, '', 'background:#080b12') +
      abs(0, 340, 832, 44, `<div class="row" style="gap:6px;padding:2px 10px">${['BAND', 'MODE', 'FILTER', 'NOISE', 'AGC', 'A⇄B', 'SPLIT', 'RIT', 'CTUN', '3D WF'].map((b, i) => `<div class="btn ${i === 9 ? 'on' : ''}" style="flex:1;height:38px">${b}</div>`).join('')}</div>`, '', 'background:#080b12'),
    canvases: [['#c', { f0: 14.000e6, f1: 14.192e6, vfo: VFO, filt: USB, want: 9, vfoCol: '#ffffff', res: 1,
      parts: [{ kind: 'spec2d', h: 0.48, fill: 'pal', pal: 'desk', line: '#f6f6c0', glow: false, dbl: true, lo: -122, hi: -62 },
        { kind: 'surface3d', h: 0.52, pal: 'desk', n: 48 }] }]],
  }),

  // 4 — Immersive: nothing but the waterfall, edge drawers
  L4: () => ({
    html: cv('c', 0, 0, 832, 384) +
      abs(18, 12, 320, null, `<div class="row" style="gap:10px">${fq(VFO, 42, 'light')}</div>
        <div class="row" style="gap:8px;margin-top:6px;font-size:12px"><span style="color:#ffb06a;font-weight:600">USB-D</span><span class="dim">3.0k · AGC M · NR2</span></div>`, '', 'text-shadow:0 2px 12px rgba(0,0,0,.8)') +
      abs(626, 16, 190, null, `<div class="row" style="justify-content:flex-end;gap:8px;margin-bottom:5px"><span style="font-size:18px;font-weight:500">S7</span><span class="dim">−87 dBm</span></div>${meter(190, 5, 7.4, false)}`) +
      abs(0, 152, 6, 80, '', 'handle', 'border-radius:0 4px 4px 0;opacity:.6') + abs(10, 180, 60, null, '<span class="lbl" style="color:rgba(255,255,255,.45)">BAND</span>') +
      abs(826, 152, 6, 80, '', 'handle', 'border-radius:4px 0 0 4px;opacity:.6') + abs(778, 180, 40, null, '<span class="lbl" style="color:rgba(255,255,255,.45)">DSP</span>') +
      abs(376, 376, 80, 5, '', 'handle', 'opacity:.6') + abs(384, 356, 80, null, '<span class="lbl" style="color:rgba(255,255,255,.45)">CONTROLS</span>'),
    canvases: [['#c', { f0: 14.0655e6, f1: 14.0855e6, vfo: VFO, filt: USB, want: 8, vfoCol: 'rgba(255,190,120,.9)', filtCol: 'rgba(255,170,80,0.08)', filtEdge: 'rgba(255,170,80,0.3)', axisY: 0.58,
      parts: [{ kind: 'surface3d', h: 0.58, pal: 'ember', n: 56, back: 0.3, height: 0.5 }, { kind: 'wf2d', h: 0.42, pal: 'ember', lo: -121, hi: -74 }] }]],
  }),

  // 5 — Immersive with the DSP drawer pulled in from the right
  L5: () => {
    const base = LAYOUTS.L4();
    return {
      html: base.html.replace(/<div class="a handle"[^]*$/, '') +
        abs(0, 0, 500, 384, '', '', 'background:linear-gradient(90deg,rgba(0,0,0,0),rgba(0,0,0,.35))') +
        abs(500, 0, 332, 384, `<div style="padding:12px 14px">
          <div class="row" style="justify-content:space-between"><span style="font-size:17px;font-weight:500">DSP</span><span class="dim">${icon('chev', 18)}</span></div>
          <div class="seg" style="margin-top:10px"><b class="on">Noise</b><b>AGC</b><b>Filter</b><b>Audio</b><b>EQ</b></div>
          <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:10px">
            <div class="tog"><span>NB</span><s></s></div><div class="tog"><span>NB2</span><s></s></div>
            <div class="tog on"><span>SNB</span><s></s></div><div class="tog"><span>ANF</span><s></s></div></div>
          <div class="lbl" style="margin:11px 0 5px">Noise reduction</div>
          <div class="seg"><b>Off</b><b>NR1</b><b class="on">NR2</b><b>NR3</b><b>NR4</b><b>NNR</b></div>
          <div class="lbl" style="margin:11px 0 5px">NR2 settings</div>
          <div class="col" style="gap:6px">${slider('GAIN METHOD', 'Gamma', 60, 302)}${slider('TRAIN T1', '−0.5', 40, 302)}</div>
          <div class="dim" style="font-size:10px;margin-top:9px">Hold any control for its full settings</div></div>`, 'glass', 'border-radius:18px 0 0 18px;background:rgba(9,12,22,.78)'),
      canvases: base.canvases,
    };
  },

  // 6 — Thumb zones: two-hand grip, controls at the edges
  L6: () => ({
    html: abs(0, 0, 124, 384, `<div class="col" style="gap:8px;padding:10px 10px">
        ${[['BAND', '20 m'], ['MODE', 'USB-D'], ['FILTER', '3.0k'], ['STEP', '10 Hz']].map(([l, v], i) => `<div class="btn col ${i === 2 ? 'on' : ''}" style="height:${i === 3 ? 56 : 80}px;gap:3px;border-radius:16px"><span class="lbl" style="${i === 2 ? 'color:var(--acc)' : ''}">${l}</span><span style="font-size:${i === 3 ? 16 : 20}px">${v}</span></div>`).join('')}</div>`, '', 'background:var(--panel)') +
      abs(124, 0, 584, 58, `<div class="row" style="height:100%;justify-content:space-between;padding:0 14px">
        <div class="row" style="gap:10px"><span class="tag vfoA">A</span>${fq(VFO, 34)}</div>
        <div class="col" style="align-items:flex-end;gap:4px"><span><b style="font-size:16px;font-weight:500">S7</b> <span class="dim">−87 dBm</span></span>${meter(150, 6, 7.4, false)}</div></div>`, '', 'background:#070a12') +
      cv('c', 124, 58, 584, 326) +
      abs(708, 0, 124, 384, `<div class="row" style="gap:10px;padding:10px 9px;height:100%;align-items:stretch">${vslider('AF', '42', 42, 364)}${vslider('LNA', '+20', 55, 364)}</div>`, '', 'background:var(--panel)'),
    canvases: [['#c', SPEC.look3(14.058e6, 14.090e6, 0.48, { want: 6 })]],
  }),

  // 7 — Big number: the frequency is the hero, band row at the bottom
  L7: () => ({
    html: abs(0, 0, 832, 112, `<div class="row" style="height:78px;padding:0 18px;gap:16px">
        <div class="col" style="gap:5px"><span class="tag vfoA">VFO A</span><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>
        ${fq(VFO, 66, 'light').replace('<small>', '<small style="font-size:.5em">')}
        <div class="col" style="margin-left:auto;align-items:flex-end;gap:2px"><span style="font-size:30px;font-weight:300">S7</span><span class="dim">−87 dBm</span></div>
        <div class="col" style="gap:6px;margin-left:10px"><div class="ico" style="background:var(--panel2)">${icon('menu')}</div></div></div>
        <div style="padding:0 18px">${meter(796, 8, 7.4, false)}</div>`, '', 'background:radial-gradient(120% 140% at 30% 0%,#0f1a30,#05070d)') +
      cv('c', 0, 112, 832, 222) +
      abs(0, 334, 832, 50, `<div class="row" style="gap:6px;padding:5px 10px">${bandChips('20', 58, 40)}<div class="btn" style="width:58px;height:40px">${icon('dots')}</div></div>`, '', 'background:#070a12;border-top:1px solid var(--line)'),
    canvases: [['#c', { f0: 14.0655e6, f1: 14.0855e6, vfo: VFO, filt: USB, want: 8,
      parts: [{ kind: 'spec2d', h: 0.36, hi: -72, line: '#d9ffb0', fillTop: 'rgba(80,230,120,0.45)', fillBot: 'rgba(80,230,120,0.02)' }, { kind: 'wf2d', h: 0.64, pal: 'x6100' }] }]],
  }),

  // 8 — Band scope with band plan and peak labels (whole 20 m segment)
  L8: () => ({
    html: cv('c', 0, 0, 832, 384) +
      abs(10, 10, null, null, `<div class="row" style="gap:9px;padding:7px 12px"><span class="tag vfoA">A</span>${fq(VFO, 24)}<span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>`, 'glass') +
      abs(612, 10, 210, null, `<div class="row" style="justify-content:space-between;margin-bottom:5px"><b style="font-size:15px;font-weight:500">S7</b><span class="dim">−87 dBm</span></div>${meter(186, 6, 7.4, false)}`, 'glass', 'padding:8px 12px') +
      abs(712, 320, null, null, `<div class="row" style="gap:8px"><div class="ico glass" style="width:50px;height:50px">${icon('sliders')}</div><div class="ico glass" style="width:50px;height:50px">${icon('menu')}</div></div>`) +
      abs(10, 330, null, null, `<div class="row" style="gap:6px;padding:6px 10px;font-size:11px" ><span class="dim">Span</span><b style="font-weight:500">192 kHz</b><span class="dim" style="margin-left:8px">Pinch to zoom · tap a label to tune</span></div>`, 'glass'),
    canvases: [['#c', { f0: 14.000e6, f1: 14.200e6, vfo: VFO, filt: USB, want: 10, bandplan: true, axisY: 0.62,
      parts: [{ kind: 'stack3d', h: 0.62, line: 'x6100line', fog: true, back: 0.34, height: 0.55, n: 50 }, { kind: 'wf2d', h: 0.38, pal: 'x6100' }],
      labels: [{ f: 14.0105e6, y: 0.30, t: 'CW 14.0105' }, { f: 14.0231e6, y: 0.24, t: 'CW 14.0231' }, { f: 14.0521e6, y: 0.30, t: 'CW 14.0521' },
        { f: 14.0755e6, y: 0.24, t: 'FT8 · 24', c: '#7cf59a' }, { f: 14.0836e6, y: 0.31, t: 'RTTY', c: '#7cf59a' }, { f: 14.100e6, y: 0.27, t: 'NCDXF', c: '#ff8ae0' },
        { f: 14.1715e6, y: 0.22, t: 'USB 14.170', c: '#ffc56b' }, { f: 14.1505e6, y: 0.30, t: 'USB 14.150', c: '#ffc56b' }, { f: 14.1870e6, y: 0.30, t: 'USB 14.185', c: '#ffc56b' }] }]],
  }),

  // 9 — Tuning by touch, close up: filter edge drag, gestures
  L9: () => {
    const f0 = 14.0733e6, f1 = 14.0779e6, X = f => (f - f0) / (f1 - f0) * 832;
    const xa = X(VFO + 100), xb = X(VFO + 3100);
    const grip = x => abs(x - 13, 120, 26, 70, `<div style="width:4px;height:44px;margin:13px auto;border-radius:2px;background:#fff"></div>`, '', 'border-radius:13px;background:rgba(63,208,255,.55);box-shadow:0 0 16px rgba(63,208,255,.7)');
    return {
      html: cv('c', 0, 0, 832, 384) + grip(xa) + grip(xb) +
        abs(xa, 196, xb - xa, 22, `<div class="row" style="justify-content:center;height:100%;font-size:11px;color:var(--acc);font-weight:600">⟵ 3.0 kHz ⟶</div>`) +
        `<div class="finger" style="left:${xb - 23}px;top:132px"></div>` +
        `<div class="tip" style="left:${xb + 26}px;top:116px"><b style="color:var(--acc)">High cut 3100 Hz</b><br><span class="dim">drag the edge</span></div>` +
        abs(320, 10, null, null, `<div class="row" style="gap:9px;padding:7px 14px"><span class="tag vfoA">A</span>${fq(VFO, 26)}<span class="tag acc">USB-D</span></div>`, 'glass') +
        abs(706, 10, null, null, `<div class="row" style="gap:6px;padding:8px 12px">${icon('pinch', 16)}<b style="font-weight:500">×42</b><span class="dim">4.6 kHz</span></div>`, 'glass') +
        abs(10, 268, 380, null, `<div style="display:grid;grid-template-columns:auto 1fr;gap:4px 10px;font-size:11px">
          <b>Drag</b><span class="dim">tune (spectrum moves under the line)</span>
          <b>Fling</b><span class="dim">keep tuning, slows to a stop</span>
          <b>Pinch</b><span class="dim">zoom in / out</span>
          <b>Tap</b><span class="dim">jump to that signal</span>
          <b>Edges</b><span class="dim">drag to change filter width</span></div>`, 'glass', 'padding:9px 12px'),
      canvases: [['#c', { f0, f1, vfo: VFO, filt: USB, want: 6, res: 1, filtCol: 'rgba(63,208,255,0.12)', filtEdge: 'rgba(63,208,255,0.6)', vfoCol: '#ff5a5a',
        parts: [{ kind: 'spec2d', h: 0.52, line: '#bff3ff', dbl: true, dblx: 6, lo: -124, hi: -62 }, { kind: 'wf2d', h: 0.48, pal: 'aurora', lo: -120, hi: -72 }] }]],
    };
  },

  // 10 — Floating cards over a full-screen 3D waterfall
  L10: () => {
    const grab = `<div class="row" style="justify-content:center;margin:-4px 0 4px"><div style="width:24px;height:3px;border-radius:2px;background:rgba(255,255,255,.25)"></div></div>`;
    return {
      html: cv('c', 0, 0, 832, 384) +
        abs(12, 12, 230, null, `${grab}<div class="row" style="gap:8px"><span class="tag vfoA">A</span>${fq(VFO, 28)}</div>
          <div class="row" style="gap:5px;margin-top:6px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span><span class="tag">AGC M</span></div>
          <div class="row dim" style="gap:7px;margin-top:7px;font-size:11px"><b style="color:var(--dim)">B</b>${fq(VFOB, 14, 'dim')}<span style="margin-left:auto">${icon('swap', 15)}</span></div>`, 'glass', 'padding:10px 12px') +
        abs(632, 12, 188, null, `${grab}<div class="col" style="align-items:center">${arcMeter(160, 0.42)}<div class="row" style="justify-content:space-between;width:100%;margin-top:-2px"><b style="font-size:15px;font-weight:500">S7</b><span class="dim">−87 dBm</span></div></div>`, 'glass', 'padding:10px 12px') +
        abs(12, 262, 236, null, `${grab}<div class="col" style="gap:6px">${slider('AF', '42', 42, 210)}${slider('LNA', '+20 dB', 55, 210)}</div>`, 'glass', 'padding:10px 12px') +
        abs(632, 262, 188, null, `${grab}<div style="display:grid;grid-template-columns:1fr 1fr;gap:5px">${['NR2', 'SNB', 'NB', 'ANF'].map((t, i) => `<div class="btn ${i < 2 ? 'on' : ''}" style="height:34px">${t}</div>`).join('')}</div>`, 'glass', 'padding:10px 12px') +
        abs(360, 334, 112, 40, `<div class="row" style="justify-content:center;gap:6px;height:100%;font-size:11px" class="dim">${icon('layers', 16)}<span class="dim">+ card</span></div>`, 'glass', 'border-radius:20px'),
      canvases: [['#c', { f0: 14.054e6, f1: 14.094e6, vfo: VFO, filt: USB, want: 8, axisY: 0.6, vfoCol: '#aef', filtCol: 'rgba(170,238,255,0.07)', filtEdge: 'rgba(170,238,255,.3)',
        parts: [{ kind: 'surface3d', h: 0.6, pal: 'ice', n: 60, back: 0.18, height: 0.6, persp: 0.3 }, { kind: 'wf2d', h: 0.4, pal: 'ice', lo: -120, hi: -74 }] }]],
    };
  },

  // 11 — Display left, tile panel right
  L11: () => {
    const tiles = [['AGC', 'Med', 1], ['NR', 'NR2', 1], ['NB', 'Off', 0], ['ANF', 'Off', 0], ['LNA', '+20 dB', 0], ['AF', '42', 0], ['FILTER', '3.0k', 0], ['STEP', '10 Hz', 0]];
    return {
      html: cv('c', 0, 0, 574, 384) +
        abs(574, 0, 258, 384, `<div style="padding:10px 12px 0">
          <div class="row" style="justify-content:space-between"><span class="row" style="gap:6px"><span class="tag vfoA">A</span><span class="tag acc">USB-D</span><span class="tag">20 m</span></span><span class="dim">${icon('menu', 20)}</span></div>
          <div style="margin:8px 0 6px">${fq(VFO, 36)}</div>
          <div class="row" style="justify-content:space-between;margin-bottom:4px"><b style="font-weight:500">S7</b><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(232, 7, 7.4, false)}</div>
          <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px;padding:10px 12px">${tiles.map(([l, v, on]) => `<div class="btn col ${on ? 'on' : ''}" style="height:52px;align-items:flex-start;padding:0 12px;gap:3px;border-radius:13px"><span class="lbl" style="${on ? 'color:var(--acc)' : ''}">${l}</span><span style="font-size:17px">${v}</span></div>`).join('')}</div>
          <div class="dim" style="text-align:center;font-size:10px">tap = on/off · hold = settings · drag tiles to rearrange</div>`, '', 'background:var(--panel);border-left:1px solid var(--line)'),
      canvases: [['#c', SPEC.look3(14.064e6, 14.088e6, 0.5, { want: 5 })]],
    };
  },

  // 12 — Flat 2D, tabbed control drawer at the bottom
  L12: () => ({
    html: abs(0, 0, 832, 50, `<div class="row" style="height:100%;padding:0 12px;gap:12px">
        <span class="tag vfoA">A</span>${fq(VFO, 28)}<span class="tag acc">USB-D</span><span class="tag">3.0k</span><span class="tag">AGC M</span><span class="tag grn">NR2</span>
        <span class="row dim" style="gap:6px;margin-left:12px">B ${fq(VFOB, 14, 'dim')}</span>
        <div style="margin-left:auto" class="row"><b style="font-weight:500;margin-right:8px">S7</b>${meter(130, 8, 7.4, false)}</div></div>`, '', 'background:#080b13;border-bottom:1px solid var(--line)') +
      cv('c', 0, 50, 832, 236) +
      abs(0, 286, 832, 98, `<div class="row" style="gap:2px;padding:4px 10px 0">${['Audio', 'DSP', 'Band', 'Mode', 'Filter', 'VFO', 'View'].map((t, i) => `<div style="padding:6px 16px;border-radius:9px;font-size:12.5px;font-weight:500;${i === 0 ? 'background:rgba(63,208,255,.14);color:var(--acc)' : 'color:var(--dim)'}">${t}</div>`).join('')}<span class="dim" style="margin-left:auto">${icon('up', 18)}</span></div>
        <div class="row" style="gap:12px;padding:10px 12px">${slider('AF', '42', 42, 190)}${slider('SQUELCH', 'Off', 0, 190)}${slider('BALANCE', 'C', 50, 190)}<div class="btn" style="width:150px;height:30px">MUTE</div></div>`, '', 'background:var(--panel);border-top:1px solid var(--line)'),
    canvases: [['#c', { f0: 14.044e6, f1: 14.104e6, vfo: VFO, filt: USB, want: 8,
      parts: [{ kind: 'spec2d', h: 0.42, dbl: true, line: '#ffe08a', fillTop: 'rgba(255,190,60,0.42)', fillBot: 'rgba(255,190,60,0.02)' }, { kind: 'wf2d', h: 0.58, pal: 'desk', lo: -119, hi: -72 }] }]],
  }),

  // ---------------- portrait 384x832 ----------------
  P1: () => ({
    html: abs(0, 0, 384, 108, `<div style="padding:12px 16px 0">
        <div class="row" style="justify-content:space-between"><span class="row" style="gap:6px"><span class="tag vfoA">A</span><span class="tag acc">USB-D</span><span class="tag">3.0k</span><span class="tag">20 m</span></span><span class="dim">${icon('menu')}</span></div>
        <div class="row" style="justify-content:space-between;margin-top:8px">${fq(VFO, 40)}<div class="col" style="align-items:flex-end"><b style="font-size:17px;font-weight:500">S7</b><span class="dim" style="font-size:10px">−87 dBm</span></div></div>
        <div style="margin-top:8px">${meter(352, 5, 7.4, false)}</div></div>`, '', 'background:#070a12') +
      cv('c', 0, 108, 384, 492) +
      abs(0, 600, 384, 160, `<div class="col" style="gap:8px;padding:12px 14px">${slider('AF', '42', 42, 356)}${slider('LNA', '+20 dB', 55, 356)}
        <div class="row" style="gap:6px;margin-top:2px">${chip('AGC', 'Med', true, 'btn')}${chip('NR', 'NR2', true, 'btn')}${chip('NB', 'Off', false, 'btn')}${chip('SNB', 'On', true, 'btn')}</div></div>`, '', 'background:var(--panel)') +
      abs(0, 760, 384, 72, `<div class="row" style="justify-content:space-around;height:100%;padding-bottom:10px">${[['grid', 'Band'], ['mode', 'Mode'], ['filt', 'Filter'], ['dsp', 'DSP'], ['menu', 'More']].map(([ic, t], i) => `<div class="col" style="align-items:center;gap:3px;${i === 0 ? 'color:var(--acc)' : 'color:var(--dim)'}">${icon(ic, 22)}<span style="font-size:10.5px">${t}</span></div>`).join('')}</div>`, '', 'background:#080b13;border-top:1px solid var(--line)'),
    canvases: [['#c', SPEC.look3(14.0695e6, 14.0835e6, 0.36, { want: 4 })]],
  }),

  // P2 — portrait, frequency runs down the long side, waterfall flows sideways
  P2: () => {
    const f0 = 14.060e6, f1 = 14.104e6, Y = f => (f - f0) / (f1 - f0) * 832;
    let ticks = '';
    for (let f = 14.065e6; f < f1; f += 5000) ticks += abs(4, Y(f) - 7, 60, 14, `<span style="font-size:10px;color:rgba(230,240,255,.8);text-shadow:0 1px 4px #000">${(f / 1e6).toFixed(3)}</span>`);
    return {
      html: cv('c', 0, 0, 384, 832) + ticks +
        abs(84, Y(VFO) - 56, null, null, `<div class="row" style="gap:8px;padding:6px 12px"><span class="tag vfoA">A</span>${fq(VFO, 24)}<span class="tag acc">USB-D</span></div>`, 'glass') +
        abs(150, 14, 132, null, `<div class="row" style="justify-content:space-between;margin-bottom:4px"><b style="font-weight:500">S7</b><span class="dim" style="font-size:10px">−87 dBm</span></div>${meter(108, 5, 7.4, false)}`, 'glass', 'padding:8px 12px') +
        abs(296, 572, 66, null, `<div class="col" style="gap:8px">${['grid', 'filt', 'dsp', 'menu'].map(ic => `<div class="ico glass" style="width:58px;height:44px;border-radius:14px">${icon(ic)}</div>`).join('')}</div>`) +
        abs(70, 806, 220, null, `<span class="dim" style="font-size:10px;text-shadow:0 1px 4px #000">← older · frequency runs down the screen</span>`),
      canvases: [['#c', { f0, f1, vfo: VFO, filt: USB, rotate: true, axis: 'none',
        parts: [{ kind: 'spec2d', h: 0.3, line: '#d9ffb0', fillTop: 'rgba(80,230,120,0.5)', fillBot: 'rgba(80,230,120,0.03)', hi: -68 }, { kind: 'wf2d', h: 0.7, pal: 'x6100' }] }]],
    };
  },

  // P3 — portrait, display top, radio front panel bottom
  P3: () => ({
    html: cv('c', 0, 0, 384, 430) +
      abs(0, 430, 384, 402, `<div style="padding:14px 16px">
        <div class="row" style="justify-content:space-between"><span class="row" style="gap:6px"><span class="tag vfoA">A</span><span class="tag acc">USB-D</span><span class="tag">3.0k</span></span><span class="row dim" style="gap:4px;font-size:11px">B ${fq(VFOB, 13, 'dim')}</span></div>
        <div style="margin:8px 0 6px">${fq(VFO, 50, 'light')}</div>
        <div class="row" style="justify-content:space-between;margin-bottom:4px"><b style="font-size:16px;font-weight:500">S7</b><span class="dim">−87 dBm</span></div>${meter(352, 9, 7.4, true)}
        <div style="display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:12px">
        ${[['BAND', '20 m'], ['MODE', 'USB-D'], ['FILTER', '3.0k'], ['AGC', 'Med', 1], ['NR', 'NR2', 1], ['NB', 'Off'], ['AF', '42'], ['LNA', '+20 dB'], ['MORE', '···']].map(([l, v, on]) => `<div class="btn col ${on ? 'on' : ''}" style="height:58px;gap:3px;border-radius:15px"><span class="lbl" style="${on ? 'color:var(--acc)' : ''}">${l}</span><span style="font-size:18px">${v}</span></div>`).join('')}</div></div>`, '', 'background:linear-gradient(180deg,#0c1220,#070a12);border-top:1px solid var(--line)'),
    canvases: [['#c', { f0: 14.0715e6, f1: 14.0815e6, vfo: VFO, filt: USB, want: 4, parts: [{ kind: 'surface3d', h: 0.5, pal: 'desk', n: 44, back: 0.22 }, { kind: 'wf2d', h: 0.5, pal: 'desk', lo: -119, hi: -72 }] }]],
  }),

  // P4 — portrait, band sheet open over the waterfall
  P4: () => ({
    html: cv('c', 0, 0, 384, 832) +
      abs(12, 12, null, null, `<div class="row" style="gap:8px;padding:7px 12px"><span class="tag vfoA">A</span>${fq(VFO, 26)}<span class="tag acc">USB-D</span></div>`, 'glass') +
      abs(0, 0, 384, 832, '', '', 'background:linear-gradient(180deg,rgba(0,0,0,0) 30%,rgba(0,0,0,.45) 50%)') +
      abs(0, 384, 384, 448, `<div class="row" style="justify-content:center;padding-top:8px"><div style="width:36px;height:4px;border-radius:2px;background:rgba(255,255,255,.3)"></div></div>
        <div style="padding:10px 16px">
        <div class="row" style="justify-content:space-between"><span style="font-size:18px;font-weight:500">Band</span><span class="dim" style="font-size:11px">IARU Region 2</span></div>
        <div style="display:grid;grid-template-columns:repeat(4,1fr);gap:7px;margin-top:12px">${bandChips('20', null, 42).replace(/width:nullpx;/g, '')}
          <div class="btn" style="height:42px;font-size:13px">WWV</div><div class="btn" style="height:42px;font-size:13px">AIR</div><div class="btn" style="height:42px;font-size:13px">XVTR</div><div class="btn" style="height:42px;font-size:13px">MEM</div></div>
        <div class="lbl" style="margin:16px 0 7px">20 m band stack</div>
        <div class="col" style="gap:6px">${[[14.074e6, 'USB-D', '3.0k', 1], [14.205e6, 'USB', '2.7k'], [14.025e6, 'CW', '500']].map(([f, m, w, on]) => `<div class="btn ${on ? 'on' : ''}" style="justify-content:flex-start;gap:12px;height:42px;padding:0 14px">${fq(f, 17)}<span class="tag ${on ? 'acc' : ''}">${m}</span><span class="dim">${w}</span></div>`).join('')}</div></div>`, '', 'background:rgba(12,17,30,.96);border-radius:22px 22px 0 0;border-top:1px solid var(--line);box-shadow:0 -10px 30px rgba(0,0,0,.5)'),
    canvases: [['#c', SPEC.look3(14.0695e6, 14.0835e6, 0.3, { want: 4 })]],
  }),
});
