#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "cacti.h"
#include "err.h"

#define UNUSED_PARAMETER(x) (void)(x)

#define MSG_INIT     (message_type_t)0x1
#define MSG_HI_BACK  (message_type_t)0x2
#define MSG_READY    (message_type_t)0x3
#define MSG_SETSTATE (message_type_t)0x4
#define MSG_COMPUTE  (message_type_t)0x5
#define MSG_FREE     (message_type_t)0x6




typedef struct actor_state {
    int column_number;
    actor_id_t actor_id_prev;
    actor_id_t actor_id_first;
    int n_actors_system;
    int *times;
    int *cells;
    int n_rows;
    int *result;                 // used only by first actor
    role_t *role;                // used only by first actor
    int n_actors_curr;           // used only by first actor
    int n_actors_ready;          // used only by first actor
    int n_rows_counted;          // used only by first actor
} actor_state_t;

typedef struct partial {
    int result;
    int row;
} partial_t;



/** Called when an actor receives HELLO message
 *
 * @param stateptr      a pointer to NULL. Function changes NULL to pointer to actor's state
 * @param nbytes        irrelevant
 * @param data          actor_id_t of parent actor
 */
void callback_hello(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(stateptr);
    UNUSED_PARAMETER(nbytes);

    int err;
    actor_id_t parent = (actor_id_t) data;

    message_t message_hi_back = {
            .message_type = MSG_HI_BACK,
            .nbytes = 0, // irrelevant
            .data = (void*) actor_id_self()
    };

    if ((err = send_message(parent, message_hi_back)) != 0) {
        syserr(err, "send_message HI_BACK failed");
    }

}

/** An initial call to the first actor in system
 *
 * @param stateptr  pointer to NULL
 * @param nbytes    irrelevant
 * @param data      pointer to actor_state_t
 */
void callback_init(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(nbytes);
    int err, i;
    *stateptr = data;

    actor_state_t state = **(actor_state_t**) stateptr;

    for (i = 0; i < state.n_actors_system; i++) {
        message_t message_spawn = {
                .message_type = MSG_SPAWN,
                .nbytes = 0, // irrelevant
                .data = (void*) state.role
        };

        if ((err = send_message(actor_id_self(), message_spawn)) != 0) {
            syserr(err, "send_message SPAWN failed");
        }

    }

}

/** Called when the first actor in system receives HI_BACK message
 *
 * @param stateptr  state of actor (actor_state_t**)
 * @param nbytes    irrelevent
 * @param data      actor_id_t of calling actor
 */
void callback_hi_back(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(nbytes);

    int err;
    actor_id_t child = (actor_id_t) data;

    actor_state_t *state = *stateptr;
    actor_state_t *child_state = safe_malloc(sizeof(actor_state_t));
    *child_state = *state; // shallow copy

    state->actor_id_prev = child;

    state->n_actors_curr--;
    child_state->column_number = state->n_actors_curr;

    message_t message_setstate = {
            .message_type = MSG_SETSTATE,
            .nbytes = 0, // irrelevant
            .data = (void*) state
    };

    if ((err = send_message(child, message_setstate)) != 0) {
        syserr(err, "send_message SETSTATE failed");
    }
}


/** Called when the first actor in system receives READY message
 *
 * @param stateptr  state of actor (actor_state_t**)
 * @param nbytes    irrelevent
 * @param data      actor_id_t of calling actor
 */
void callback_ready(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(nbytes);
    UNUSED_PARAMETER(data);

    int err, i;
    actor_state_t *state = *stateptr;
    state->n_actors_ready++;

    // start actual computation when all actors initialized
    if (state->n_actors_ready != state->n_actors_system) {
        return;
    }

    for (i = 0; i < state->n_rows; ++i) {

        partial_t *results = safe_malloc(sizeof(partial_t));
        results->row = i;
        results->result = 0;

        message_t message = {
                .message_type = MSG_COMPUTE,
                .data = (void*) results
        };

        if ((err = send_message(state->actor_id_prev, message)) != 0) {
            syserr(err, "send_message COMPUTE failed");
        }
    }

}


/** Called when actor receives SETSTATE message
 *
 * @param stateptr      state of actor (actor_state_t**)
 * @param nbytes        irrelevant
 * @param data          a pointer to actor_state_t
 */
void callback_setstate(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(nbytes);
    int err;
    *stateptr = data;
    actor_state_t *state = *stateptr;
    message_t message = { .message_type = MSG_READY };

    if ((err = send_message(state->actor_id_first, message)) != 0) {
        syserr(err, "send_message READY failed");
    }

}

/** Called when actor receives COMPUTE message
 *
 * @param stateptr      state of actor (actor_state_t**)
 * @param nbytes        irrelevant
 * @param data          pointer to partial_t
 */
void callback_computation(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(nbytes);

    int err, i;
    partial_t *results = (partial_t*) data;
    actor_state_t *state = *stateptr;
    int cell = results->row * state->n_rows + state->column_number;

    sleep(state->times[ cell ]);
    results->result += state->cells[ cell ];

    // if reached first actor again, stop computation.
    if (state->actor_id_first == actor_id_self()) {
        state->result[results->row] = results->result;
        state->n_rows_counted++;
        free(results);

        if (state->n_rows_counted == state->n_rows) {
            message_t message = { .message_type = MSG_FREE };

            if ((err = send_message(state->actor_id_prev, message)) != 0) {
                syserr(err, "send_message FREE failed");
            }

        }

        return;
    }

    // if first actor not reached yet, continue computation
    message_t message = {
            .message_type = MSG_COMPUTE,
            .data = data
    };

    if ((err = send_message(state->actor_id_prev, message)) != 0) {
        syserr(err, "send_message failed");
    }

}

/**
 *
 * @param stateptr
 * @param nbytes
 * @param data
 */
void callback_free(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(nbytes);
    UNUSED_PARAMETER(data);
    int err;
    actor_state_t *state = *stateptr;

    // Replicate message
    if (state->actor_id_first != actor_id_self()) {
        message_t message = { .message_type = MSG_FREE };

        if ((err = send_message(state->actor_id_prev, message)) != 0) {
            syserr(err, "send_message FREE failed");
        }
    }

    // Free resources
    free(*stateptr);
    free(stateptr);

    // Send goodbye to itself

    message_t message = { .message_type = MSG_GODIE };
    if ((err = send_message(actor_id_self(), message)) != 0) {
        syserr(err, "send_message GODIE failed");
    }



}

int main() {
    int n, k, i, j, err; // k - number of rows, n - number of columns
    scanf("%d %d", &n, &k);
    int num[k * n];
    int time[k * n];

    for (i = 0; i < k; ++i) {
        for (j = 0; j < n; ++j) {
            scanf("%d %d", num + i * k + j, time + i * k + j);
        }
    }

    actor_id_t first_actor;

    const int action_size = 3;
    act_t actions[] = {
            &callback_hello,
            &callback_init,
            &callback_hi_back,
            &callback_ready,
            &callback_setstate,
            &callback_computation,
            &callback_free
    };

    /* * * * * * * * * * *
     *  First actor      *
     * * * * * * * * * * */

    role_t first_role = {
            .nprompts = action_size,
            .prompts = actions
    };

    int result[k];

    actor_state_t first_actor_state = {
        .column_number = n - 1,
        .actor_id_prev = first_actor,
        .actor_id_first = first_actor,
        .n_actors_system = n,
        .times = time,
        .cells = num,
        .n_rows = k,
        .result = result,            // used only by first actor
        .role = &first_role,         // used only by first actor
        .n_actors_curr = 1,          // used only by first actor
        .n_actors_ready = 1,         // used only by first actor
        .n_rows_counted = 0,         // used only by first actor
    };

    message_t message = {
            .message_type = MSG_INIT,
            .data = &first_actor_state
    };

    actor_system_create(&first_actor, &first_role);

    if ((err = send_message(first_actor, message)) != 0) {
        syserr(err, "send_message INIT failed");
    }



	return 0;
}
