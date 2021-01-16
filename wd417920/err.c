#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "err.h"

// extern int sys_nerr;

void syserr(int e, const char *fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end (fmt_args);
    fprintf(stderr," (%d; %s)\n", bl, strerror(bl));
    exit(1);
}

void fatal(const char *fmt, ...) {
  va_list fmt_args;

  fprintf(stderr, "ERROR: ");

  va_start(fmt_args, fmt);
  vfprintf(stderr, fmt, fmt_args);
  va_end (fmt_args);

  fprintf(stderr,"\n");
  exit(1);
}


void* safe_malloc_help(size_t n, unsigned long line) {
    void* p = malloc(n);
    if (!p) {
        fprintf(stderr, "[%s:%ul] Out of memory (%ul bytes)\n",
                __FILE__, line, (unsigned long) n);
        exit(1);
    }
    return p;
}