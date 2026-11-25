/*
 * gl_render.c — the spectrum and waterfall, drawn on the GPU (GLES 3).
 *
 * History: a ring of spectrum rows in one R16F texture (newest row at
 * `head`), each row with its own frequency (meta texture), so when the VFO
 * moves the older rows stay where their signals really were.
 *
 * 3D view: the newest NROWS3D rows as a receding perspective trapezoid,
 * painted back to front. Each row is a "curtain" from its ridge down to the
 * floor plus a ridge line (a screen-space ribbon, so it can be thicker than
 * GL's 1 px lines). The four styles are the round-2 mock-ups:
 *   0  X6100 look 3: dark curtains, ridge lines coloured by strength
 *   1  AetherSDR 3DSS: curtains coloured by strength, thin white ridge lines
 *   2  lit terrain: coloured curtains shaded by slope, glowing strong ridges
 *   3  wire: dark curtains, bright ridge lines
 * The geometry follows AetherSDR's dss_mesh.vert (back width, depth span,
 * ridge height, non-linear height, rows wider than the view so the
 * trapezoid has no empty wedges); the code is ours.
 *
 * Waterfall: the ring texture again, one row per screen pixel, newest at
 * the top.
 *
 * Text (frequency axis, band names) is drawn by Compose over this surface.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "droid_app.h"

#define TEXW 4096          /* = DROID_ROW_MAX */
#define NR 1024            /* rows of history (~34 s at 30 rows/s) */
#define NROWS3D 80         /* rows in the 3D view */

static GLuint height_tex, meta_tex, pal_wf, pal_surf[4], pal_line, vao;
static GLuint prog_wf, prog_3d;
static int vw, vh;
static int head = -1, valid;
static uint64_t seq;
static double base_hz;                 /* meta frequencies are stored relative to this */
static double meta_cpu[NR][3];         /* fmin, hzpp, width: to re-base without loss */
static float meta_gpu[NR][4];
static uint16_t half_row[TEXW];

/* ---- palettes (the mock-ups' colours) ---- */

static const char *PAL_X6100[] = { "#071a6e", "#0d2f9e", "#1747c8", "#1fa84a", "#cfd52a", "#f2862a", "#ff2d24", 0 };
static const char *PAL_X6100LINE[] = { "#0b3a1c", "#178a34", "#2fc04a", "#c9da2c", "#f39030", "#ff3a2a", "#ffd0c0", 0 };
static const char *PAL_AETHER[] = { "#071040", "#0e2cb8", "#1658f0", "#16c0ea", "#3ae060", "#f6e232", "#ff3a22", 0 };

static GLuint make_palette(const char **stops) {
  int n = 0;
  unsigned char rgb[16][3], px[256 * 4];
  while (stops[n]) {
    unsigned v = (unsigned)strtoul(stops[n] + 1, NULL, 16);
    rgb[n][0] = v >> 16; rgb[n][1] = (v >> 8) & 255; rgb[n][2] = v & 255;
    n++;
  }
  for (int i = 0; i < 256; i++) {
    float x = i / 255.0f * (n - 1);
    int k = (int)x;
    if (k > n - 2) k = n - 2;
    float f = x - k;
    for (int c = 0; c < 3; c++) px[i * 4 + c] = (unsigned char)(rgb[k][c] + (rgb[k + 1][c] - rgb[k][c]) * f);
    px[i * 4 + 3] = 255;
  }
  GLuint t;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}

/* ---- shaders ---- */

#define COMMON \
  "uniform highp sampler2D uHeight;\n" \
  "uniform highp sampler2D uMeta;\n" \
  "uniform int uHead, uValid;\n" \
  "uniform highp float uViewLo, uViewSpan;\n" \
  "highp float rowDb(int age, highp float f) {\n" \
  "  if (age >= uValid) return -200.0;\n" \
  "  int r = (uHead - age + " "1024" ") % " "1024" ";\n" \
  "  highp vec4 m = texelFetch(uMeta, ivec2(r, 0), 0);\n" \
  "  highp float px = (f - m.x) / m.y;\n" \
  "  if (px < 0.5 || px > m.z - 0.5) return -200.0;\n" \
  "  return texture(uHeight, vec2(px / 4096.0, (float(r) + 0.5) / 1024.0)).r;\n" \
  "}\n"

static const char *WF_VS =
  "#version 300 es\n"
  "uniform vec4 uRect;\n"   /* x0, y0, x1, y1 in NDC */
  "out vec2 vUv;\n"
  "void main() {\n"
  "  vec2 c = vec2(gl_VertexID & 1, gl_VertexID >> 1);\n"
  "  vUv = c;\n"   /* y: 0 = top (newest) */
  "  gl_Position = vec4(mix(uRect.x, uRect.z, c.x), mix(uRect.w, uRect.y, c.y), 0.0, 1.0);\n"
  "}\n";

static const char *WF_FS =
  "#version 300 es\n"
  "precision highp float;\n"
  "precision highp int;\n"
  COMMON
  "uniform sampler2D uPal;\n"
  "uniform float uRowsShown, uLo, uHi;\n"
  "in vec2 vUv;\n"
  "out vec4 o;\n"
  "void main() {\n"
  "  int age = int(vUv.y * uRowsShown);\n"
  "  float db = rowDb(age, uViewLo + vUv.x * uViewSpan);\n"
  "  if (db < -190.0) { o = vec4(0.008, 0.016, 0.04, 1.0); return; }\n"
  "  o = vec4(texture(uPal, vec2(clamp((db - uLo) / (uHi - uLo), 0.0, 1.0), 0.5)).rgb, 1.0);\n"
  "}\n";

static const char *T3_VS =
  "#version 300 es\n"
  "precision highp float;\n"
  "precision highp int;\n"
  COMMON
  "uniform int uAge, uCols, uPass;\n"
  "uniform float uDepth, uBackW, uDepthSpan, uRidge, uZc, uFloor, uRange, uCRange, uSpan;\n"
  "uniform vec2 uPlotPx;\n"            /* 3D area size in pixels */
  "uniform vec2 uRectY;\n"             /* NDC y of the 3D area's top and bottom */
  "uniform float uLineHalf;\n"         /* ridge ribbon half width, px */
  "out float vS, vC, vEdge, vShade, vRib;\n"
  /* this row and the two before it, averaged as power, each at its own
   * frequency: smooth like deskHPSDR's averaging, but no smear when tuning */
  "float smoothDb(float f) {\n"
  "  float p = 0.0, n = 0.0;\n"
  "  for (int k = 0; k < 3; k++) {\n"
  "    float d = rowDb(uAge + k, f);\n"
  "    if (d > -190.0) { p += exp2(d * 0.33219281); n += 1.0; }\n"
  "  }\n"
  "  return n > 0.0 ? log2(p / n) * 3.0103 : -200.0;\n"
  "}\n"
  "float strength(float fu) {\n"
  "  float db = smoothDb(uViewLo + fu * uViewSpan);\n"
  "  return clamp((db - uFloor) / uRange, 0.0, 1.0);\n"
  "}\n"
  "vec2 ridge(float mu) {\n"
  "  float fu = 0.5 + (mu - 0.5) * uSpan;\n"
  "  float ws = mix(1.0, uBackW, uDepth);\n"
  "  float base = mix(1.0, 1.0 - uDepthSpan, uDepth);\n"
  "  return vec2(0.5 + (fu - 0.5) * ws, base - pow(strength(fu), uZc) * uRidge * ws);\n"
  "}\n"
  "void main() {\n"
  "  int c = gl_VertexID / 2, side = gl_VertexID % 2;\n"
  "  float du = 1.0 / float(uCols - 1);\n"
  "  float mu = float(c) * du;\n"
  "  float fu = 0.5 + (mu - 0.5) * uSpan;\n"
  "  float db = smoothDb(uViewLo + fu * uViewSpan);\n"
  "  vS = clamp((db - uFloor) / uRange, 0.0, 1.0);\n"
  "  vC = clamp((db - uFloor) / uCRange, 0.0, 1.0);\n"
  "  vec2 p = ridge(mu);\n"
  "  vec2 a = ridge(max(mu - du, 0.0)) * uPlotPx, b = ridge(min(mu + du, 1.0)) * uPlotPx;\n"
  "  vec2 t = b - a;\n"
  "  vShade = clamp(0.5 + (b.y - a.y) / max(length(t), 0.001) * 0.9, 0.0, 1.0);\n"
  "  vRib = 0.0; vEdge = 0.0;\n"
  "  if (uPass == 0) {\n"
  "    if (side == 1) { p.y = 1.0; vEdge = 1.0; }\n"
  "  } else {\n"
  "    vec2 n = length(t) > 0.001 ? normalize(vec2(-t.y, t.x)) : vec2(0.0, 1.0);\n"
  "    float sgn = side == 1 ? 1.0 : -1.0;\n"
  "    p += n * sgn * uLineHalf / uPlotPx;\n"
  "    vRib = sgn;\n"
  "  }\n"
  "  gl_Position = vec4(p.x * 2.0 - 1.0, mix(uRectY.x, uRectY.y, p.y), 0.0, 1.0);\n"
  "}\n";

static const char *T3_FS =
  "#version 300 es\n"
  "precision highp float;\n"
  "precision highp int;\n"
  "uniform sampler2D uSurf, uLine;\n"
  "uniform int uPass, uStyle;\n"
  "uniform float uDepth;\n"
  "in float vS, vC, vEdge, vShade, vRib;\n"
  "out vec4 o;\n"
  "const vec3 BG = vec3(0.012, 0.024, 0.055);\n"
  "void main() {\n"
  "  float fade = 1.0 - uDepth;\n"
  "  if (uPass == 0) {\n"
  "    if (uStyle == 0 || uStyle == 3) { o = vec4(mix(BG, vec3(0.03, 0.06, 0.12), vEdge * 0.0) , 1.0); return; }\n"
  "    vec3 c = texture(uSurf, vec2(vC, 0.5)).rgb;\n"
  "    if (uStyle == 1) c *= mix(1.0, 0.6, vEdge);\n"
  "    else c *= 0.45 + 1.1 * vShade + 0.15 * fade;\n"
  "    c = mix(c, BG, clamp(uDepth * (uStyle == 1 ? 0.35 : 0.5), 0.0, 1.0));\n"
  "    o = vec4(c, 1.0);\n"
  "    return;\n"
  "  }\n"
  "  float cov = 1.0 - smoothstep(0.35, 1.0, abs(vRib));\n"
  "  if (uStyle == 1) {\n"
  "    float a = (0.10 + 0.42 * fade) * (vS < 0.035 ? 0.42 : 1.0);\n"
  "    o = vec4(vec3(0.925, 0.94, 0.98), a * cov);\n"
  "    return;\n"
  "  }\n"
  "  vec3 lc = texture(uLine, vec2(vC, 0.5)).rgb;\n"
  "  float a = uStyle == 2 ? (vC > 0.35 ? 0.6 * fade : 0.0) : uStyle == 3 ? 0.25 + 0.75 * fade : 1.0 - 0.78 * pow(uDepth, 0.8);\n"
  "  o = vec4(lc, a * cov);\n"
  "}\n";

static GLuint compile(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetShaderInfoLog(s, sizeof log, NULL, log);
    droid_log("shader compile failed: %s", log);
  }
  return s;
}

static GLuint link(const char *vs, const char *fs) {
  GLuint p = glCreateProgram();
  glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
  glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
  glLinkProgram(p);
  GLint ok;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetProgramInfoLog(p, sizeof log, NULL, log);
    droid_log("shader link failed: %s", log);
  }
  return p;
}

#define U(p, n) glGetUniformLocation(p, n)

/* ---- float -> half for the R16F upload ---- */

static uint16_t to_half(float f) {
  union { float f; uint32_t u; } v = { f };
  uint32_t sign = (v.u >> 16) & 0x8000;
  int exp = (int)((v.u >> 23) & 0xff) - 127 + 15;
  uint32_t man = v.u & 0x7fffff;
  if (exp <= 0) return (uint16_t)sign;
  if (exp >= 31) return (uint16_t)(sign | 0x7bff);
  return (uint16_t)(sign | (exp << 10) | (man >> 13));
}

static void rebase(double hz) {
  base_hz = hz;
  for (int r = 0; r < NR; r++) {
    meta_gpu[r][0] = (float)(meta_cpu[r][0] - base_hz);
    meta_gpu[r][1] = (float)meta_cpu[r][1];
    meta_gpu[r][2] = (float)meta_cpu[r][2];
    meta_gpu[r][3] = 1.0f;
  }
  glBindTexture(GL_TEXTURE_2D, meta_tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, NR, 1, GL_RGBA, GL_FLOAT, meta_gpu);
}

static void take_row(const droid_row *row, void *ctx) {
  (void)ctx;
  head = (head + 1) % NR;
  if (valid < NR) valid++;
  for (int i = 0; i < row->width; i++) half_row[i] = to_half(row->db[i]);
  glBindTexture(GL_TEXTURE_2D, height_tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, head, row->width, 1, GL_RED, GL_HALF_FLOAT, half_row);
  meta_cpu[head][0] = row->fmin;
  meta_cpu[head][1] = row->hzpp;
  meta_cpu[head][2] = row->width;
  if (valid == 1 || fabs(row->fmin - base_hz) > 2e6) {
    rebase(row->fmin);
  } else {
    float m[4] = { (float)(row->fmin - base_hz), (float)row->hzpp, (float)row->width, 1.0f };
    memcpy(meta_gpu[head], m, sizeof m);
    glBindTexture(GL_TEXTURE_2D, meta_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, head, 0, 1, 1, GL_RGBA, GL_FLOAT, m);
  }
}

void droid_gl_init(void) {
  /* A new EGL context: everything is created again, history restarts. */
  head = -1;
  valid = 0;
  seq = 0;
  memset(meta_cpu, 0, sizeof meta_cpu);

  glGenTextures(1, &height_tex);
  glBindTexture(GL_TEXTURE_2D, height_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, TEXW, NR, 0, GL_RED, GL_HALF_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &meta_tex);
  glBindTexture(GL_TEXTURE_2D, meta_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, NR, 1, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  pal_wf = make_palette(PAL_X6100);
  pal_line = make_palette(PAL_X6100LINE);
  pal_surf[0] = pal_surf[3] = pal_wf;
  pal_surf[1] = make_palette(PAL_AETHER);
  pal_surf[2] = pal_wf;

  prog_wf = link(WF_VS, WF_FS);
  prog_3d = link(T3_VS, T3_FS);
  glGenVertexArrays(1, &vao);
  droid_log("GL: %s / %s", glGetString(GL_RENDERER), glGetString(GL_VERSION));
}

void droid_gl_resize(int w, int h) {
  vw = w;
  vh = h;
  glViewport(0, 0, w, h);
}

static void bind_common(GLuint p, const droid_view *v) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, height_tex);
  glUniform1i(U(p, "uHeight"), 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, meta_tex);
  glUniform1i(U(p, "uMeta"), 1);
  glUniform1i(U(p, "uHead"), head < 0 ? 0 : head);
  glUniform1i(U(p, "uValid"), valid);
  glUniform1f(U(p, "uViewLo"), (float)(v->view_lo - base_hz));
  glUniform1f(U(p, "uViewSpan"), (float)(v->view_hi - v->view_lo));
}

void droid_gl_draw(const droid_view *v) {
  droid_rows_pull(&seq, take_row, NULL);

  glDisable(GL_DEPTH_TEST);
  glClearColor(0.004f, 0.008f, 0.03f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (vw <= 0 || vh <= 0 || valid == 0) return;
  glBindVertexArray(vao);

  float split_ndc = 1.0f - 2.0f * (float)v->split;   /* NDC y where the waterfall starts */

  /* ---- waterfall ---- */
  glDisable(GL_BLEND);
  glUseProgram(prog_wf);
  bind_common(prog_wf, v);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, pal_wf);
  glUniform1i(U(prog_wf, "uPal"), 2);
  float wf_px = (float)vh * (1.0f - (float)v->split);
  glUniform1f(U(prog_wf, "uRowsShown"), wf_px > NR ? NR : wf_px);
  glUniform1f(U(prog_wf, "uLo"), v->wf_lo);
  glUniform1f(U(prog_wf, "uHi"), v->wf_hi);
  glUniform4f(U(prog_wf, "uRect"), -1.0f, -1.0f, 1.0f, split_ndc);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  /* ---- 3D ---- */
  const int style = v->style < 0 || v->style > 3 ? 0 : v->style;
  /* X6100 look 3 recedes gently, as in the picked mock-up; the terrain styles use AetherSDR's depth */
  const float back_w = style == 0 ? 0.8f : 0.6f, span = 1.0f / back_w;
  int cols = (int)(vw * 0.5f * span);
  if (cols > 2400) cols = 2400;
  float plot_h = (float)vh * (float)v->split;
  glUseProgram(prog_3d);
  bind_common(prog_3d, v);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, pal_surf[style]);
  glUniform1i(U(prog_3d, "uSurf"), 2);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, pal_line);
  glUniform1i(U(prog_3d, "uLine"), 3);
  glUniform1i(U(prog_3d, "uCols"), cols);
  glUniform1i(U(prog_3d, "uStyle"), style);
  glUniform1f(U(prog_3d, "uBackW"), back_w);
  glUniform1f(U(prog_3d, "uSpan"), span);
  glUniform1f(U(prog_3d, "uDepthSpan"), style == 0 ? 0.70f : 0.58f);
  glUniform1f(U(prog_3d, "uRidge"), style == 0 ? 0.62f : 0.46f);
  glUniform1f(U(prog_3d, "uZc"), style == 0 ? 1.0f : 0.6f);
  glUniform1f(U(prog_3d, "uFloor"), v->floor_db);
  glUniform1f(U(prog_3d, "uRange"), v->range_db);
  glUniform1f(U(prog_3d, "uCRange"), v->range_db * 0.9f);
  glUniform2f(U(prog_3d, "uPlotPx"), (float)vw, plot_h);
  glUniform2f(U(prog_3d, "uRectY"), 1.0f, split_ndc);
  glUniform1f(U(prog_3d, "uLineHalf"), style == 1 ? 0.9f : 1.6f);
  GLint u_age = U(prog_3d, "uAge"), u_depth = U(prog_3d, "uDepth"), u_pass = U(prog_3d, "uPass");
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  int rows = valid < NROWS3D ? valid : NROWS3D;
  for (int i = rows - 1; i >= 0; i--) {
    glUniform1i(u_age, i);
    glUniform1f(u_depth, (float)i / (NROWS3D - 1));
    glDisable(GL_BLEND);
    glUniform1i(u_pass, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, cols * 2);
    glEnable(GL_BLEND);
    glUniform1i(u_pass, 1);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, cols * 2);
  }
  glDisable(GL_BLEND);
}
