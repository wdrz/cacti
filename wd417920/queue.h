// Unsynchronised module

#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include "cacti.h"

typedef message_t content_t;

typedef struct queue {
    volatile size_t len;
    volatile size_t capacity;
    content_t* list;
    size_t volatile back;
    size_t volatile front;
} queue_t;

extern queue_t* queue_init();

extern int queue_empty(queue_t* q);

/**
 * Push back.
 * @param q     - pointer to a queue
 * @param data  - data to be inserted
 * @return      0 on success, -1 on failure
 */
extern int queue_push(queue_t* q, content_t data);

extern content_t queue_pop(queue_t* q);

extern int queue_destroy(queue_t* q);

#endif
