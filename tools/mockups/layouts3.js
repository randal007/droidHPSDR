// Round 2: layout 1 (Randal's pick, 2026-11-23) with the band plan bar and the
// band-segment alert, in four 3D styles; W5 shows the out-of-band alert.
'use strict';
const toast = (txt, sub, c) => abs(166, 78, 500, null,
  `<div class="row" style="justify-content:center"><div class="row glass" style="white-space:nowrap;gap:8px;padding:6px 14px;border-color:${c}88;box-shadow:0 0 18px ${c}55">
    <span style="width:9px;height:9px;border-radius:50%;background:${c};box-shadow:0 0 8px ${c}"></span>
    <b style="color:${c};font-weight:600;letter-spacing:.6px">${txt}</b><span class="dim" style="font-size:11px">${sub}</span></div></div>`);
const DIGI = toast('DIGITAL', '14.070 – 14.099 · FT8 · JS8 · PSK', '#2fd07a');
function l1With(top, extra = '', spec = {}) {
  const base = LAYOUTS.L1();
  return {
    html: base.html + extra,
    canvases: [['#c', { f0: 14.044e6, f1: 14.104e6, vfo: VFO, filt: USB, want: 7, bandplan: true,
      parts: [top, { kind: 'wf2d', h: 0.5, pal: 'x6100' }], ...spec }]],
  };
}
Object.assign(LAYOUTS, {
  // the picked look: X6100 look 3 stacked lines
  W1: () => l1With({ kind: 'stack3d', h: 0.5, line: 'x6100line', fog: true }, DIGI),
  // AetherSDR 3DSS: coloured curtains + white ridge lines, strong perspective
  W2: () => l1With({ kind: 'terrain', h: 0.5, style: 'aether', pal: 'aether' }, DIGI),
  // lit terrain in the X6100 colours: slope shading, glowing front ridge, no wire lines
  W3: () => l1With({ kind: 'terrain', h: 0.5, style: 'lit', pal: 'x6100', haze: 0.5, zCurve: 0.7 }, DIGI),
  // neon wire mesh: ridge lines plus lines back through time
  W4: () => l1With({ kind: 'terrain', h: 0.5, style: 'wire', pal: 'x6100line', n: 56, zCurve: 0.75 }, DIGI),
  // out-of-band alert, tuned just below 14.000
  W5: () => {
    const v = 13.9985e6, b = LAYOUTS.L1();
    return {
      html: b.html.replace(fq(VFO, 34), fq(v, 34)) + toast('OUT OF BAND', '20 m starts at 14.000', '#ff5050'),
      canvases: [['#c', { f0: 13.975e6, f1: 14.035e6, vfo: v, filt: USB, want: 7, bandplan: true, vfoCol: '#ff5050',
        parts: [{ kind: 'terrain', h: 0.5, style: 'aether', pal: 'aether' }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }] }]],
    };
  },
});
