/*
 * gdk/gdk.h — droidHPSDR compat shim, NOT GDK. See glib.h.
 * Event structs carry only the fields the upstream files read; droidHPSDR
 * never delivers GDK events, so the handlers that use them never run.
 * GPL v3 or later.
 */
#ifndef DROID_COMPAT_GDK_H
#define DROID_COMPAT_GDK_H
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GdkWindow GdkWindow;
typedef struct _GdkDevice GdkDevice;
typedef struct _GdkScreen GdkScreen;
typedef struct _GdkDisplay GdkDisplay;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkCursor GdkCursor;
typedef struct { double red, green, blue, alpha; } GdkRGBA;

typedef enum {
  GDK_SHIFT_MASK = 1 << 0, GDK_LOCK_MASK = 1 << 1, GDK_CONTROL_MASK = 1 << 2,
  GDK_MOD1_MASK = 1 << 3, GDK_BUTTON1_MASK = 1 << 8, GDK_BUTTON2_MASK = 1 << 9,
  GDK_BUTTON3_MASK = 1 << 10
} GdkModifierType;
typedef enum { GDK_SCROLL_UP, GDK_SCROLL_DOWN, GDK_SCROLL_LEFT, GDK_SCROLL_RIGHT,
               GDK_SCROLL_SMOOTH } GdkScrollDirection;
#define GDK_BUTTON_PRIMARY 1
#define GDK_BUTTON_MIDDLE 2
#define GDK_BUTTON_SECONDARY 3

typedef struct {
  int type; GdkWindow *window; double x, y; guint state; guint button; GdkDevice *device;
} GdkEventButton;
typedef struct {
  int type; GdkWindow *window; double x, y; guint state; GdkDevice *device;
} GdkEventMotion;
typedef struct {
  int type; GdkWindow *window; double x, y; guint state; GdkScrollDirection direction;
  double delta_x, delta_y; GdkDevice *device;
} GdkEventScroll;
typedef struct { int type; GdkWindow *window; guint state; guint keyval; } GdkEventKey;
typedef union { int type; GdkEventButton button; GdkEventMotion motion; GdkEventScroll scroll;
                GdkEventKey key; } GdkEvent;

guint gdk_threads_add_timeout_full(gint priority, guint interval_ms, GSourceFunc function,
                                   gpointer data, GDestroyNotify notify);
guint gdk_threads_add_idle(GSourceFunc function, gpointer data);
GdkWindow *gdk_window_get_device_position(GdkWindow *window, GdkDevice *device, gint *x, gint *y,
                                          GdkModifierType *mask);

#ifdef __cplusplus
}
#endif
#endif
