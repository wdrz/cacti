#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "err.h"
#include "cacti.h"
#include "pool.h"
#include "messages.h"

typedef struct thread_pool_t {
    pthread_t tid[POOL_SIZE];
} thread_pool;

thread_pool TP;

int execute_spawn(computation_t *c) {
    int err;
    actor_id_t new_actor_id;

    if ((err = new_actor(&new_actor_id, (role_t*)message.data)) != 0) {
        return err;
    }

    message_t hello_message = {
            .message_type = MSG_HELLO,
            .nbytes = sizeof(actor_id_t),
            .data = (void*) (intptr_t) actor_id_self()
    };

    return send_message(new_actor_id, hello_message);
}

/**
 * The life of a thread.
 * @param data     - number of this thread
 * @return         NULL
 */
void *worker(void* data) {
    int id = (int) data;

    pthread_t tid = pthread_self();
    TP.tid[id] = tid;

    computation c;

    while(1) {
        if (next_computation(&c) == -1) {
            break;
        }

        switch (c.message->message_type) {
            case MSG_SPAWN:
                execute_spawn();
                break;

            case MSG_GODIE:
                say_goodbye(c.actor);
                break;

            default:
                (*(c.prompt))(c.stateptr, c.message->nbytes, c.message->data);
                computation_ended(c.actor);
                break;
        }
    }
}

void init_tp() {
    int i, err;
    pthread_attr_t attr;

    if ((err = pthread_attr_init(&attr)) != 0 )
        syserr(err, "attr_init");

    if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0)
        syserr(err, "set detach state");

    TP.pool_working = 1;
    COM.pending = 0;

    // create threads
    for (i = 0; i < POOL_SIZE; ++i) {
        /*int* id = (int*)safe_malloc(sizeof(int));
        *id = i;*/

        if ((err = pthread_create(&TP.tid[i], &attr, worker, (void*) i)) != 0) {
            syserr(err, "create");
        }
    }
}
/*
int interrupt_all() {
    int i, err;
    TP.pool_working = 0;
    for (i = 0; i < POOL_SIZE; i++) {
        if ((err = pthread_join(TP.tid[i], NULL)) != 0) {
            syserr(err, "join failed");
        }
    }
}*/

actor_id_t actor_id_self() {
    pthread_t tid = pthread_self();
    int i;
    for (i = 0; i < POOL_SIZE; ++i) {
        if (pthread_equal(tid, TP.tid[i])) {
            return (actor_id_t) i;
        }
    }
    fatal("actor_id_self used incorrectly");
    return -1;
}