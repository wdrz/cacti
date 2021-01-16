#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "messages.h"
#include "err.h"
#include "cacti.h"
#include "queue.h"
#include "blocking_queue.h"

typedef struct actor {
    role_t role;
    queue_t messages;

    pthread_mutex_t lock;

    int processed_now;
    int goodbye;
    void **stateptr;

} actor_t;

typedef struct actors {
    pthread_mutex_t lock;
    int num;
    actor_t* actors[CAST_LIMIT];

    int n_processed_now;
    blocking_queue_t* waiting;

} actors_t;

volatile actors_t AC;

static actor_t* generate_actor(role_t *const role) {

    actor_t* created_actor = safe_malloc(sizeof(actor_t));

    created_actor->role = role;
    created_actor->messages = queue_init();

    // init mutex
    if ((err = pthread_mutex_init(created_actor->lock, 0)) != 0)
        syserr(err, "mutex init failed");

    created_actor->stateptr = NULL;
    created_actor->goodbye = 0;
    created_actor->processed_now = 0;

    return created_actor;
}

int init_messages(actor_id_t *actor, role_t *const role) {
    // init mutex
    if ((err = pthread_mutex_init(&AC.lock, 0)) != 0)
        syserr(err, "mutex init failed");

    AC.num = 1;
    AC.n_processed_now = 0;
    AC.waiting = blocking_queue_init();
    *actor = 0;

    actor_t* first_actor = generate_actor(role);
    AC.actors[0] = first_actor;

    blocking_queue_push(AC.waiting, 0);

}

int new_actor(actor_id_t *actor, role_t *const role) {

    safe_lock(&AC.lock);

    actor_t* next_actor = generate_actor(role);
    AC.actors[AC.num] = next_actor;
    AC.num++;

    safe_unlock(&AC.lock);

}

inline int is_actor_queued(actor_t* actor_temp) {
    return actor_temp->processed_now == 0 && !queue_empty(actor_temp->messages);
}

int add_message(actor_id_t actor, message_t message) {
    actor_t* actor_temp = AC.actors[actor];

    safe_lock(&(actor_temp->lock));
    queue_push(actor_temp->messages, (void *) message);

    if (!is_actor_queued(actor_temp)) {
        blocking_queue_push(AC.waiting, actor); // this queue is synchronised
    }
    safe_unlock(&(actor_temp->lock));
}

int have_actors_finished() {
    int res;
    safe_lock(&(AC->lock));
    res = (AC.n_processed_now == 0 && blocking_queue_empty(AC.waiting));
    safe_unlock(&(AC->lock));
    return res;
}

int next_computation(computation_t *result) {
    if (have_actors_finished()) {
        // ..... free resources
        // join all threads, finish computation
        return -1;
    }

    // blocking instruction
    actor_id_t id = blocking_queue_pop(AC.waiting);

    if (id == -1) {
        // ..... free resources
        // join all threads, finish computation
        return -1;
    }

    actor_t *actor_temp = AC.actors[id];

    safe_lock(&(actor_temp->lock));

    result->message = (message_t*) queue_pop(actor_temp->messages);
    actor_temp->processed_now = 1;
    result->actor = id;
    result->stateptr = actor_temp->stateptr;
    result->prompt = actor_temp->role.prompts[ result->message->message_type ];

    safe_unlock(&(actor_temp->lock));
}

void computation_ended(actor_id_t actor) {
    actor_t* actor_temp = AC.actors[actor];

    safe_lock(&(actor_temp->lock));

    actor_temp->processed_now = 0;
    if (!is_actor_queued(actor_temp)) {
        queue_push(AC.waiting, (void *) (intptr_t) actor); // this queue is synchronised
    }
    safe_unlock(&(actor_temp->lock));
}