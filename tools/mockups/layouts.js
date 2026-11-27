// droidHPSDR UI mock-up layouts. Landscape viewport 832x384 dp, portrait 384x832 dp
// (S26 Ultra at 3.75 px/dp = 3120x1440). Sizes below are dp.
'use strict';
const I = {
  menu: '<path d="M4 7h16M4 12h16M4 17h16"/>',
  cube: '<path d="M12 3l8 4.5v9L12 21l-8-4.5v-9z"/><path d="M12 12l8-4.5M12 12v9M12 12L4 7.5"/>',
  sliders: '<path d="M4 7h9M17 7h3M4 17h3M11 17h9"/><circle cx="15" cy="7" r="2"/><circle cx="9" cy="17" r="2"/>',
  spk: '<path d="M4 9.5v5h3.5L12 18V6L7.5 9.5z"/><path d="M15.5 9a4 4 0 0 1 0 6M18 6.5a7.5 7.5 0 0 1 0 11"/>',
  grid: '<rect x="4" y="4" width="6.5" height="6.5" rx="1.5"/><rect x="13.5" y="4" width="6.5" height="6.5" rx="1.5"/><rect x="4" y="13.5" width="6.5" height="6.5" rx="1.5"/><rect x="13.5" y="13.5" width="6.5" height="6.5" rx="1.5"/>',
  wave: '<path d="M2 12c2.5-7 4.5-7 7 0s4.5 7 7 0 4-5 6-2"/>',
  filt: '<path d="M3 18c3 0 3-12 6-12h6c3 0 3 12 6 12"/>',
  ant: '<path d="M12 21V10M12 10L6.5 3.5M12 10l5.5-6.5M9 21h6"/>',
  layers: '<path d="M12 4l9 4.5-9 4.5-9-4.5z"/><path d="M3 13l9 4.5 9-4.5"/>',
  mode: '<path d="M4 18V6l5 8 5-8v12"/><path d="M17 10h4M19 8v4"/>',
  dsp: '<path d="M3 12h3l2-6 3 12 3-9 2 3h5"/>',
  gear: '<circle cx="12" cy="12" r="3.2"/><path d="M12 2.8v3M12 18.2v3M2.8 12h3M18.2 12h3M5.5 5.5l2.1 2.1M16.4 16.4l2.1 2.1M5.5 18.5l2.1-2.1M16.4 7.6l2.1-2.1"/>',
  target: '<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="3"/><path d="M12 1v4M12 19v4M1 12h4M19 12h4"/>',
  lock: '<rect x="5" y="11" width="14" height="9" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>',
  swap: '<path d="M5 8h13l-3-3M19 16H6l3 3"/>',
  chev: '<path d="M9 6l6 6-6 6"/>',
  up: '<path d="M6 15l6-6 6 6"/>',
  dots: '<circle cx="8" cy="12" r="1.3"/><circle cx="12" cy="12" r="1.3"/><circle cx="16" cy="12" r="1.3"/>',
  pinch: '<path d="M5 5l5 5M5 5v4M5 5h4M19 19l-5-5M19 19v-4M19 19h-4"/>',
};
const icon = (n, s = 20, sw = 1.8) =>
  `<svg width="${s}" height="${s}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="${sw}" stroke-linecap="round" stroke-linejoin="round">${I[n]}</svg>`;
function fq(f, size, cls = '') {
  const s = (f / 1e6).toFixed(6), [m, r] = s.split('.');
  return `<span class="freq ${cls}" style="font-size:${size}px">${m}.${r.slice(0, 3)}<small>.${r.slice(3)}</small></span>`;
}
// S-meter: 15 segments (S1..S9, +10..+60), level in segments
function meter(w, h, level = 7.4, scale = true) {
  const n = 15, gap = 2, sw = (w - gap * (n - 1)) / n;
  let s = '';
  for (let i = 0; i < n; i++) {
    const f = Math.max(0, Math.min(1, level - i)), col = i < 9 ? '62,224,122' : '255,90,80';
    const bg = f > 0 ? `linear-gradient(90deg, rgba(${col},.95) ${f * 100}%, rgba(255,255,255,.08) ${f * 100}%)` : '';
    s += `<u style="width:${sw}px;height:${h}px;${bg ? 'background:' + bg : ''}"></u>`;
  }
  const sc = scale ? `<div class="mscale" style="width:${w}px;margin-top:3px"><span>S1</span><span>3</span><span>5</span><span>7</span><span>9</span><span style="color:#ff8a80">+20</span><span style="color:#ff8a80">+40</span><span style="color:#ff8a80">+60</span></div>` : '';
  return `<div><div class="meter">${s}</div>${sc}</div>`;
}
function arcMeter(w, val = 0.46) {
  const h = w * 0.56, cx = w / 2, cy = h * 0.95, r = w * 0.42;
  const pt = (a, rr) => [cx + rr * Math.cos(a), cy - rr * Math.sin(a)];
  const a0 = Math.PI * 0.86, a1 = Math.PI * 0.14, aS9 = a0 - (a0 - a1) * 0.6;
  const arc = (from, to, col, wdt) => { const [x0, y0] = pt(from, r), [x1, y1] = pt(to, r); return `<path d="M${x0} ${y0} A${r} ${r} 0 0 1 ${x1} ${y1}" stroke="${col}" stroke-width="${wdt}" fill="none" stroke-linecap="round"/>`; };
  let ticks = '';
  ['1', '3', '5', '7', '9', '+20', '+40', '+60'].forEach((t, i) => {
    const a = i < 5 ? a0 - (a0 - aS9) * (i / 4) : aS9 - (aS9 - a1) * ((i - 4) / 3), [tx, ty] = pt(a, r - 12);
    ticks += `<text x="${tx}" y="${ty + 3}" fill="${i > 4 ? '#ff8a80' : '#9aa6bb'}" font-size="8.5" text-anchor="middle">${t}</text>`;
  });
  const av = a0 - (a0 - a1) * val, [nx, ny] = pt(av, r + 3);
  return `<svg width="${w}" height="${h}">${arc(a0, aS9, 'rgba(62,224,122,.75)', 3)}${arc(aS9, a1, 'rgba(255,90,80,.8)', 3)}${ticks}
    <line x1="${cx}" y1="${cy}" x2="${nx}" y2="${ny}" stroke="#fff" stroke-width="1.6" stroke-linecap="round"/><circle cx="${cx}" cy="${cy}" r="3" fill="#fff"/></svg>`;
}
const chip = (lab, val, on = false, cls = 'glass') => `<div class="chip ${cls} ${on ? 'on' : ''}">${lab}<b>${val}</b></div>`;
const slider = (lab, val, pct, w) => `<div class="sl" style="width:${w}px"><i style="width:${pct}%"></i><span>${lab}</span><em>${val}</em></div>`;
const vslider = (lab, val, pct, h) => `<div class="vsl" style="height:${h}px"><i style="height:${pct}%"></i><span>${lab}</span><em>${val}</em></div>`;
const abs = (x, y, w, h, inner, cls = '', st = '') =>
  `<div class="a ${cls}" style="left:${x}px;top:${y}px;${w != null ? 'width:' + w + 'px;' : ''}${h != null ? 'height:' + h + 'px;' : ''}${st}">${inner}</div>`;
const cv = (id, x, y, w, h) => `<canvas id="${id}" style="left:${x}px;top:${y}px;width:${w}px;height:${h}px"></canvas>`;

const VFO = 14.074e6, VFOB = 14.0785e6, USB = [100, 3100];
// standard spectrum specs
const SPEC = {
  look3: (f0, f1, top = 0.46, extra = {}) => ({
    f0, f1, vfo: VFO, filt: USB, parts: [
      { kind: 'stack3d', h: top, line: 'x6100line', fog: true },
      { kind: 'wf2d', h: 1 - top, pal: 'x6100' }], ...extra }),
};

const LAYOUTS = {
  // 1 — Glass cockpit: X6100 look 3 full bleed, floating glass controls
  L1: () => ({
    html: cv('c', 0, 0, 832, 384) +
      abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px') +
      abs(266, 8, 300, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%">
          <span class="tag vfoA">A</span>${fq(VFO, 34)}
          <div class="col" style="gap:3px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div></div>`, 'glass') +
      abs(720, 12, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass">${icon('cube')}</div><div class="ico glass">${icon('menu')}</div></div>`) +
      abs(10, 328, 812, 46, `<div class="row" style="gap:7px">
          ${chip('BAND', '20 m')}${chip('AF', '42')}${chip('LNA', '+20 dB')}${chip('AGC', 'Med', true)}${chip('NR', 'NR2', true)}${chip('NB', 'Off')}${chip('SNB', 'On', true)}${chip('FILTER', '3.0k')}${chip('STEP', '10 Hz')}</div>`),
    canvases: [['#c', SPEC.look3(14.044e6, 14.104e6, 0.5, { want: 7 })]],
  }),
};
