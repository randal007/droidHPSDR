/*
 * glib.h — droidHPSDR compat shim, NOT GLib.
 *
 * Just enough of GLib's API for the upstream deskHPSDR files that droidHPSDR
 * compiles unchanged (old_protocol.c, receiver.c, ...). Locks and threads
 * are plain pthreads. Main-loop calls (g_idle_add, timeouts) go to a small
 * queue that the host program drains on its own thread; see glib_compat.c.
 *
 * Every file in one build must include this same header: GMutex is embedded
 * by value in upstream structs, so all of them must agree on its size.
 *
 * GPL v3 or later, like the rest of droidHPSDR.
 */
#ifndef DROID_COMPAT_GLIB_H
#define DROID_COMPAT_GLIB_H

#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int gboolean;
typedef char gchar;
typedef unsigned char guchar;
typedef int gint;
typedef unsigned int guint;
typedef short gshort;
typedef unsigned short gushort;
typedef long glong;
typedef unsigned long gulong;
typedef int8_t gint8;
typedef uint8_t guint8;
typedef int16_t gint16;
typedef uint16_t guint16;
typedef int32_t gint32;
typedef uint32_t guint32;
typedef int64_t gint64;
typedef uint64_t guint64;
typedef float gfloat;
typedef double gdouble;
typedef size_t gsize;
typedef ssize_t gssize;
typedef void *gpointer;
typedef const void *gconstpointer;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef CLAMP
#define CLAMP(x, lo, hi) (((x) > (hi)) ? (hi) : (((x) < (lo)) ? (lo) : (x)))
#endif
#define G_N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))
#define G_GNUC_UNUSED __attribute__((unused))
#define G_GNUC_PRINTF(f, a) __attribute__((format(printf, f, a)))
#define G_LIKELY(x) __builtin_expect(!!(x), 1)
#define G_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define GINT_TO_POINTER(i) ((gpointer)(intptr_t)(i))
#define GPOINTER_TO_INT(p) ((gint)(intptr_t)(p))
#define GUINT_TO_POINTER(u) ((gpointer)(uintptr_t)(u))
#define GPOINTER_TO_UINT(p) ((guint)(uintptr_t)(p))
#define G_SOURCE_REMOVE FALSE
#define G_SOURCE_CONTINUE TRUE
#define G_PRIORITY_HIGH -100
#define G_PRIORITY_DEFAULT 0
#define G_PRIORITY_HIGH_IDLE 100
#define G_PRIORITY_DEFAULT_IDLE 200
#define G_USEC_PER_SEC 1000000

typedef gboolean (*GSourceFunc)(gpointer user_data);
typedef gpointer (*GThreadFunc)(gpointer data);
typedef void (*GDestroyNotify)(gpointer data);

/* ---- memory ---- */
#define g_malloc(n) malloc(n)
#define g_malloc0(n) calloc(1, (n))
#define g_realloc(p, n) realloc((p), (n))
#define g_free(p) free(p)
#define g_free_sized(p, n) free(p)
static inline gpointer g_malloc_n(gsize n, gsize size) { return malloc(n * size); }
static inline gpointer g_malloc0_n(gsize n, gsize size) { return calloc(n, size); }
#define g_new(type, n) ((type *)g_malloc_n((n), sizeof(type)))
#define g_new0(type, n) ((type *)g_malloc0_n((n), sizeof(type)))
static inline gchar *g_strdup(const gchar *s) { return s ? strdup(s) : NULL; }
gsize g_strlcpy(gchar *dest, const gchar *src, gsize dest_size);

/* ---- locks ---- */
typedef struct { pthread_mutex_t m; } GMutex;
typedef struct { pthread_mutex_t m; } GRecMutex;
typedef struct { pthread_cond_t c; } GCond;
void g_mutex_init(GMutex *mutex);
static inline void g_mutex_clear(GMutex *mutex) { pthread_mutex_destroy(&mutex->m); }
static inline void g_mutex_lock(GMutex *mutex) { pthread_mutex_lock(&mutex->m); }
static inline void g_mutex_unlock(GMutex *mutex) { pthread_mutex_unlock(&mutex->m); }
static inline gboolean g_mutex_trylock(GMutex *mutex) { return pthread_mutex_trylock(&mutex->m) == 0; }
void g_rec_mutex_init(GRecMutex *mutex);
static inline void g_rec_mutex_lock(GRecMutex *mutex) { pthread_mutex_lock(&mutex->m); }
static inline void g_rec_mutex_unlock(GRecMutex *mutex) { pthread_mutex_unlock(&mutex->m); }
static inline void g_cond_init(GCond *cond) { pthread_cond_init(&cond->c, NULL); }
static inline void g_cond_signal(GCond *cond) { pthread_cond_signal(&cond->c); }
static inline void g_cond_wait(GCond *cond, GMutex *mutex) { pthread_cond_wait(&cond->c, &mutex->m); }

/* ---- atomics (GLib's are full barriers; so are these) ---- */
#define g_atomic_int_inc(p) ((void)__atomic_add_fetch((p), 1, __ATOMIC_SEQ_CST))
#define g_atomic_int_dec_and_test(p) (__atomic_sub_fetch((p), 1, __ATOMIC_SEQ_CST) == 0)
#define g_atomic_int_get(p) __atomic_load_n((p), __ATOMIC_SEQ_CST)
#define g_atomic_int_set(p, v) __atomic_store_n((p), (v), __ATOMIC_SEQ_CST)
#define g_atomic_int_add(p, v) __atomic_fetch_add((p), (v), __ATOMIC_SEQ_CST)

/* ---- threads ---- */
typedef struct _GThread GThread;
GThread *g_thread_new(const gchar *name, GThreadFunc func, gpointer data);
gpointer g_thread_join(GThread *thread);
void g_thread_exit(gpointer retval);
static inline void g_usleep(gulong usec) { usleep(usec); }
gint64 g_get_monotonic_time(void);
gint64 g_get_real_time(void);

/* ---- main loop (queued to the host program, see glib_compat.c) ---- */
typedef struct _GMainContext GMainContext;
typedef struct _GSource GSource;
guint g_idle_add(GSourceFunc function, gpointer data);
guint g_idle_add_full(gint priority, GSourceFunc function, gpointer data, GDestroyNotify notify);
guint g_timeout_add(guint interval_ms, GSourceFunc function, gpointer data);
guint g_timeout_add_full(gint priority, guint interval_ms, GSourceFunc function, gpointer data,
                         GDestroyNotify notify);
gboolean g_source_remove(guint tag);
GSource *g_main_current_source(void);
guint g_source_get_id(GSource *source);
gboolean g_main_context_iteration(GMainContext *context, gboolean may_block);

/* ---- objects (only used on GUI widgets, which droidHPSDR never creates) ---- */
typedef struct _GObject GObject;
typedef struct _GTypeInstance GTypeInstance;
typedef void (*GWeakNotify)(gpointer data, GObject *where_the_object_was);
#define G_OBJECT(o) ((GObject *)(o))
static inline void g_object_weak_ref(GObject *o, GWeakNotify n, gpointer d) { (void)o; (void)n; (void)d; }

/* ---- printing ---- */
void g_print(const gchar *format, ...) G_GNUC_PRINTF(1, 2);

/* Hook for the host program: runs queued g_idle_add/g_timeout_add callbacks
 * that are due. Call it regularly from one thread (the "UI" thread). */
void droid_compat_run_pending(void);

#ifdef __cplusplus
}
#endif
#endif
