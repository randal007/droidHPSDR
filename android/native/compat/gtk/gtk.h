/*
 * gtk/gtk.h — droidHPSDR compat shim, NOT GTK. See glib.h.
 * Opaque widget types and no-op functions for the few GTK calls left in the
 * upstream files droidHPSDR compiles unchanged. droidHPSDR's UI is Jetpack
 * Compose; no GTK widget is ever created, so every GtkWidget * is NULL.
 * GPL v3 or later.
 */
#ifndef DROID_COMPAT_GTK_H
#define DROID_COMPAT_GTK_H
#include <glib.h>
#include <gdk/gdk.h>
#include <cairo.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkGrid GtkGrid;
typedef struct _GtkFixed GtkFixed;
typedef struct _GtkContainer GtkContainer;
typedef struct _GtkLabel GtkLabel;
typedef struct _GtkButton GtkButton;
typedef struct _GtkAdjustment GtkAdjustment;

#define GTK_WIDGET(w) ((GtkWidget *)(w))
#define GTK_WINDOW(w) ((GtkWindow *)(w))
#define GTK_GRID(w) ((GtkGrid *)(w))
#define GTK_FIXED(w) ((GtkFixed *)(w))
#define GTK_CONTAINER(w) ((GtkContainer *)(w))
#define GTK_LABEL(w) ((GtkLabel *)(w))
#define GTK_BUTTON(w) ((GtkButton *)(w))

/* signals: no widget exists, so nothing is ever connected */
typedef void (*GCallback)(void);
#define G_CALLBACK(f) ((GCallback)(f))
#define g_signal_connect(instance, signal, handler, data) ((gulong)0)

/* vfo.c's VFO bar drawing area: never created (Compose draws the VFO) */
static inline GtkWidget *gtk_drawing_area_new(void) { return NULL; }
static inline GdkWindow *gtk_widget_get_window(GtkWidget *w) { (void)w; return NULL; }
static inline int gtk_widget_get_allocated_width(GtkWidget *w) { (void)w; return 0; }
static inline int gtk_widget_get_allocated_height(GtkWidget *w) { (void)w; return 0; }
static inline gint gtk_widget_get_events(GtkWidget *w) { (void)w; return 0; }
static inline void gtk_widget_set_events(GtkWidget *w, gint events) { (void)w; (void)events; }

/* transmitter.c's TX dialog, levels popover and panel (M7): never created, every widget stays NULL */
typedef struct _GtkBox GtkBox;
typedef struct _GtkBin GtkBin;
typedef struct _GtkDialog GtkDialog;
typedef struct _GtkHeaderBar GtkHeaderBar;
typedef struct _GtkPopover GtkPopover;
typedef GdkRectangle GtkAllocation;
typedef enum { GTK_ORIENTATION_HORIZONTAL, GTK_ORIENTATION_VERTICAL } GtkOrientation;
typedef enum { GTK_POS_LEFT, GTK_POS_RIGHT, GTK_POS_TOP, GTK_POS_BOTTOM } GtkPositionType;
typedef enum { GTK_POPOVER_CONSTRAINT_NONE, GTK_POPOVER_CONSTRAINT_WINDOW } GtkPopoverConstraint;
typedef enum { GTK_WIN_POS_NONE, GTK_WIN_POS_CENTER, GTK_WIN_POS_MOUSE } GtkWindowPosition;
#define GTK_BOX(w) ((GtkBox *)(w))
#define GTK_BIN(w) ((GtkBin *)(w))
#define GTK_DIALOG(w) ((GtkDialog *)(w))
#define GTK_HEADER_BAR(w) ((GtkHeaderBar *)(w))
#define GTK_POPOVER(w) ((GtkPopover *)(w))
#define GTK_IS_BIN(w) ((w) != NULL && 0)
#define GTK_IS_WINDOW(w) ((w) != NULL && 0)
static inline GtkWidget *gtk_dialog_new(void) { return NULL; }
static inline GtkWidget *gtk_dialog_get_content_area(GtkDialog *d) { (void)d; return NULL; }
static inline GtkWidget *gtk_header_bar_new(void) { return NULL; }
static inline void gtk_header_bar_set_show_close_button(GtkHeaderBar *b, gboolean s) { (void)b; (void)s; }
static inline void gtk_header_bar_set_title(GtkHeaderBar *b, const gchar *t) { (void)b; (void)t; }
static inline GtkWidget *gtk_box_new(GtkOrientation o, gint spacing) { (void)o; (void)spacing; return NULL; }
static inline void gtk_box_pack_start(GtkBox *b, GtkWidget *c, gboolean e, gboolean f, guint p) {
  (void)b; (void)c; (void)e; (void)f; (void)p;
}
static inline GtkWidget *gtk_bin_get_child(GtkBin *b) { (void)b; return NULL; }
static inline void gtk_container_add(GtkContainer *c, GtkWidget *w) { (void)c; (void)w; }
static inline GtkWidget *gtk_popover_new(GtkWidget *relative_to) { (void)relative_to; return NULL; }
static inline void gtk_popover_set_constrain_to(GtkPopover *p, GtkPopoverConstraint c) { (void)p; (void)c; }
static inline void gtk_popover_set_modal(GtkPopover *p, gboolean m) { (void)p; (void)m; }
static inline void gtk_popover_set_pointing_to(GtkPopover *p, const GdkRectangle *r) { (void)p; (void)r; }
static inline void gtk_popover_set_position(GtkPopover *p, GtkPositionType pos) { (void)p; (void)pos; }
static inline void gtk_widget_add_events(GtkWidget *w, gint events) { (void)w; (void)events; }
static inline void gtk_widget_destroy(GtkWidget *w) { (void)w; }
static inline void gtk_widget_get_allocation(GtkWidget *w, GtkAllocation *a) { (void)w; a->x = a->y = a->width = a->height = 0; }
static inline gboolean gtk_widget_get_mapped(GtkWidget *w) { (void)w; return FALSE; }
static inline GtkWidget *gtk_widget_get_parent(GtkWidget *w) { (void)w; return NULL; }
static inline gboolean gtk_widget_get_realized(GtkWidget *w) { (void)w; return FALSE; }
static inline gboolean gtk_widget_get_visible(GtkWidget *w) { (void)w; return FALSE; }
static inline void gtk_widget_set_can_focus(GtkWidget *w, gboolean c) { (void)w; (void)c; }
static inline void gtk_window_get_position(GtkWindow *w, gint *x, gint *y) { (void)w; if (x) *x = 0; if (y) *y = 0; }
static inline void gtk_window_move(GtkWindow *w, gint x, gint y) { (void)w; (void)x; (void)y; }
static inline void gtk_window_present(GtkWindow *w) { (void)w; }
static inline void gtk_window_set_accept_focus(GtkWindow *w, gboolean s) { (void)w; (void)s; }
static inline void gtk_window_set_default_size(GtkWindow *w, gint width, gint height) { (void)w; (void)width; (void)height; }
static inline void gtk_window_set_position(GtkWindow *w, GtkWindowPosition p) { (void)w; (void)p; }
static inline void gtk_window_set_resizable(GtkWindow *w, gboolean r) { (void)w; (void)r; }
static inline void gtk_window_set_titlebar(GtkWindow *w, GtkWidget *t) { (void)w; (void)t; }
static inline void gtk_window_set_transient_for(GtkWindow *w, GtkWindow *p) { (void)w; (void)p; }

GtkWidget *gtk_fixed_new(void);
void gtk_fixed_put(GtkFixed *fixed, GtkWidget *widget, gint x, gint y);
void gtk_fixed_move(GtkFixed *fixed, GtkWidget *widget, gint x, gint y);
void gtk_container_remove(GtkContainer *container, GtkWidget *widget);
void gtk_widget_set_size_request(GtkWidget *widget, gint width, gint height);
void gtk_widget_show_all(GtkWidget *widget);
void gtk_widget_queue_draw(GtkWidget *widget);

#ifdef __cplusplus
}
#endif
#endif
