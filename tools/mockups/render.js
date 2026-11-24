// droidHPSDR UI mock-ups: synthetic 20 m band + spectrum/waterfall painters.
// Pictures only; nothing here goes into the app.
'use strict';
const R = (() => {
  function mulberry32(a) {
    return function () {
      a |= 0; a = a + 0x6D2B79F5 | 0;
      let t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  }
  function h2(a, b) {
    let h = Math.imul((a * 374761393 + b * 668265263) | 0, 1274126177);
    h ^= h >>> 13; h = Math.imul(h, 1274126177);
    return ((h ^ h >>> 16) >>> 0) / 4294967296;
  }

  // ---- the band -------------------------------------------------------
  const rng = mulberry32(20261123);
  const S = [];
  const add = o => { o.id = S.length + 1; S.push(o); };
  [[14.0105, -78], [14.0127, -97], [14.0182, -91], [14.0231, -71], [14.0254, -99],
   [14.0307, -84], [14.0335, -101], [14.0364, -90], [14.0402, -87], [14.0448, -95],
   [14.0521, -80], [14.0573, -103], [14.0606, -89], [14.0652, -96]]
    .forEach(([f, l]) => add({ type: 'cw', f: f * 1e6, l, wpm: 4 + rng() * 6 }));
  for (let i = 0; i < 24; i++)
    add({ type: 'ft8', f: 14.074e6 + 180 + rng() * 2750, l: -114 + rng() * 38, par: i % 2, p: 0.55 + rng() * 0.45 });
  for (let i = 0; i < 6; i++)
    add({ type: 'ft8', f: 14.078e6 + 400 + rng() * 2100, l: -110 + rng() * 22, par: (i + 1) % 2, p: 0.5, slot: 15 });
  for (let i = 0; i < 4; i++)
    add({ type: 'psk', f: 14.0705e6 + rng() * 1800, l: -106 + rng() * 18 });
  add({ type: 'rtty', f: 14.0835e6, l: -82 });
  add({ type: 'rtty', f: 14.0868e6, l: -96 });
  add({ type: 'car', f: 14.1000e6, l: -92 });
  [[14.1505, -76], [14.1625, -90], [14.1700, -70], [14.1785, -94], [14.1855, -81], [14.1950, -99]]
    .forEach(([f, l]) => add({ type: 'ssb', f: f * 1e6, l }));

  const db = x => 10 * Math.log10(x);
  function comps(s, t) {
    switch (s.type) {
      case 'cw': {
        const el = Math.floor(t * s.wpm);
        const on = h2(s.id, el) > 0.42 && h2(s.id + 77, Math.floor(t / 5)) > 0.22;
        return on ? [[s.f, 9, s.l]] : [];
      }
      case 'ft8': {
        const slot = Math.floor(t / 15), ph = t - slot * 15;
        if (slot % 2 !== s.par || ph < 0.5 || ph > 13.1 || h2(s.id, slot) > s.p) return [];
        const sym = Math.floor(h2(s.id * 7 + 1, Math.floor(t / 0.16)) * 8);
        return [[s.f + sym * 6.25, 11, s.l]];
      }
      case 'psk': return [[s.f, 14, s.l + (h2(s.id, Math.floor(t * 3)) - 0.5) * 4]];
      case 'rtty': return [[s.f, 18, s.l], [s.f + 170, 18, s.l - 1]];
      case 'car': return [[s.f, 3, s.l]];
      case 'ssb': {
        if (h2(s.id, Math.floor(t / 7)) < 0.3) return [];
        const env = 0.25 + 0.75 * h2(s.id + 5, Math.floor(t * 4));
        const out = [];
        for (let off = 300; off <= 2700; off += 300)
          out.push([s.f + off, 150, s.l + db(env) - (off - 300) / 2400 * 14 + (h2(s.id + off, Math.floor(t * 4)) - 0.5) * 8]);
        return out;
      }
    }
    return [];
  }

  // rows[0] = newest; dB values
  function rows(f0, f1, nb, nr, dt, seed) {
    const r = mulberry32(seed || 99), out = [], bw = (f1 - f0) / nb, T0 = 1000.3;
    for (let k = 0; k < nr; k++) {
      const t = T0 - k * dt, p = new Float64Array(nb);
      for (let b = 0; b < nb; b++) {
        let e = 0;
        for (let q = 0; q < 4; q++) e -= Math.log(1 - r() * 0.999);
        p[b] = Math.pow(10, -11.85) * e / 4;
      }
      for (const s of S) {
        if (s.f < f0 - 4000 || s.f > f1 + 4000) continue;
        for (const [f, sg, l] of comps(s, t)) {
          const sig = Math.max(sg, bw * 0.6), a = Math.pow(10, l / 10);
          const b0 = Math.max(0, Math.floor((f - 4 * sig - f0) / bw)), b1 = Math.min(nb - 1, Math.ceil((f + 4 * sig - f0) / bw));
          for (let b = b0; b <= b1; b++) {
            const d = f0 + (b + 0.5) * bw - f;
            p[b] += a * Math.exp(-d * d / (2 * sig * sig));
          }
        }
      }
      const o = new Float32Array(nb);
      for (let b = 0; b < nb; b++) o[b] = db(p[b]);
      out.push(o);
    }
    return out;
  }

  // ---- palettes --------------------------------------------------------
  const PAL = {
    x6100: ['#071a6e', '#0d2f9e', '#1747c8', '#1fa84a', '#cfd52a', '#f2862a', '#ff2d24'],
    x6100line: ['#0b3a1c', '#178a34', '#2fc04a', '#c9da2c', '#f39030', '#ff3a2a', '#ffd0c0'],
    aurora: ['#010308', '#04112e', '#0a3a8c', '#0f86d0', '#2fe0e6', '#7cf59a', '#f6f27a'],
    ember: ['#020104', '#1c0636', '#560c5e', '#a3204c', '#e5532a', '#ffa828', '#fff3b8'],
    desk: ['#000818', '#00208a', '#0058e0', '#00c8e0', '#30e040', '#f0f020', '#ff3010'],
    aether: ['#071040', '#0e2cb8', '#1658f0', '#16c0ea', '#3ae060', '#f6e232', '#ff3a22'],
    ice: ['#010409', '#03142a', '#073a66', '#0f75a8', '#43b8d8', '#b0ecf4', '#ffffff'],
  };
  const hex = h => [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16));
  function lut(name) {
    const st = PAL[name].map(hex), n = st.length - 1, o = [];
    for (let i = 0; i < 256; i++) {
      const x = i / 255 * n, k = Math.min(n - 1, Math.floor(x)), f = x - k;
      o.push(st[k].map((c, j) => Math.round(c + (st[k + 1][j] - c) * f)));
    }
    return o;
  }
  const rgb = (c, a = 1, m = 1) => `rgba(${c[0] * m | 0},${c[1] * m | 0},${c[2] * m | 0},${a})`;
  const clamp = v => v < 0 ? 0 : v > 1 ? 1 : v;
  const norm = (d, lo, hi) => clamp((d - lo) / (hi - lo));
  function smooth(row, k) {
    if (!k) return row;
    const o = new Float32Array(row.length);
    for (let i = 0; i < row.length; i++) {
      let s = 0, n = 0;
      for (let j = -k; j <= k; j++) { const v = row[i + j]; if (v !== undefined) { s += v; n++; } }
      o[i] = s / n;
    }
    return o;
  }

  // ---- painters ---------------------------------------------------------
  function stack3d(ctx, x, y, w, h, rs, o) {
    const n = o.n || 46, L = lut(o.line || 'x6100line'), nb = rs[0].length;
    const g = ctx.createLinearGradient(0, y, 0, y + h);
    g.addColorStop(0, o.sky || '#020409'); g.addColorStop(1, o.ground || '#071226');
    ctx.fillStyle = g; ctx.fillRect(x, y, w, h);
    const backY = y + h * (o.back ?? 0.30), frontY = y + h - 1, Hf = h * (o.height ?? 0.62), cx = x + w / 2;
    const lo = o.lo ?? -117, hi = o.hi ?? -64;
    for (let i = n - 1; i >= 0; i--) {
      const z = i / (n - 1), s = 1 - (o.persp ?? 0.2) * z;
      const base = frontY - (frontY - backY) * Math.pow(z, 0.85), H = Hf * (1 - 0.42 * z);
      const row = smooth(rs[i], 1), px = [], py = [], pv = [];
      for (let j = 0; j < nb; j++) {
        const v = norm(row[j], lo, hi);
        px.push(cx + (x + (j + 0.5) / nb * w - cx) * s); py.push(base - H * v); pv.push(v);
      }
      ctx.beginPath(); ctx.moveTo(px[0], base);
      for (let j = 0; j < nb; j++) ctx.lineTo(px[j], py[j]);
      ctx.lineTo(px[nb - 1], base); ctx.closePath();
      ctx.fillStyle = o.occlude || 'rgba(3,7,16,0.93)'; ctx.fill();
      const fade = 1 - 0.78 * Math.pow(z, 0.8);
      ctx.lineWidth = i === 0 ? 1.4 : 1;
      for (let bk = 0; bk < 24; bk++) {
        ctx.beginPath(); let any = false;
        for (let j = 1; j < nb; j++) {
          const v = Math.max(pv[j], pv[j - 1]);
          if (Math.min(23, Math.floor(v * 24)) !== bk) continue;
          ctx.moveTo(px[j - 1], py[j - 1]); ctx.lineTo(px[j], py[j]); any = true;
        }
        if (any) { ctx.strokeStyle = rgb(L[Math.round(bk / 23 * 255)], fade); ctx.stroke(); }
      }
      if (i === 0 && o.glow !== false) {
        ctx.save(); ctx.globalCompositeOperation = 'lighter'; ctx.shadowBlur = 10;
        for (let j = 1; j < nb; j++) {
          if (pv[j] < 0.45) continue;
          const c = L[Math.round(pv[j] * 255)];
          ctx.shadowColor = rgb(c, 1); ctx.strokeStyle = rgb(c, 0.55); ctx.lineWidth = 2;
          ctx.beginPath(); ctx.moveTo(px[j - 1], py[j - 1]); ctx.lineTo(px[j], py[j]); ctx.stroke();
        }
        ctx.restore();
      }
    }
    if (o.fog) {
      const f = ctx.createLinearGradient(0, y, 0, backY + 20);
      f.addColorStop(0, 'rgba(2,4,9,0.9)'); f.addColorStop(1, 'rgba(2,4,9,0)');
      ctx.fillStyle = f; ctx.fillRect(x, y, w, backY + 20 - y);
    }
  }

  function surface3d(ctx, x, y, w, h, rs, o) {
    const n = o.n || 52, L = lut(o.pal || 'desk'), nb = rs[0].length;
    ctx.fillStyle = o.sky || '#01030a'; ctx.fillRect(x, y, w, h);
    const backY = y + h * (o.back ?? 0.22), frontY = y + h - 1, Hf = h * (o.height ?? 0.55), cx = x + w / 2;
    const lo = o.lo ?? -119, hi = o.hi ?? -66;
    const cache = {};
    const col = (vi, m) => { const k = vi * 16 + m; return cache[k] || (cache[k] = rgb(L[vi], 1, 0.35 + 0.65 * m / 15)); };
    for (let i = n - 1; i >= 0; i--) {
      const z = i / (n - 1), s = 1 - (o.persp ?? 0.18) * z;
      const base = frontY - (frontY - backY) * Math.pow(z, 0.8), H = Hf * (1 - 0.4 * z);
      const row = smooth(rs[i], 1), cw = w / nb * s + 0.6, m = Math.round((1 - z) * 15);
      for (let j = 0; j < nb; j++) {
        const v = norm(row[j], lo, hi), xx = cx + (x + j / nb * w - cx) * s, yy = base - H * v;
        ctx.fillStyle = col(Math.round(v * 255), m);
        ctx.fillRect(xx, yy, cw, base - yy + (h / n) * 1.2);
      }
    }
    const f = ctx.createLinearGradient(0, y, 0, backY + 30);
    f.addColorStop(0, 'rgba(1,3,10,1)'); f.addColorStop(1, 'rgba(1,3,10,0)');
    ctx.fillStyle = f; ctx.fillRect(x, y, w, backY + 30 - y);
  }

  // AetherSDR-style perspective terrain ("3DSS"): each row is a coloured
  // curtain down to the plot floor, newest at the front; rows cover a wider
  // span than the view so the receding trapezoid has no empty wedges.
  // style: 'aether' (white ridge lines), 'lit' (slope-lit, glowing front), 'wire' (neon mesh)
  function terrain(ctx, x, y, w, h, sp, o) {
    const n = o.n || 72, backW = o.backW ?? 0.6, depth = o.depth ?? 0.58, ridgeF = o.ridge ?? 0.46;
    const haze = o.haze ?? 0.35, zc = o.zCurve ?? 0.6, floor = o.floor ?? -121, range = o.range ?? 52, crange = o.crange ?? 46;
    const span = sp.f1 - sp.f0, fac = 1 / backW, ext = span * (fac - 1) / 2;
    const nb = Math.round(w * fac * (o.res || 1));
    const rs = rows(sp.f0 - ext, sp.f1 + ext, nb, n + 2, o.dt || 0.25, sp.seed);
    const L = lut(o.pal || 'aether'), bg = hex(o.bg || '#02040b'), style = o.style || 'aether';
    const g = ctx.createLinearGradient(0, y, 0, y + h);
    g.addColorStop(0, o.sky || '#010208'); g.addColorStop(1, rgb(bg));
    ctx.fillStyle = g; ctx.fillRect(x, y, w, h);
    ctx.save(); ctx.beginPath(); ctx.rect(x, y, w, h); ctx.clip();
    const cache = {};
    const col = (vi, hz, sh) => {
      const k = (vi * 16 + hz) * 32 + sh;
      if (cache[k]) return cache[k];
      const c = L[vi], f = hz / 15, m = 0.45 + sh / 31 * 1.1;
      return cache[k] = `rgb(${(c[0] * m * (1 - f) + bg[0] * f) | 0},${(c[1] * m * (1 - f) + bg[1] * f) | 0},${(c[2] * m * (1 - f) + bg[2] * f) | 0})`;
    };
    let prevX = null, prevY = null;
    for (let i = n - 1; i >= 0; i--) {
      const z = i / (n - 1), ws = 1 - z * (1 - backW), baseY = y + h - z * depth * h, fade = 1 - z;
      const row = smooth(rs[i].map((v, j) => 10 * Math.log10((Math.pow(10, v / 10) + Math.pow(10, rs[i + 1][j] / 10) + Math.pow(10, rs[i + 2][j] / 10)) / 3)), o.smooth ?? 2), px = new Float32Array(nb), py = new Float32Array(nb), pc = new Float32Array(nb), ps = new Float32Array(nb);
      for (let j = 0; j < nb; j++) {
        const fu = (j + 0.5) / nb * fac - (fac - 1) / 2;
        const s = clamp((row[j] - floor) / range);
        px[j] = x + (0.5 + (fu - 0.5) * ws) * w;
        py[j] = baseY - Math.pow(s, zc) * ridgeF * h * ws;
        pc[j] = clamp((row[j] - floor) / crange); ps[j] = s;
      }
      const cw = w * fac / nb * ws + 0.7, hz = Math.min(15, Math.round(z * haze * 15 / 0.5));
      if (style === 'wire') {
        ctx.beginPath(); ctx.moveTo(px[0], y + h);
        for (let j = 0; j < nb; j++) ctx.lineTo(px[j], py[j]);
        ctx.lineTo(px[nb - 1], y + h); ctx.closePath(); ctx.fillStyle = rgb(bg, 0.96); ctx.fill();
      } else {
        for (let j = 0; j < nb; j++) {
          if (px[j] < x - cw || px[j] > x + w) continue;
          let sh = 15;
          if (style === 'lit') {
            const dy = (py[Math.min(nb - 1, j + 1)] - py[Math.max(0, j - 1)]) / (2 * cw);
            sh = Math.max(0, Math.min(31, Math.round(15 + dy * 22 + fade * 4)));
          }
          ctx.fillStyle = col(Math.round(pc[j] * 255), hz, sh);
          ctx.fillRect(px[j], py[j], cw, y + h - py[j]);
        }
      }
      if (style === 'aether') {
        ctx.beginPath();
        for (let j = 0; j < nb; j++) j ? ctx.lineTo(px[j], py[j]) : ctx.moveTo(px[j], py[j]);
        ctx.strokeStyle = `rgba(236,240,250,${(0.10 + 0.42 * fade).toFixed(3)})`; ctx.lineWidth = 0.6; ctx.stroke();
      } else {
        const every = style === 'wire' ? 1 : 0;
        for (let bk = 0; bk < 20; bk++) {
          ctx.beginPath(); let any = false;
          for (let j = 1; j < nb; j++) {
            const v = Math.max(pc[j], pc[j - 1]);
            if (Math.min(19, Math.floor(v * 20)) !== bk) continue;
            if (style === 'lit' && v < 0.35) continue;
            ctx.moveTo(px[j - 1], py[j - 1]); ctx.lineTo(px[j], py[j]); any = true;
            if (every && prevX && j % 9 === 0) { ctx.moveTo(px[j], py[j]); ctx.lineTo(prevX[j], prevY[j]); }
          }
          if (any) { ctx.strokeStyle = rgb(L[Math.round(bk / 19 * 255)], (style === 'wire' ? 0.25 + 0.75 * fade : 0.6 * fade)); ctx.lineWidth = style === 'wire' ? 0.8 : 1; ctx.stroke(); }
        }
      }
      if (i === 0 && style !== 'aether') {
        ctx.save(); ctx.globalCompositeOperation = 'lighter'; ctx.shadowBlur = 12;
        for (let j = 1; j < nb; j++) {
          if (pc[j] < 0.4) continue;
          const c = L[Math.round(pc[j] * 255)];
          ctx.shadowColor = rgb(c); ctx.strokeStyle = rgb(c, 0.6); ctx.lineWidth = 2;
          ctx.beginPath(); ctx.moveTo(px[j - 1], py[j - 1]); ctx.lineTo(px[j], py[j]); ctx.stroke();
        }
        ctx.restore();
      }
      prevX = px; prevY = py;
    }
    ctx.restore();
    const f = ctx.createLinearGradient(0, y, 0, y + h * (1 - depth) + 10);
    f.addColorStop(0, 'rgba(1,2,8,0.95)'); f.addColorStop(1, 'rgba(1,2,8,0)');
    ctx.fillStyle = f; ctx.fillRect(x, y, w, h * (1 - depth) + 10);
  }

  function spec2d(ctx, x, y, w, h, rs, o) {
    const nb = rs[0].length, lo = o.lo ?? -125, hi = o.hi ?? -60;
    const g0 = ctx.createLinearGradient(0, y, 0, y + h);
    g0.addColorStop(0, o.sky || '#03060d'); g0.addColorStop(1, o.ground || '#060c18');
    ctx.fillStyle = g0; ctx.fillRect(x, y, w, h);
    if (o.grid !== false) {
      ctx.strokeStyle = 'rgba(255,255,255,0.06)'; ctx.lineWidth = 1;
      ctx.fillStyle = 'rgba(255,255,255,0.35)'; ctx.font = '9px Roboto';
      for (let d = Math.ceil(lo / 10) * 10; d <= hi; d += 10) {
        const yy = y + h - (d - lo) / (hi - lo) * h;
        ctx.beginPath(); ctx.moveTo(x, yy); ctx.lineTo(x + w, yy); ctx.stroke();
        if (o.dbl && d > lo + 5 && d < hi - 3) ctx.fillText(d, x + (o.dblx || 4), yy - 2);
      }
    }
    // average a few rows for the trace, max for peak hold
    const av = new Float32Array(nb), pk = new Float32Array(nb).fill(-200);
    for (let k = 0; k < 4; k++) for (let j = 0; j < nb; j++) av[j] += rs[k][j] / 4;
    for (let k = 0; k < Math.min(rs.length, 60); k++) for (let j = 0; j < nb; j++) pk[j] = Math.max(pk[j], rs[k][j]);
    const sm = smooth(av, 1), sp = smooth(pk, 1);
    const Y = d => y + h - norm(d, lo, hi) * h, X = j => x + (j + 0.5) / nb * w;
    ctx.beginPath(); ctx.moveTo(x, y + h);
    for (let j = 0; j < nb; j++) ctx.lineTo(X(j), Y(sm[j]));
    ctx.lineTo(x + w, y + h); ctx.closePath();
    const g = ctx.createLinearGradient(0, y, 0, y + h);
    if (o.fill === 'pal') {
      const st = PAL[o.pal || 'desk'];
      [[0, st[6]], [0.3, st[5]], [0.6, st[4]], [1, st[3]]].forEach(([p, c]) => g.addColorStop(p, c));
    } else {
      g.addColorStop(0, o.fillTop || 'rgba(63,208,255,0.55)'); g.addColorStop(1, o.fillBot || 'rgba(63,208,255,0.02)');
    }
    ctx.fillStyle = g; ctx.fill();
    if (o.peak !== false) {
      ctx.beginPath(); for (let j = 0; j < nb; j++) j ? ctx.lineTo(X(j), Y(sp[j])) : ctx.moveTo(X(j), Y(sp[j]));
      ctx.strokeStyle = 'rgba(255,255,255,0.16)'; ctx.lineWidth = 1; ctx.stroke();
    }
    ctx.beginPath(); for (let j = 0; j < nb; j++) j ? ctx.lineTo(X(j), Y(sm[j])) : ctx.moveTo(X(j), Y(sm[j]));
    ctx.strokeStyle = o.line || '#bff3ff'; ctx.lineWidth = 1.2;
    ctx.save(); ctx.shadowBlur = o.glow === false ? 0 : 6; ctx.shadowColor = o.line || '#3fd0ff'; ctx.stroke(); ctx.restore();
  }

  function wf2d(ctx, x, y, w, h, rs, o) {
    const nb = rs[0].length, nr = Math.min(rs.length, Math.round(h * (o.rowsPerPx || 1))), L = lut(o.pal || 'x6100');
    const lo = o.lo ?? -121, hi = o.hi ?? -70;
    const c = document.createElement('canvas'); c.width = nb; c.height = nr;
    const cx = c.getContext('2d'), im = cx.createImageData(nb, nr);
    for (let r = 0; r < nr; r++) for (let j = 0; j < nb; j++) {
      const p = L[Math.round(norm(rs[r][j], lo, hi) * 255)], k = (r * nb + j) * 4;
      im.data[k] = p[0]; im.data[k + 1] = p[1]; im.data[k + 2] = p[2]; im.data[k + 3] = 255;
    }
    cx.putImageData(im, 0, 0);
    ctx.imageSmoothingEnabled = true; ctx.drawImage(c, x, y, w, h);
  }

  // ---- overlays ----------------------------------------------------------
  function niceStep(span, want) {
    for (const s of [100, 200, 250, 500, 1000, 2000, 2500, 5000, 10000, 20000, 25000, 50000, 100000])
      if (span / s <= want) return s;
    return 100000;
  }
  function fmtF(f, step) {
    const d = step % 10000 === 0 ? 2 : step % 1000 === 0 ? 3 : 4;
    return (f / 1e6).toFixed(d);
  }
  function axis(ctx, x, y, w, f0, f1, o) {
    const step = niceStep(f1 - f0, o.want || 7);
    if (o.band) { ctx.fillStyle = o.band; ctx.fillRect(x, y - 8, w, 16); }
    ctx.font = (o.size || 10) + 'px Roboto'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    for (let f = Math.ceil(f0 / step) * step; f <= f1; f += step) {
      const xx = x + (f - f0) / (f1 - f0) * w;
      if (xx < x + 18 || xx > x + w - 18) continue;
      ctx.fillStyle = 'rgba(255,255,255,0.28)'; ctx.fillRect(xx - 0.5, y - 8, 1, 3);
      ctx.fillStyle = o.color || 'rgba(225,235,250,0.82)'; ctx.fillText(fmtF(f, step), xx, y + 1);
    }
    ctx.textAlign = 'left'; ctx.textBaseline = 'alphabetic';
  }
  const PLAN = [
    [0, 14.000, 'OUT OF BAND', '#e04040'], [14.350, 99, 'OUT OF BAND', '#e04040'],
    [14.000, 14.070, 'CW', '#3b7bff'], [14.070, 14.099, 'DIGITAL', '#2fd07a'], [14.099, 14.101, 'BCN', '#ff5ad0'],
    [14.101, 14.112, 'DIGI', '#2fd07a'], [14.112, 14.350, 'ALL MODES · PHONE', '#ffb341'],
  ];
  function bandplan(ctx, x, y, w, f0, f1) {
    ctx.font = '600 8.5px Roboto';
    for (const [a, b, t, c] of PLAN) {
      const xa = x + Math.max(0, (a * 1e6 - f0) / (f1 - f0)) * w, xb = x + Math.min(1, (b * 1e6 - f0) / (f1 - f0)) * w;
      if (xb <= xa) continue;
      ctx.fillStyle = c; ctx.globalAlpha = 0.9; ctx.fillRect(xa + 0.5, y, xb - xa - 1, 4); ctx.globalAlpha = 1;
      if (xb - xa > 40) { ctx.save(); ctx.shadowColor = '#000'; ctx.shadowBlur = 4; ctx.fillStyle = c; ctx.fillText(t, xa + 5, y + 14); ctx.restore(); }
    }
  }

  function paint(cv, sp) {
    const dpr = window.devicePixelRatio || 1, W = cv.clientWidth, H = cv.clientHeight;
    const rot = !!sp.rotate, w = rot ? H : W, h = rot ? W : H;
    cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr);
    const ctx = cv.getContext('2d');
    ctx.scale(dpr, dpr);
    if (rot) { ctx.translate(W, 0); ctx.rotate(Math.PI / 2); }
    const nb = Math.round(w * (sp.res || 1));
    let need = 0;
    for (const p of sp.parts) need = Math.max(need, p.kind === 'wf2d' ? Math.ceil(p.h * h) + 2 : p.kind === 'terrain' ? 2 : 70);
    const rs = rows(sp.f0, sp.f1, nb, need, sp.dt || 0.25, sp.seed);
    let yy = 0; const bounds = [];
    for (const p of sp.parts) {
      const ph = p.h * h;
      if (p.kind === 'terrain') terrain(ctx, 0, yy, w, ph, sp, p);
      else ({ stack3d, surface3d, spec2d, wf2d })[p.kind](ctx, 0, yy, w, ph, rs, p);
      bounds.push([yy, ph]); yy += ph;
    }
    const X = f => (f - sp.f0) / (sp.f1 - sp.f0) * w;
    if (sp.filt) {
      const [a, b] = sp.filt, xa = X(sp.vfo + a), xb = X(sp.vfo + b);
      ctx.fillStyle = sp.filtCol || 'rgba(120,200,255,0.13)'; ctx.fillRect(xa, 0, xb - xa, h);
      ctx.strokeStyle = sp.filtEdge || 'rgba(140,210,255,0.35)'; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(xa, 0); ctx.lineTo(xa, h); ctx.moveTo(xb, 0); ctx.lineTo(xb, h); ctx.stroke();
    }
    if (sp.vfo) {
      ctx.strokeStyle = sp.vfoCol || '#ff4d4d'; ctx.lineWidth = 1.2;
      ctx.beginPath(); ctx.moveTo(X(sp.vfo), 0); ctx.lineTo(X(sp.vfo), h); ctx.stroke();
    }
    if (sp.axis !== 'none' && sp.parts.length > 1) {
      const ay = sp.axisY != null ? sp.axisY * h : bounds[1][0];
      axis(ctx, 0, ay, w, sp.f0, sp.f1, { band: sp.axisBand ?? 'rgba(2,5,12,0.72)', want: sp.want, size: sp.axisSize });
      if (sp.bandplan) bandplan(ctx, 0, ay + 8, w, sp.f0, sp.f1);
    } else if (sp.axis === 'top') axis(ctx, 0, 9, w, sp.f0, sp.f1, { band: 'rgba(2,5,12,0.6)', want: sp.want });
    if (sp.labels) {
      ctx.font = '600 9px Roboto';
      for (const lb of sp.labels) {
        const xx = X(lb.f), ty = lb.y * h, tw = ctx.measureText(lb.t).width + 10;
        ctx.strokeStyle = 'rgba(255,255,255,0.35)'; ctx.beginPath(); ctx.moveTo(xx, ty + 7); ctx.lineTo(xx, ty + (lb.drop || 14)); ctx.stroke();
        ctx.fillStyle = lb.bg || 'rgba(8,14,28,0.82)';
        rr(ctx, xx - tw / 2, ty - 7, tw, 14, 7); ctx.fill();
        ctx.strokeStyle = lb.c || 'rgba(255,255,255,0.22)'; ctx.stroke();
        ctx.fillStyle = lb.c || '#e8eef8'; ctx.textAlign = 'center'; ctx.fillText(lb.t, xx, ty + 3); ctx.textAlign = 'left';
      }
    }
  }
  function rr(ctx, x, y, w, h, r) {
    ctx.beginPath(); ctx.moveTo(x + r, y); ctx.arcTo(x + w, y, x + w, y + h, r); ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r); ctx.arcTo(x, y, x + w, y, r); ctx.closePath();
  }
  return { paint, PAL };
})();
