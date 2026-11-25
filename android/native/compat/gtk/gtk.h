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
