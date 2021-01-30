#include <stdio.h>
#include <stdlib.h>

#include "cacti.h"
#include "err.h"

#define UNUSED_PARAMETER(x) (void)(x)

#define MSG_HI_BACK  (message_type_t)0x1
#define MSG_COMPUTE  (message_type_t)0x2

typedef struct partial {
    int k;
    long long int k_factorial;
    int n;
    role_t *role;
} partial_t;

void callback_hello(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(stateptr);
    UNUSED_PARAMETER(nbytes);

    int err;
    actor_id_t parent = (actor_id_t) data;

    message_t message = {
            .message_type   = MSG_HI_BACK,
            .data           = (void*) actor_id_self()
    };

    if ((err = send_message(parent, message)) != 0) {
        syserr(err, "send_message HI_BACK failed");
    }

}

void callback_hi_back(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(nbytes);
    int err;
    actor_id_t child = (actor_id_t) data;

    message_t message = {
            .message_type   = MSG_COMPUTE,
            .data           = *stateptr
    };

    if ((err = send_message(child, message)) != 0) {
        syserr(err, "send_message MSG_COMPUTE failed");
    }

    //free(stateptr);

    message_t message_godie = { .message_type = MSG_GODIE };
    if ((err = send_message(actor_id_self(), message_godie)) != 0) {
        syserr(err, "send_message GODIE failed");
    }
}

void callback_computation(void **stateptr, size_t nbytes, void *data) {
    UNUSED_PARAMETER(stateptr);
    UNUSED_PARAMETER(nbytes);

    int err;

    *stateptr = data;

    partial_t *partial = data;
    partial->k += 1;
    partial->k_factorial *= partial->k;

    if (partial->k == partial->n) {
        //free(stateptr);

        message_t message_godie = { .message_type = MSG_GODIE };
        if ((err = send_message(actor_id_self(), message_godie)) != 0) {
            syserr(err, "send_message GODIE failed");
        }

    } else {
        message_t message = {
                .message_type   = MSG_SPAWN,
                .data           = partial->role
        };

        if ((err = send_message(actor_id_self(), message)) != 0) {
            syserr(err, "send_message MSG_SPAWN failed");
        }
    }

}

int main(){
    int n, err;
    scanf("%d", &n);

    act_t actions[] = {
            callback_hello,
            callback_hi_back,
            callback_computation
    };

    act_t first_actions[] = {
            NULL,
            callback_hi_back,
            callback_computation
    };

    /* * * * * * * * * * *
     *    First actor    *
     * * * * * * * * * * */
    actor_id_t first_actor;

    role_t first_role = {
            .nprompts = sizeof(actions) / sizeof(act_t),
            .prompts = first_actions
    };

    role_t role = {
            .nprompts = sizeof(actions) / sizeof(act_t),
            .prompts = actions
    };

    actor_system_create(&first_actor, &first_role);

    partial_t partial = {
            .k = 0,
            .k_factorial = 1,
            .n = n,
            .role = &role
    };

    message_t message = {
            .message_type = MSG_COMPUTE,
            .data = &partial
    };

    if ((err = send_message(first_actor, message)) != 0) {
        syserr(err, "send_message COMPUTE failed");
    }

    actor_system_join(first_actor);

    printf("%lld\n", partial.k_factorial);

	return 0;
}
