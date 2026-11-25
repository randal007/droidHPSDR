/*
 * cairo.h — droidHPSDR compat shim, NOT cairo. Opaque types, and do-nothing
 * versions of the drawing calls in upstream files droidHPSDR compiles
 * unchanged (vfo.c paints deskHPSDR's VFO bar with them). Compose and
 * gl_render.c draw droidHPSDR's screen; no cairo context ever exists, so
 * cairo_create() returns NULL and every call below does nothing.
 * Signatures follow cairo's, so a mismatch shows up at compile time.
 * GPL v3 or later.
 */
#ifndef DROID_COMPAT_CAIRO_H
#define DROID_COMPAT_CAIRO_H
#include <string.h>

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_pattern cairo_pattern_t;

typedef enum { CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_SLANT_OBLIQUE } cairo_font_slant_t;
typedef enum { CAIRO_FONT_WEIGHT_NORMAL, CAIRO_FONT_WEIGHT_BOLD } cairo_font_weight_t;
typedef enum { CAIRO_CONTENT_COLOR = 0x1000, CAIRO_CONTENT_ALPHA = 0x2000,
               CAIRO_CONTENT_COLOR_ALPHA = 0x3000 } cairo_content_t;
typedef struct { double x_bearing, y_bearing, width, height, x_advance, y_advance; } cairo_text_extents_t;

static inline cairo_t *cairo_create(cairo_surface_t *s) { (void)s; return NULL; }
static inline void cairo_destroy(cairo_t *cr) { (void)cr; }
static inline void cairo_surface_destroy(cairo_surface_t *s) { (void)s; }
static inline void cairo_save(cairo_t *cr) { (void)cr; }
static inline void cairo_restore(cairo_t *cr) { (void)cr; }
static inline void cairo_paint(cairo_t *cr) { (void)cr; }
static inline void cairo_stroke(cairo_t *cr) { (void)cr; }
static inline void cairo_fill(cairo_t *cr) { (void)cr; }
static inline void cairo_fill_preserve(cairo_t *cr) { (void)cr; }
static inline void cairo_new_path(cairo_t *cr) { (void)cr; }
static inline void cairo_new_sub_path(cairo_t *cr) { (void)cr; }
static inline void cairo_close_path(cairo_t *cr) { (void)cr; }
static inline void cairo_move_to(cairo_t *cr, double x, double y) { (void)cr; (void)x; (void)y; }
static inline void cairo_line_to(cairo_t *cr, double x, double y) { (void)cr; (void)x; (void)y; }
static inline void cairo_rectangle(cairo_t *cr, double x, double y, double w, double h) {
  (void)cr; (void)x; (void)y; (void)w; (void)h;
}
static inline void cairo_arc(cairo_t *cr, double xc, double yc, double r, double a1, double a2) {
  (void)cr; (void)xc; (void)yc; (void)r; (void)a1; (void)a2;
}
static inline void cairo_set_line_width(cairo_t *cr, double w) { (void)cr; (void)w; }
static inline void cairo_set_source_rgb(cairo_t *cr, double r, double g, double b) {
  (void)cr; (void)r; (void)g; (void)b;
}
static inline void cairo_set_source_rgba(cairo_t *cr, double r, double g, double b, double a) {
  (void)cr; (void)r; (void)g; (void)b; (void)a;
}
static inline void cairo_set_source(cairo_t *cr, cairo_pattern_t *p) { (void)cr; (void)p; }
static inline void cairo_set_source_surface(cairo_t *cr, cairo_surface_t *s, double x, double y) {
  (void)cr; (void)s; (void)x; (void)y;
}
static inline cairo_pattern_t *cairo_pattern_create_linear(double x0, double y0, double x1, double y1) {
  (void)x0; (void)y0; (void)x1; (void)y1;
  return NULL;
}
static inline void cairo_pattern_add_color_stop_rgb(cairo_pattern_t *p, double o, double r, double g, double b) {
  (void)p; (void)o; (void)r; (void)g; (void)b;
}
static inline void cairo_pattern_add_color_stop_rgba(cairo_pattern_t *p, double o, double r, double g, double b,
                                                     double a) {
  (void)p; (void)o; (void)r; (void)g; (void)b; (void)a;
}
static inline void cairo_pattern_destroy(cairo_pattern_t *p) { (void)p; }
static inline void cairo_select_font_face(cairo_t *cr, const char *family, cairo_font_slant_t slant,
                                          cairo_font_weight_t weight) {
  (void)cr; (void)family; (void)slant; (void)weight;
}
static inline void cairo_set_font_size(cairo_t *cr, double size) { (void)cr; (void)size; }
static inline void cairo_show_text(cairo_t *cr, const char *utf8) { (void)cr; (void)utf8; }
static inline void cairo_text_extents(cairo_t *cr, const char *utf8, cairo_text_extents_t *e) {
  (void)cr; (void)utf8;
  memset(e, 0, sizeof *e);
}
#endif
