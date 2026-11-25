// M4 round 1: where the receive-operating controls live on layout 1
// (VFO B, A/B, split, RIT, CTUN, lock, step, band stack, memories, AGC, meter).
'use strict';
const VB = 14.0785e6;
const meterCard = () => abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}`, 'glass', 'padding:9px 11px');
const topBtns = () => abs(720, 12, 100, 44, `<div class="row" style="gap:8px;justify-content:flex-end"><div class="ico glass" style="font-weight:600;font-size:13px">3D</div><div class="ico glass">${icon('menu')}</div></div>`);
const pill = (extra = '', w = 300, x = 266) => abs(x, 8, w, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%">
  <span class="tag vfoA">A</span>${fq(VFO, 34)}
  <div class="col" style="gap:3px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>${extra}</div>`, 'glass');
const chips = (list) => abs(10, 328, 812, 46, `<div class="row" style="gap:7px">${list.map(([l, v, on]) => chip(l, v, on)).join('')}</div>`);
const CHIPS = [['BAND', '20m'], ['AF', '70'], ['LNA', '+20 dB'], ['PHONE', 'Off'], ['FILTER', '3.0k'], ['STEP', '10 Hz'], ['SPAN', '48 kHz']];
const base = (spec = {}, kind = 'stack3d') => ({
  f0: 14.044e6, f1: 14.104e6, vfo: VFO, filt: [0, 3000], want: 7, bandplan: true,
  parts: [kind === 'spec2d' ? { kind: 'spec2d', h: 0.5, dbl: true, line: '#cfe9ff', fillTop: 'rgba(80,140,255,0.45)' }
    : { kind: 'stack3d', h: 0.5, line: 'x6100line', fog: true }, { kind: 'wf2d', h: 0.5, pal: 'x6100' }], ...spec,
});
const bigBtn = (t, on, w = null, sub = '') => `<div class="btn ${on ? 'on' : ''}" style="height:40px;${w ? 'width:' + w + 'px;' : 'flex:1;'}flex-direction:column;gap:1px">${t}${sub ? `<span style="font-size:9.5px;color:var(--dim)">${sub}</span>` : ''}</div>`;

Object.assign(LAYOUTS, {
  // A — tap the VFO pill: it drops open into the VFO panel
  V1: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) +
      abs(236, 8, 360, 250, `<div style="padding:12px 16px">
        <div class="row" style="gap:10px"><span class="tag vfoA">A</span>${fq(VFO, 32)}<span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>
        <div class="row dim" style="gap:10px;margin-top:8px"><span class="tag" style="background:none;border:1px solid var(--line);color:var(--dim)">B</span>${fq(VB, 18, 'dim')}<span class="tag">USB-D</span></div>
        <div class="row" style="gap:6px;margin-top:12px">${bigBtn('A›B')}${bigBtn('B›A')}${bigBtn('A⇄B')}${bigBtn('SPLIT', false, null, 'TX on B')}</div>
        <div class="row" style="gap:6px;margin-top:6px">${bigBtn('RIT', true, null, '+120 Hz')}${bigBtn('CTUN')}${bigBtn('LOCK')}${bigBtn('STEP', false, null, '10 Hz')}</div>
        <div class="dim" style="font-size:10px;margin-top:8px;text-align:center">RIT on: drag the passband to set the offset · tap outside to close</div></div>`, 'glass', 'border-radius:18px;background:rgba(9,13,24,.9)'),
    canvases: [['#c', base()]],
  }),

  // B — VFO B always shown, state as badges you tap; markers on the spectrum
  V2: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) +
      abs(206, 8, 420, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%">
        <span class="tag vfoA">A</span>${fq(VFO, 34)}
        <div class="col" style="gap:3px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>
        <div class="col" style="gap:2px;padding-left:10px;border-left:1px solid var(--line)"><span class="dim" style="font-size:10px">B · hold to swap</span>${fq(VB, 15, 'dim')}</div></div>`, 'glass') +
      abs(246, 74, 340, 26, `<div class="row" style="gap:6px;justify-content:center">
        <span class="tag amb">RIT +120</span><span class="tag" style="background:rgba(180,140,255,.18);color:#c9a8ff">SPLIT · TX B</span><span class="tag">CTUN</span><span class="tag">🔒 LOCK</span></div>`),
    canvases: [['#c', base({ labels: [{ f: VB, y: 0.08, t: 'B / TX', c: '#c9a8ff' }] })]],
  }),

  // C — the bottom row becomes pages you swipe: Audio · VFO · DSP · Band
  V3: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + pill() +
      abs(10, 306, 400, 18, `<div class="row" style="gap:14px;font-size:10.5px;letter-spacing:.6px"><span class="dim">AUDIO</span><b style="color:var(--acc)">VFO</b><span class="dim">DSP</span><span class="dim">BAND</span><span class="dim" style="margin-left:6px">‹ swipe ›</span></div>`, '', 'padding-left:4px') +
      chips([['B', '14.078.500'], ['A⇄B', 'swap'], ['SPLIT', 'Off'], ['RIT', '+120', true], ['CTUN', 'Off'], ['LOCK', 'Off'], ['STEP', '10 Hz']]),
    canvases: [['#c', base()]],
  }),

  // D — a VFO drawer from the right edge, like the DSP drawer picture
  V4: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + pill('', 300, 150) + chips(CHIPS.slice(0, 4)) +
      abs(0, 0, 532, 384, '', '', 'background:linear-gradient(90deg,rgba(0,0,0,0),rgba(0,0,0,.35))') +
      abs(532, 0, 300, 384, `<div style="padding:12px 14px">
        <div class="row" style="justify-content:space-between"><span style="font-size:17px;font-weight:500">VFO</span><span class="dim">${icon('chev', 18)}</span></div>
        <div class="row" style="gap:8px;margin-top:10px"><span class="tag vfoA">A</span>${fq(VFO, 22)}</div>
        <div class="row dim" style="gap:8px;margin-top:6px"><span class="tag">B</span>${fq(VB, 18, 'dim')}</div>
        <div class="row" style="gap:6px;margin-top:10px">${bigBtn('A›B')}${bigBtn('B›A')}${bigBtn('A⇄B')}</div>
        <div class="lbl" style="margin:10px 0 5px">RIT</div>
        <div class="row" style="gap:6px">${bigBtn('Off')}${bigBtn('On', true)}<div class="btn" style="flex:2;height:40px;font-size:15px">+120 Hz</div>${bigBtn('Clear')}</div>
        <div class="row" style="gap:6px;margin-top:8px">${bigBtn('SPLIT')}${bigBtn('CTUN')}${bigBtn('LOCK')}</div>
        <div class="dim" style="font-size:10px;margin-top:8px">Swipe in from the right edge · RIT: drag the passband</div></div>`, 'glass', 'border-radius:18px 0 0 18px;background:rgba(9,12,22,.86)'),
    canvases: [['#c', base()]],
  }),

  // E — band sheet: bands, this band's stack, memories
  V5: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + pill() +
      abs(0, 0, 832, 384, '', '', 'background:linear-gradient(180deg,rgba(0,0,0,0) 30%,rgba(0,0,0,.45) 55%)') +
      abs(116, 150, 600, 234, `<div style="padding:12px 16px">
        <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">Band</span><span class="dim" style="font-size:11px">hold a memory to store here</span></div>
        <div class="row" style="gap:5px;margin-top:8px">${['160', '80', '60', '40', '30', '20', '17', '15', '12', '10', '6', 'GEN', 'WWV'].map(b => `<div class="btn ${b === '20' ? 'on' : ''}" style="flex:1;height:32px;font-size:12.5px">${b}</div>`).join('')}</div>
        <div class="lbl" style="margin:10px 0 5px">20 m band stack</div>
        <div class="row" style="gap:6px">${[[14.074e6, 'USB-D', 1], [14.025e6, 'CW'], [14.205e6, 'USB'], [14.078e6, 'USB-D'], [14.230e6, 'USB']].map(([f, m, on]) => `<div class="btn col ${on ? 'on' : ''}" style="flex:1;height:44px;gap:1px">${fq(f, 13)}<span style="font-size:9.5px;color:var(--dim)">${m}</span></div>`).join('')}</div>
        <div class="lbl" style="margin:10px 0 5px">Memories</div>
        <div class="row" style="gap:5px">${['7.074', '14.074', '3.573', '10.136', '—', '—', '—', '—', '—', '—'].map((f, i) => `<div class="btn col" style="flex:1;height:40px;gap:0"><span style="font-size:9px;color:var(--dim)">M${i}</span><span style="font-size:11.5px">${f}</span></div>`).join('')}</div></div>`,
      'glass', 'border-radius:18px 18px 0 0;background:rgba(10,14,26,.95)'),
    canvases: [['#c', base()]],
  }),

  // F — AGC on the spectrum: drag the AGC line (deskHPSDR draws it on the panadapter)
  V6: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + pill() + chips([['AGC', 'Med', true], ...CHIPS.slice(0, 6)]) +
      abs(0, 120, 832, 2, '', '', 'background:repeating-linear-gradient(90deg,#ffb341 0 10px,transparent 10px 18px);opacity:.9') +
      abs(600, 104, 150, 30, `<div class="row" style="gap:6px;padding:0 10px;height:100%;font-size:11px"><b style="color:var(--amb)">AGC</b><span>−95 dBm</span><span class="dim">drag ↕</span></div>`, 'glass', 'border-radius:15px;border-color:rgba(255,179,65,.5)') +
      `<div class="finger" style="left:690px;top:98px"></div>` +
      abs(200, 140, 420, 44, `<div class="seg" style="height:100%;align-items:center">${['Off', 'Long', 'Slow', 'Med', 'Fast', 'Auto'].map(t => `<b class="${t === 'Med' ? 'on' : ''}">${t}</b>`).join('')}</div>`, 'glass', 'border-radius:12px;padding:3px'),
    canvases: [['#c', base({}, 'spec2d')]],
  }),

  // G — meter choices: digital bar (now), analog needle, both with peak/average
  V7: () => ({
    html: cv('c', 0, 0, 832, 384) + topBtns() + pill() + chips(CHIPS) +
      abs(10, 10, 190, null, `<div class="col" style="align-items:center">${arcMeter(170, 0.42)}<div class="row" style="justify-content:space-between;width:100%;margin-top:-2px"><b style="font-size:15px;font-weight:500">S7</b><span class="dim">−87 dBm</span></div>
        <div class="seg" style="width:100%;margin-top:6px"><b>Peak</b><b class="on">Avg</b></div></div>`, 'glass', 'padding:8px 12px'),
    canvases: [['#c', base()]],
  }),

  // H — RIT and split shown on the spectrum, set by dragging
  V8: () => {
    const f0 = 14.0725e6, f1 = 14.0815e6, X = f => (f - f0) / (f1 - f0) * 832;
    return {
      html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + pill(`<span class="tag amb" style="margin-left:4px">RIT +350</span>`, 360, 236) + chips(CHIPS) +
        `<div class="finger" style="left:${X(VFO + 350 + 1500) - 23}px;top:150px"></div>` +
        `<div class="tip" style="left:${X(VFO + 350 + 1500) + 28}px;top:136px"><b style="color:var(--amb)">RIT +350 Hz</b><br><span class="dim">hold the passband, drag</span></div>` +
        abs(X(VB) - 30, 80, 60, 20, `<div class="row" style="justify-content:center"><span class="tag" style="background:rgba(180,140,255,.22);color:#d4b8ff">TX · B</span></div>`),
      canvases: [['#c', { ...base(), f0, f1, want: 6, filt: [350, 3350], filtCol: 'rgba(255,179,65,0.12)', filtEdge: 'rgba(255,179,65,0.55)' }]],
    };
  },
});

// M4 round 2 (Randal: "a mix of A and B, and E"): B's pill (VFO B always
// shown, state badges) that drops open into A's panel when tapped.
const abPill = (badgesInside) => abs(196, 8, 440, badgesInside ? 84 : 62, `
  <div class="row" style="gap:10px;justify-content:center;height:62px">
    <span class="tag vfoA">A</span>${fq(VFO, 34)}
    <div class="col" style="gap:3px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>
    <div class="col" style="gap:2px;padding-left:10px;border-left:1px solid var(--line)"><span class="dim" style="font-size:10px">B · hold to swap</span>${fq(VB, 15, 'dim')}</div></div>
  ${badgesInside ? `<div class="row" style="gap:6px;justify-content:center;margin-top:-4px"><span class="tag amb">RIT +120</span><span class="tag" style="background:rgba(180,140,255,.18);color:#c9a8ff">SPLIT · TX B</span></div>` : ''}`, 'glass');
Object.assign(LAYOUTS, {
  // closed: badges float under the pill, only the ones that are on
  X1: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) + abPill(false) +
      abs(246, 74, 340, 26, `<div class="row" style="gap:6px;justify-content:center"><span class="tag amb">RIT +120</span><span class="tag" style="background:rgba(180,140,255,.18);color:#c9a8ff">SPLIT · TX B</span></div>`),
    canvases: [['#c', base({ labels: [{ f: VB, y: 0.08, t: 'B / TX', c: '#c9a8ff' }] })]],
  }),
  // closed, other way: the badges live inside the pill, under the numbers
  X2: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) + abPill(true),
    canvases: [['#c', base({ labels: [{ f: VB, y: 0.12, t: 'B / TX', c: '#c9a8ff' }] })]],
  }),
  // open: tap the pill and it drops open into the full VFO panel
  X3: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) +
      abs(196, 8, 440, 268, `<div style="padding:12px 16px">
        <div class="row" style="gap:10px"><span class="tag vfoA">A</span>${fq(VFO, 32)}<span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>
        <div class="row" style="gap:10px;margin-top:8px"><span class="tag" style="background:none;border:1px solid var(--line);color:var(--dim)">B</span>${fq(VB, 20, 'dim')}<span class="tag">USB-D</span><span class="dim" style="font-size:10px;margin-left:auto">tap B to make it active</span></div>
        <div class="row" style="gap:6px;margin-top:12px">${bigBtn('A›B')}${bigBtn('B›A')}${bigBtn('A⇄B')}${bigBtn('SPLIT', true, null, 'TX on B')}</div>
        <div class="row" style="gap:6px;margin-top:6px">${bigBtn('RIT', true, null, '+120 Hz')}${bigBtn('RIT 0')}${bigBtn('CTUN')}${bigBtn('LOCK')}${bigBtn('STEP', false, null, '10 Hz')}</div>
        <div class="dim" style="font-size:10px;margin-top:8px;text-align:center">RIT: drag the passband · tap outside to close · the badges show what's on</div></div>`, 'glass', 'border-radius:18px;background:rgba(9,13,24,.92)'),
    canvases: [['#c', base()]],
  }),
});

// M4 round 3 (Randal: "I like 1 but not the hold-to-swap text"): option 1, B labelled only.
Object.assign(LAYOUTS, {
  X4: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) +
      abs(216, 8, 400, 62, `<div class="row" style="gap:10px;justify-content:center;height:100%">
        <span class="tag vfoA">A</span>${fq(VFO, 34)}
        <div class="col" style="gap:3px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>
        <div class="row" style="gap:6px;padding-left:10px;border-left:1px solid var(--line)"><span class="tag" style="background:none;border:1px solid var(--line);color:var(--dim)">B</span>${fq(VB, 15, 'dim')}</div></div>`, 'glass') +
      abs(246, 74, 340, 26, `<div class="row" style="gap:6px;justify-content:center"><span class="tag amb">RIT +120</span><span class="tag" style="background:rgba(180,140,255,.18);color:#c9a8ff">SPLIT · TX B</span></div>`),
    canvases: [['#c', base({ labels: [{ f: VB, y: 0.08, t: 'B / TX', c: '#c9a8ff' }] })]],
  }),
});

// M4 round 4 (Randal: "put VFO B under VFO A, small; the pill is too big because of it")
Object.assign(LAYOUTS, {
  X5: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) +
      abs(286, 8, 260, 74, `<div class="row" style="gap:10px;justify-content:center;height:100%">
        <div class="col" style="gap:3px;align-items:flex-start">
          <div class="row" style="gap:8px"><span class="tag vfoA">A</span>${fq(VFO, 32)}</div>
          <div class="row dim" style="gap:6px;padding-left:2px"><span style="font-size:10px;font-weight:700;color:var(--dim);width:24px;text-align:center">B</span>${fq(VB, 13, 'dim')}</div>
        </div>
        <div class="col" style="gap:3px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div></div>`, 'glass') +
      abs(246, 86, 340, 26, `<div class="row" style="gap:6px;justify-content:center"><span class="tag amb">RIT +120</span><span class="tag" style="background:rgba(180,140,255,.18);color:#c9a8ff">SPLIT · TX B</span></div>`),
    canvases: [['#c', base({ labels: [{ f: VB, y: 0.08, t: 'B / TX', c: '#c9a8ff' }] })]],
  }),
});

// M4 round 5 (Randal: the indicators under the pill should go somewhere better)
const pillB = (aExtra = '', bLine = null, aTag = '<span class="tag vfoA">A</span>', w = 260, x = 286, bottom = '') =>
  abs(x, 8, w, 74, `<div class="row" style="gap:10px;justify-content:center;height:100%">
    <div class="col" style="gap:3px;align-items:flex-start">
      <div class="row" style="gap:8px">${aTag}${fq(VFO, 32)}${aExtra}</div>
      ${bLine ?? `<div class="row dim" style="gap:6px;padding-left:2px"><span style="font-size:10px;font-weight:700;width:24px;text-align:center">B</span>${fq(VB, 13, 'dim')}</div>`}
    </div>
    <div class="col" style="gap:3px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div></div>${bottom}`, 'glass');
Object.assign(LAYOUTS, {
  // 1: built into what is already there - RIT after A's digits, split turns the B line purple, lock replaces the A tag
  Y1: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) +
      pillB(`<span style="font-size:11px;font-weight:700;color:var(--amb);align-self:flex-start;margin-top:4px">+120</span>`,
        `<div class="row" style="gap:6px;padding-left:2px"><span style="font-size:9.5px;font-weight:700;color:#c9a8ff;width:24px;text-align:center">TX B</span>${fq(VB, 13).replace('class="freq', 'style="color:#c9a8ff" class="freq')}</div>`,
        `<span class="tag" style="background:var(--amb);color:#1a1000">🔒</span>`, 290, 271),
    canvases: [['#c', base({ labels: [{ f: VB, y: 0.08, t: 'TX', c: '#c9a8ff' }] })]],
  }),
  // 2: on the spectrum itself - RIT shown at the passband, split as the TX marker, lock on the VFO line
  Y2: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) + pillB() +
      abs(426, 128, 70, 20, `<span class="tag amb">RIT +120</span>`) +
      abs(398, 100, 20, 20, `<span style="font-size:13px">🔒</span>`),
    canvases: [['#c', base({ labels: [{ f: VB, y: 0.46, t: 'TX · B', c: '#c9a8ff', drop: 40 }] })]],
  }),
  // 3: in the meter card, under the scale
  Y3: () => ({
    html: cv('c', 0, 0, 832, 384) + topBtns() + chips(CHIPS) + pillB() +
      abs(10, 10, 168, null, `<div class="row" style="justify-content:space-between;margin-bottom:6px"><span style="font-size:20px;font-weight:500">S7</span><span class="dim" style="font-size:11px">−87 dBm</span></div>${meter(146, 9, 7.4, true)}
        <div class="row" style="gap:4px;margin-top:7px"><span class="tag amb">RIT +120</span><span class="tag" style="background:rgba(180,140,255,.18);color:#c9a8ff">SPLIT</span><span class="tag">🔒</span></div>`, 'glass', 'padding:9px 11px'),
    canvases: [['#c', base()]],
  }),
  // 4: small coloured icons along the pill's bottom edge, the pill keeps its size
  Y4: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) +
      pillB('', null, undefined, 260, 286,
        `<div class="row" style="position:absolute;left:0;right:0;bottom:-9px;justify-content:center;gap:5px">
          ${[['R+120', 'var(--amb)', '#1a1000'], ['SPLIT', '#b48cff', '#140a26'], ['🔒', '#8b97ad', '#0a0f1a']].map(([t, bg, fg]) => `<span style="font-size:9px;font-weight:800;padding:1px 6px;border-radius:7px;background:${bg};color:${fg}">${t}</span>`).join('')}</div>`),
    canvases: [['#c', base()]],
  }),
});

// M4 round 6 (Randal: indicators must not cover the 3D view)
const pB = () => pillB();
const tagsHTML = (sz = 10.5) => `<span class="tag amb" style="font-size:${sz}px">RIT +120</span><span class="tag" style="background:rgba(180,140,255,.2);color:#c9a8ff;font-size:${sz}px">SPLIT</span><span class="tag" style="font-size:${sz}px">🔒 LOCK</span>`;
Object.assign(LAYOUTS, {
  // 1: one status chip at the end of the bottom row (over the waterfall)
  Z1: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + pB() +
      abs(10, 328, 812, 46, `<div class="row" style="gap:7px">${CHIPS.slice(0, 6).map(([l, v]) => chip(l, v)).join('')}
        <div class="chip glass" style="min-width:150px;border-color:rgba(255,179,65,.45)"><span>VFO</span><div class="row" style="gap:4px"><b style="color:var(--amb);font-size:13px">RIT+120</b><b style="color:#c9a8ff;font-size:13px">SPL</b><b style="font-size:13px">🔒</b></div></div></div>`),
    canvases: [['#c', base()]],
  }),
  // 2: in the dark frequency strip between 3D and waterfall, right end
  Z2: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + pB() + chips(CHIPS) +
      abs(596, 184, 236, 16, `<div class="row" style="gap:5px;justify-content:flex-end;height:100%;padding-right:8px">${tagsHTML(9)}</div>`, '', 'background:rgba(2,5,12,.95)'),
    canvases: [['#c', base({ f1: 14.098e6 })]],
  }),
  // 3: top-right corner of the waterfall, just under the band bar
  Z3: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + pB() + chips(CHIPS) +
      abs(600, 210, 222, 26, `<div class="row" style="gap:5px;justify-content:flex-end">${tagsHTML()}</div>`),
    canvases: [['#c', base()]],
  }),
  // 4: shown in the waterfall at their frequencies: RIT shifts the passband there, TX on B is a purple line
  Z4: () => ({
    html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + pB() + chips(CHIPS) +
      abs(440, 236, 70, 20, `<span class="tag amb">RIT +120</span>`) +
      abs(Math.round((VB - 14.044e6) / 60e3 * 832) + 6, 262, 60, 20, `<span class="tag" style="background:rgba(180,140,255,.25);color:#d4b8ff">TX · B</span>`) +
      abs(Math.round((VB - 14.044e6) / 60e3 * 832), 200, 2, 184, '', '', 'background:#b48cff;opacity:.85') +
      abs(14, 236, 40, 20, `<span style="font-size:14px">🔒</span>`),
    canvases: [['#c', base()]],
  }),
});

// M4 round 7 (Randal: indicators in the top pill, wider not taller, a bit more transparent)
const pillWide = (alpha) => abs(246, 8, 340, 74, `<div class="row" style="gap:10px;justify-content:center;height:100%">
    <div class="col" style="gap:3px;align-items:flex-start">
      <div class="row" style="gap:8px"><span class="tag vfoA">A</span>${fq(VFO, 32)}</div>
      <div class="row dim" style="gap:6px;padding-left:2px"><span style="font-size:10px;font-weight:700;width:24px;text-align:center">B</span>${fq(VB, 13, 'dim')}</div>
    </div>
    <div class="col" style="gap:3px"><span class="tag acc">USB-D</span><span class="tag">3.0k</span></div>
    <div class="col" style="gap:2px;padding-left:9px;border-left:1px solid var(--line);align-items:flex-start">
      <span style="font-size:10px;font-weight:700;color:var(--amb)">RIT +120</span>
      <span style="font-size:10px;font-weight:700;color:#c9a8ff">SPLIT · TX B</span>
      <span style="font-size:10px;font-weight:700;color:var(--dim)">🔒 LOCK</span>
    </div></div>`, 'glass', `background:rgba(10,16,30,${alpha});backdrop-filter:blur(10px)`);
Object.assign(LAYOUTS, {
  K1: () => ({ html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) + pillWide(0.40), canvases: [['#c', base()]] }),
  K2: () => ({ html: cv('c', 0, 0, 832, 384) + meterCard() + topBtns() + chips(CHIPS) + pillWide(0.28), canvases: [['#c', base()]] }),
});
