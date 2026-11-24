/*
 * droid_headless.h — shared declarations for droidHPSDR's headless build.
 * GPL v3 or later.
 */
#ifndef DROID_HEADLESS_H
#define DROID_HEADLESS_H
#include <stdatomic.h>

/* Packets the radio's sequence numbers say we missed (stubs.c). */
extern atomic_uint droid_sequence_errors;

/* printf to stdout and, on Android, to logcat (tag "droidHPSDR"). */
void droid_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
