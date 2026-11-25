/*
 * glib_compat.c — implementation of droidHPSDR's GLib/GDK/GTK compat shim.
 *
 * Threads and locks are pthreads. g_idle_add / g_timeout_add put the
 * callback on a small queue; the host program drains it by calling
 * droid_compat_run_pending() from one thread, which plays the role of GTK's
 * main loop. GTK widget calls are no-ops: droidHPSDR never creates widgets.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <gtk/gtk.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

/* ---- strings, printing ---- */

gsize g_strlcpy(gchar *dest, const gchar *src, gsize dest_size) {
  gsize len = strlen(src);
  if (dest_size > 0) {
    gsize n = len < dest_size - 1 ? len : dest_size - 1;
    memcpy(dest, src, n);
    dest[n] = '\0';
  }
  return len;
}

void g_print(const gchar *format, ...) {
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  va_end(ap);
}

/* ---- locks ---- */

void g_mutex_init(GMutex *mutex) {
  pthread_mutex_init(&mutex->m, NULL);
}

void g_rec_mutex_init(GRecMutex *mutex) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutex->m, &attr);
  pthread_mutexattr_destroy(&attr);
}

/* ---- threads ---- */

struct _GThread {
  pthread_t tid;
  GThreadFunc func;
  gpointer data;
  char name[16];
};

static void *thread_trampoline(void *arg) {
  GThread *t = arg;
  if (t->name[0]) {
    pthread_setname_np(pthread_self(), t->name);
  }
  return t->func(t->data);
}

GThread *g_thread_new(const gchar *name, GThreadFunc func, gpointer data) {
  GThread *t = calloc(1, sizeof *t);
  t->func = func;
  t->data = data;
  if (name) {
    g_strlcpy(t->name, name, sizeof t->name);   /* Linux limit: 15 chars */
  }
  if (pthread_create(&t->tid, NULL, thread_trampoline, t) != 0) {
    fprintf(stderr, "g_thread_new(%s): pthread_create failed\n", name ? name : "?");
    abort();   /* GLib aborts too */
  }
  return t;
}

gpointer g_thread_join(GThread *thread) {
  void *ret = NULL;
  pthread_join(thread->tid, &ret);
  free(thread);
  return ret;
}

void g_thread_exit(gpointer retval) {
  pthread_exit(retval);
}

gint64 g_get_monotonic_time(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (gint64)ts.tv_sec * G_USEC_PER_SEC + ts.tv_nsec / 1000;
}

gint64 g_get_real_time(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (gint64)ts.tv_sec * G_USEC_PER_SEC + ts.tv_nsec / 1000;
}

/* ---- main loop replacement ---- */

#define MAX_SOURCES 256

struct _GSource {
  guint id;              /* 0 = free slot */
  GSourceFunc func;
  gpointer data;
  GDestroyNotify notify;
  guint interval_ms;     /* 0 = idle (run once per pass until it returns FALSE) */
  gint64 due_us;
};

static struct _GSource sources[MAX_SOURCES];
static pthread_mutex_t sources_lock = PTHREAD_MUTEX_INITIALIZER;
static guint next_id = 1;
static __thread GSource *current_source;

static guint add_source(GSourceFunc func, gpointer data, GDestroyNotify notify, guint interval_ms) {
  guint id = 0;
  pthread_mutex_lock(&sources_lock);
  for (int i = 0; i < MAX_SOURCES; i++) {
    if (sources[i].id == 0) {
      id = next_id++;
      if (next_id == 0) next_id = 1;
      sources[i] = (struct _GSource){ id, func, data, notify, interval_ms,
                                      g_get_monotonic_time() + (gint64)interval_ms * 1000 };
      break;
    }
  }
  pthread_mutex_unlock(&sources_lock);
  if (id == 0) {
    fprintf(stderr, "glib_compat: source table full, callback dropped\n");
  }
  return id;
}

guint g_idle_add(GSourceFunc function, gpointer data) {
  return add_source(function, data, NULL, 0);
}

guint g_idle_add_full(gint priority, GSourceFunc function, gpointer data, GDestroyNotify notify) {
  (void)priority;
  return add_source(function, data, notify, 0);
}

guint g_timeout_add(guint interval_ms, GSourceFunc function, gpointer data) {
  return add_source(function, data, NULL, interval_ms ? interval_ms : 1);
}

guint g_timeout_add_full(gint priority, guint interval_ms, GSourceFunc function, gpointer data,
                         GDestroyNotify notify) {
  (void)priority;
  return add_source(function, data, notify, interval_ms ? interval_ms : 1);
}

guint gdk_threads_add_timeout_full(gint priority, guint interval_ms, GSourceFunc function,
                                   gpointer data, GDestroyNotify notify) {
  return g_timeout_add_full(priority, interval_ms, function, data, notify);
}

guint gdk_threads_add_idle(GSourceFunc function, gpointer data) {
  return g_idle_add(function, data);
}

gboolean g_source_remove(guint tag) {
  gboolean found = FALSE;
  GDestroyNotify notify = NULL;
  gpointer data = NULL;
  pthread_mutex_lock(&sources_lock);
  for (int i = 0; i < MAX_SOURCES; i++) {
    if (sources[i].id == tag && tag != 0) {
      notify = sources[i].notify;
      data = sources[i].data;
      sources[i].id = 0;
      found = TRUE;
      break;
    }
  }
  pthread_mutex_unlock(&sources_lock);
  if (notify) notify(data);
  return found;
}

GSource *g_main_current_source(void) { return current_source; }
guint g_source_get_id(GSource *source) { return source ? source->id : 0; }

/* Upstream code calls this to keep a GTK UI responsive while it waits.
 * There is no UI loop to turn here. */
gboolean g_main_context_iteration(GMainContext *context, gboolean may_block) {
  (void)context; (void)may_block;
  return FALSE;
}

static pthread_mutex_t once_lock = PTHREAD_MUTEX_INITIALIZER;

gboolean g_once_init_enter(volatile void *location) {
  if (__atomic_load_n((volatile gsize *)location, __ATOMIC_ACQUIRE) != 0) return FALSE;
  pthread_mutex_lock(&once_lock);
  if (*(volatile gsize *)location != 0) {
    pthread_mutex_unlock(&once_lock);
    return FALSE;
  }
  return TRUE;
}

void g_once_init_leave(volatile void *location, gsize result) {
  __atomic_store_n((volatile gsize *)location, result, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&once_lock);
}

void droid_compat_run_pending(void) {
  gint64 now = g_get_monotonic_time();
  for (int i = 0; i < MAX_SOURCES; i++) {
    struct _GSource copy;
    pthread_mutex_lock(&sources_lock);
    copy = sources[i];
    pthread_mutex_unlock(&sources_lock);
    if (copy.id == 0 || copy.due_us > now) continue;

    GSource handle = copy;
    current_source = &handle;
    gboolean again = copy.func(copy.data);
    current_source = NULL;

    pthread_mutex_lock(&sources_lock);
    /* The callback may have removed itself or been replaced meanwhile. */
    if (sources[i].id == copy.id) {
      if (again) {
        sources[i].due_us = now + (gint64)copy.interval_ms * 1000;
      } else {
        sources[i].id = 0;
      }
    }
    pthread_mutex_unlock(&sources_lock);
    if (!again && copy.notify) copy.notify(copy.data);
  }
}

/* ---- GDK / GTK: no widgets exist, so these never have anything to do ---- */

GdkWindow *gdk_window_get_device_position(GdkWindow *window, GdkDevice *device, gint *x, gint *y,
                                          GdkModifierType *mask) {
  (void)window; (void)device;
  if (x) *x = 0;
  if (y) *y = 0;
  if (mask) *mask = 0;
  return NULL;
}

GtkWidget *gtk_fixed_new(void) { return NULL; }
void gtk_fixed_put(GtkFixed *f, GtkWidget *w, gint x, gint y) { (void)f; (void)w; (void)x; (void)y; }
void gtk_fixed_move(GtkFixed *f, GtkWidget *w, gint x, gint y) { (void)f; (void)w; (void)x; (void)y; }
void gtk_container_remove(GtkContainer *c, GtkWidget *w) { (void)c; (void)w; }
void gtk_widget_set_size_request(GtkWidget *w, gint width, gint height) { (void)w; (void)width; (void)height; }
void gtk_widget_show_all(GtkWidget *w) { (void)w; }
void gtk_widget_queue_draw(GtkWidget *w) { (void)w; }
