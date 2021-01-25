#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "err.h"
#include "cacti.h"
#include "pool.h"
#include "messages.h"

typedef struct thread_pool_t {
    pthread_key_t curr_actor;
    pthread_attr_t attr;
    pthread_t tid[POOL_SIZE];
    pthread_mutex_t lock;
    int ended;
} thread_pool;


typedef struct thread_specific {
    actor_id_t actor;
} thread_specific_t;


thread_pool TP; ///< System's thread pool

/**
 * Calls new_actor function and sends MSG_HELLO to the new actor
 * @return 0 on success, -1 on failure
 */
int execute_spawn(message_t message) {
    int err;
    actor_id_t new_actor_id;

    if ((err = new_actor(&new_actor_id, (role_t*)message.data)) != 0) {
        return err;
    }

    //fprintf (stdout, "HELLO MESSAGE SENT: %ld \n", actor_id_self());
    message_t hello_message = {
            .message_type = MSG_HELLO,
            .nbytes = sizeof(actor_id_t),
            .data = (void*) actor_id_self()
    };

    return send_message(new_actor_id, hello_message);
}

static void pool_clean() {
    int err;
    if ((err = pthread_key_delete(TP.curr_actor)) != 0)
        syserr(err, "key delete failed");

    if ((err = pthread_mutex_destroy (&TP.lock)) != 0)
        syserr (err, "mutex destroy failed (pool.c)");

    if ((err = pthread_attr_destroy (&TP.attr)) != 0)
        syserr(err, "attr destroy failed");

}

/**
 * The life of a thread.
 * @param data     - number of this thread
 * @return         NULL
 */
void *worker(void* data) {
    int id = *(int*) data;
    int is_last = 0;
    free(data);

    pthread_t tid = pthread_self();
    TP.tid[id] = tid;

    // thread specific data
    thread_specific_t *ts = malloc(sizeof(thread_specific_t));
    pthread_setspecific(TP.curr_actor, ts);

    computation_t c;

    while (1) {
        if (next_computation(&c) == -1) {
            break;
        }
        ts->actor = c.actor;

        switch (c.message.message_type) {
            case MSG_SPAWN:
                execute_spawn(c.message); break;

            case MSG_GODIE:
                kill_actor(); break;

            default:
                (*(c.prompt))(c.stateptr, c.message.nbytes, c.message.data); break;
        }
        computation_ended(c.actor);
    }
    safe_lock(&TP.lock);
    TP.ended++;
    if (TP.ended == POOL_SIZE) {
        is_last = 1;
    }
    safe_unlock(&TP.lock);

    if (is_last) {
        // last thread, must clean the system
        pool_clean();
        messages_destroy();
    }

    return NULL;
}

void init_tp() {
    int i, err;

    if ((err = pthread_attr_init(&TP.attr)) != 0 )
        syserr(err, "attr_init");

    if ((err = pthread_attr_setdetachstate(&TP.attr, PTHREAD_CREATE_JOINABLE)) != 0)
        syserr(err, "set detach state");

    // set up thread specific struct
    if ((err = pthread_key_create(&TP.curr_actor, NULL)) != 0)
        syserr(err, "key create failed");

    TP.ended    = 0;
    // init mutex
    if ((err = pthread_mutex_init(&TP.lock, 0)) != 0)
        syserr(err, "mutex init failed");

    // create threads
    int* t_num;
    for (i = 0; i < POOL_SIZE; ++i) {
        t_num = safe_malloc(sizeof(int));
        *t_num = i;
        if ((err = pthread_create(&TP.tid[i], &TP.attr, worker, (void*) t_num)) != 0) {
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
    thread_specific_t *tp = pthread_getspecific(TP.curr_actor);
    if (tp == NULL) {
        fatal("actor_id_self used incorrectly");
    }
    return tp->actor;
}

void actor_system_join(actor_id_t actor) {
    (void)(actor); // suppress unused argument warning
    int i, err;
    for (i = 0; i < POOL_SIZE; ++i) {
        if ((err = pthread_join(TP.tid[i], NULL)) != 0)
            syserr(err, "join failed");
    }
}