#ifndef _ERR_
#define _ERR_

#include <stdio.h>
#include <pthread.h>

/* wypisuje informacje o blednym zakonczeniu funkcji systemowej
i konczy dzialanie */
extern void syserr(int e, const char *fmt, ...);

/* wypisuje informacje o bledzie i konczy dzialanie */
extern void fatal(const char *fmt, ...);

extern void* safe_malloc_help(size_t n, unsigned long line);

#define safe_malloc(n) safe_malloc_help(n, __LINE__)

static inline void safe_lock(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_lock(mutex)) != 0)
        syserr(err, "lock failed");
}

static inline void safe_unlock(pthread_mutex_t *mutex) {
    int err;
    if ((err = pthread_mutex_unlock(mutex)) != 0)
        syserr(err, "unlock failed");
}

#endif
