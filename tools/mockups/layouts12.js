// M7 step 2 (the TX audio chain), round 1, 2026-11-26: deskHPSDR's TX menu
// processing (speech processor, leveler, CFC, CESSB, phase rotator, DEXP),
// the TX EQ, VOX and the TX monitor. All kept per mode, as deskHPSDR does.
// Three ways to place them (N1-N3), then the detail pages (N4-N6). Pictures only.
'use strict';
(() => {
const H = window.M7H;
const { lbl, seg, tog, sl, note, dim, RED } = H;
const glassPanel = 'border-radius:16px;background:rgba(10,14,26,.94)';
const CH2 = [['BAND', '20m'], ['AF', '73'], ['NR', 'NR2', true], ['FILTER', '2.8k'], ['PROC', '4 dB', true], ['VOX', 'Off'], ['MON', 'Off'], ['TX', '20 %']];
const btn = (t, w = 'flex:1', extra = '') => `<div class="btn" style="${w};height:30px;font-size:11.5px;${extra}">${t}</div>`;
const togS = (t, on, sub = '') => `<div class="tog ${on ? 'on' : ''}" style="flex:1;height:32px"><span style="font-size:12px">${t}${sub ? `<span class="dim" style="font-size:9.5px;margin-left:6px">${sub}</span>` : ''}</span><s></s></div>`;

// a level bar for one stage of the chain (vertical), value 0..1
const stage = (name, on, lvl, sub) => `<div class="col" style="align-items:center;gap:3px;width:64px">
  <div style="position:relative;width:22px;height:52px;border-radius:6px;background:rgba(255,255,255,.07);overflow:hidden">
    <div style="position:absolute;left:0;right:0;bottom:0;height:${lvl * 100}%;background:linear-gradient(0deg,rgba(62,224,122,.55),${lvl > .85 ? 'rgba(255,77,77,.95)' : 'rgba(255,179,65,.85)'})"></div>
    <div style="position:absolute;left:0;right:0;top:${(1 - .9) * 100}%;height:1px;background:rgba(255,255,255,.45)"></div></div>
  <div class="btn ${on ? 'on' : ''}" style="width:60px;height:26px;font-size:10.5px">${name}</div>
  <span class="dim" style="font-size:9px">${sub}</span></div>`;
const arrow = () => `<span class="dim" style="font-size:14px;margin-top:20px">›</span>`;

// the CFC graph: compression per band (amber) and the post-EQ (cyan), drag points
function cfcSvg(w = 600, h = 170) {
  const fr = [50, 150, 300, 500, 750, 1250, 1750, 2300, 2800, 3100];
  const comp = [0, 0, 3, 3, 3, 6, 6, 6, 9, 9], post = [0, 0, 0, 1, 2, 2, 1, 0, -2, -4];
  const gx = f => 40 + Math.log10(f / 40) / Math.log10(4000 / 40) * (w - 60), gy = d => h / 2 - 12 - d * 6;
  const line = (v, c) => { const p = fr.map((f, i) => [gx(f), gy(v[i])]); return `<path d="M${p.map(q => q.join(',')).join(' L')}" fill="none" stroke="${c}" stroke-width="2.4"/>` + p.map(([x, y]) => `<circle cx="${x}" cy="${y}" r="6" fill="#0b1220" stroke="${c}" stroke-width="2"/>`).join(''); };
  return `<svg width="${w}" height="${h}" style="display:block">
    ${[-10, 0, 10].map(d => `<line x1="40" x2="${w - 20}" y1="${gy(d)}" y2="${gy(d)}" stroke="rgba(255,255,255,${d ? .07 : .2})"/><text x="6" y="${gy(d) + 4}" fill="#8b97ad" font-size="10">${d > 0 ? '+' : ''}${d} dB</text>`).join('')}
    ${[100, 300, 1000, 3000].map(f => `<text x="${gx(f) - 10}" y="${h - 4}" fill="#8b97ad" font-size="10">${f >= 1000 ? f / 1000 + 'k' : f}</text>`).join('')}
    ${line(comp, '#ffb341')}${line(post, '#3fd0ff')}
    <text x="${w - 150}" y="18" fill="#ffb341" font-size="11">● compression</text><text x="${w - 150}" y="34" fill="#3fd0ff" font-size="11">● post EQ</text>
  </svg>`;
}

// the TX EQ graph (the RX EQ's look from M5, orange for transmit)
function eqSvg(w = 620, h = 175) {
  const fr = [70, 150, 300, 500, 1000, 1500, 2000, 2500, 3000, 3500];
  const g = [-9, -6, -6, -9, 0, 0, 3, 3, 2, 0];
  const gx = f => 40 + Math.log10(f / 50) / Math.log10(4500 / 50) * (w - 60), gy = d => h / 2 - 10 - d * 3.4;
  const pts = fr.map((f, i) => [gx(f), gy(g[i])]);
  const path = 'M' + pts.map(p => p.join(',')).join(' L');
  return `<svg width="${w}" height="${h}" style="display:block">
    ${[-20, -10, 0, 10, 20].map(d => `<line x1="40" x2="${w - 20}" y1="${gy(d)}" y2="${gy(d)}" stroke="rgba(255,255,255,${d ? .07 : .2})"/><text x="8" y="${gy(d) + 4}" fill="#8b97ad" font-size="10">${d > 0 ? '+' : ''}${d}</text>`).join('')}
    ${[100, 300, 1000, 3000].map(f => `<text x="${gx(f) - 10}" y="${h - 4}" fill="#8b97ad" font-size="10">${f >= 1000 ? f / 1000 + 'k' : f}</text>`).join('')}
    <path d="${path} L${gx(3500)},${gy(0)} L${gx(70)},${gy(0)} Z" fill="rgba(255,140,60,.12)"/>
    <path d="${path}" fill="none" stroke="#ff9a4a" stroke-width="2.4"/>
    ${pts.map(([x, y], i) => `<circle cx="${x}" cy="${y}" r="${i === 6 ? 9 : 6}" fill="${i === 6 ? '#ff9a4a' : '#0b1220'}" stroke="#ff9a4a" stroke-width="2"/>`).join('')}
  </svg>`;
}

// a mic level bar with markers you drag: VOX threshold and the noise gate trigger
const micBar = (w = 560) => `<div style="position:relative;height:46px;margin-top:6px">
  <div style="position:absolute;left:0;top:14px;width:${w}px;height:16px;border-radius:8px;background:rgba(255,255,255,.07);overflow:hidden">
    <div style="width:${.62 * w}px;height:100%;background:linear-gradient(90deg,rgba(63,208,255,.35),rgba(63,208,255,.85))"></div></div>
  <div style="position:absolute;left:${.38 * w}px;top:6px;width:3px;height:32px;background:#ffb341;border-radius:2px"></div>
  <div style="position:absolute;left:${.38 * w - 30}px;top:-8px;font-size:10px;color:#ffb341">VOX −28 dB</div>
  <div style="position:absolute;left:${.22 * w}px;top:6px;width:3px;height:32px;background:#b48cff;border-radius:2px"></div>
  <div style="position:absolute;left:${.22 * w - 30}px;top:36px;font-size:10px;color:#b48cff">gate −42 dB</div>
  <div class="row" style="position:absolute;left:0;top:32px;width:${w}px;justify-content:space-between;font-size:8.5px;color:var(--dim)"><span>−60</span><span>−40</span><span>−20</span><span>0 dB</span></div></div>`;

Object.assign(LAYOUTS, {
  // N1 — A: a TX audio drawer from the right edge, like the receive Noise drawer (M5 pick D1)
  N1: () => ({
    html: cv('c', 0, 0, 832, 384) + H.meterCard() + H.pill() + H.chipRow('rx', CH2) +
      abs(0, 0, 512, 384, '', '', 'background:linear-gradient(90deg,rgba(0,0,0,0),rgba(0,0,0,.4))') +
      abs(512, 0, 320, 384, `<div style="padding:11px 14px">
        <div class="row" style="justify-content:space-between"><span style="font-size:17px;font-weight:500">TX audio</span><span class="dim" style="font-size:10px">LSB · kept per mode</span></div>
        <div class="row" style="gap:6px;margin-top:8px">${togS('Speech proc', true)}</div>${sl('LEVEL', '4 dB', 25)}
        <div class="row" style="gap:6px;margin-top:6px">${togS('Leveler', true)}</div>${sl('GAIN', '5 dB', 30)}
        <div class="row" style="gap:6px;margin-top:6px">${togS('CESSB', true)}${togS('Phase rot.', false)}</div>
        <div class="row" style="gap:6px;margin-top:6px">${togS('CFC', false)}${togS('TX EQ', true)}</div>
        <div class="row" style="gap:6px;margin-top:6px">${togS('Noise gate', false)}${togS('Monitor', false)}</div>
        <div class="row" style="gap:6px;margin-top:7px">${btn('CFC ›')}${btn('EQ ›')}${btn('Gate / VOX ›')}</div>
        </div>`, 'glass', 'border-radius:18px 0 0 18px;background:rgba(9,12,22,.92)') +
      note(20, 120, 330, '<b style="color:#ffd27a">A: TX audio drawer</b><br><span class="dim">from the PROC chip (or ☰ → TX audio): every switch of the chain on one panel, the level sliders under the two you change most; the › buttons open the graphs. New chips PROC, VOX, MON (editable like the others).</span>'),
    canvases: [['#c', H.rxSpec()]],
  }),

  // N2 — B: the chain as a strip while transmitting, each stage with its own live level (WDSP's TX meters)
  N2: () => ({
    html: cv('c', 0, 0, 832, 384) + abs(10, 10, 236, null, H.txMeter(236), 'glass', `padding:9px 11px;border-color:rgba(${RED},.5)`) +
      H.pill(true) + H.tBtns() +
      abs(66, 190, 700, 132, `<div style="padding:7px 12px">
        <div class="row" style="justify-content:space-between"><span style="font-size:13px;font-weight:500">TX chain · LSB</span><span class="dim" style="font-size:10px">tap a stage: on/off · hold: its settings · white line = where it should peak</span></div>
        <div class="row" style="gap:2px;margin-top:4px;align-items:flex-start;justify-content:center">
        ${stage('MIC', true, .72, '−6 dB')}${arrow()}${stage('EQ', true, .70, 'on')}${arrow()}${stage('LEVELER', true, .82, '+5 dB')}${arrow()}${stage('CFC', false, .80, 'off')}${arrow()}${stage('PROC', true, .88, '4 dB')}${arrow()}${stage('CESSB', true, .86, 'on')}${arrow()}${stage('ALC', true, .90, '−1 dB')}${arrow()}${stage('OUT', true, .9, '0.9 W')}
        </div></div>`, 'glass', 'background:rgba(14,8,12,.86)') +
      H.chipRow('tx', CH2, '1:41') +
      note(300, 96, 400, '<b style="color:#ffd27a">B: the chain while you talk</b><br><span class="dim">opens from the PROC chip during transmit, over the stopped waterfall: each stage of deskHPSDR\'s TX chain with its real level (WDSP meters), so you set mic gain, leveler and processor by watching where they peak.</span>'),
    canvases: [['#c', H.txSpec(0.5)]],
  }),

  // N3 — C: tabs on the TX page (☰ → TX), deskHPSDR's TX menu sections
  N3: () => ({
    html: cv('c', 0, 0, 832, 384) + H.panel('TX', "deskHPSDR's TX menu · LSB (processing is kept per mode)", `
      <div class="row" style="gap:6px;margin-top:8px">${seg(['Basic', 'Processing', 'CFC', 'Gate', 'EQ', 'VOX/Mon', 'Profiles'], 'Processing', 30)}</div>
      <div class="row" style="gap:16px;margin-top:8px;align-items:flex-start">
        <div style="flex:1">${lbl('SPEECH PROCESSOR')}<div class="row">${tog('Compressor', true)}</div>${sl('LEVEL', '4 dB', 25, 300)}
          ${lbl('LEVELER', '10px 0 4px')}<div class="row">${tog('Leveler', true)}</div>${sl('GAIN', '5 dB', 30, 300)}${sl('DECAY', '500 ms', 50, 300)}</div>
        <div style="flex:1">${lbl('MORE')}<div class="row" style="gap:6px">${tog('CESSB', true, 'auto')}</div>
          <div class="row" style="gap:6px;margin-top:6px">${tog('Phase rotator', false)}</div>${sl('STAGES', '8', 40, 300)}${sl('CORNER', '338 Hz', 30, 300)}
          ${lbl('MIC PREAMP', '10px 0 4px')}<div class="row">${tog('Local mic preamp', false, '+10 dB')}</div></div>
      </div>`, 64, 704) +
      note(270, 330, 300, '<b style="color:#ffd27a">C: tabs on the TX page</b> <span class="dim">· like the Display page (M5)</span>'),
    canvases: [['#c', H.rxSpec()]],
  }),

  // N4 — the CFC (continuous frequency compressor): two curves to drag
  N4: () => ({
    html: cv('c', 0, 0, 832, 384) + dim() + abs(86, 16, 660, 352, `<div style="padding:12px 16px">
      <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">CFC · multiband compressor</span>
        <div class="row" style="gap:7px">${togS('Pre-CFC', true)}${togS('Post EQ', true)}<div class="btn" style="width:70px;height:32px">Reset</div></div></div>
      ${cfcSvg(628, 200)}
      <div class="row" style="gap:12px;margin-top:4px">${slider('PRE COMP', '3 dB', 30, 300)}${slider('POST GAIN', '−9 dB', 40, 300)}</div>
      <div class="dim" style="font-size:10px;margin-top:6px">drag a point ↕ level, ↔ frequency · amber: how hard each band is compressed · cyan: the EQ after it</div>
      </div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.96)'),
    canvases: [['#c', H.rxSpec()]],
  }),

  // N5 — the TX EQ: the RX EQ graph from M5, in transmit orange
  N5: () => ({
    html: cv('c', 0, 0, 832, 384) + dim() + abs(86, 40, 660, 310, `<div style="padding:12px 16px">
      <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">TX equalizer · LSB</span>
        <div class="row" style="gap:7px">${togS('Enable', true)}${togS('Ctfmode', false)}<div class="btn" style="width:70px;height:32px">Flat</div></div></div>
      ${eqSvg(628, 200)}
      <div class="dim" style="font-size:10px;margin-top:6px">drag a point ↕ gain, ↔ frequency · the same graph as the RX equalizer · kept per mode</div>
      </div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.96)'),
    canvases: [['#c', H.rxSpec()]],
  }),

  // N6 — VOX and the noise gate (DEXP) on one page: a live mic bar with both thresholds to drag
  N6: () => ({
    html: cv('c', 0, 0, 832, 384) + dim() + abs(86, 16, 660, 352, `<div style="padding:12px 16px">
      <div class="row" style="justify-content:space-between"><span style="font-size:16px;font-weight:500">VOX & noise gate</span><span class="dim" style="font-size:10px">VOX is off at every start</span></div>
      ${lbl('MIC LEVEL · drag the markers', '10px 0 2px')}${micBar(628)}
      <div class="row" style="gap:16px;margin-top:10px;align-items:flex-start">
        <div style="flex:1">${lbl('VOX')}<div class="row">${togS('VOX', false)}</div>${sl('HANG', '250 ms', 25, 296)}
          ${lbl('MONITOR', '10px 0 4px')}<div class="row" style="gap:6px">${togS('Monitor', false)}${seg(['Pre', 'Post'], 'Post', 30)}</div>${sl('VOLUME', '−10 dB', 50, 296)}</div>
        <div style="flex:1">${lbl('NOISE GATE (DEXP)')}<div class="row" style="gap:6px">${togS('Gate', false)}${togS('Side filter', false, '1-2 kHz')}</div>
          ${sl('EXPANSION', '20 dB', 50, 296)}${sl('ATTACK / RELEASE', '25 / 100 ms', 30, 296)}${sl('HOLD', '800 ms', 60, 296)}
          <div class="dim" style="font-size:10px;margin-top:4px">more: hysteresis, tau, side filter edges ›</div></div>
      </div></div>`, 'glass', 'border-radius:18px;background:rgba(10,14,26,.96)'),
    canvases: [['#c', H.rxSpec()]],
  }),
});
})();
