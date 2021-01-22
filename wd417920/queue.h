// Unsynchronised module

#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>

typedef struct entry {
    void *data;
    entry_t *prev;
} entry_t;

typedef struct queue {
    size_t len;
    entry_t *back;
    entry_t *front;
} queue_t;

extern queue_t* queue_init();

extern int queue_empty(queue_t* q);

extern int queue_push(queue_t* q, void* data);

extern void* queue_pop(queue_t* q);

extern int queue_destroy(queue_t* q);

#endif
