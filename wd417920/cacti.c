#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "cacti.h"
#include "err.h"
#include "queue.h"
#include "pool.h"
#include "messages.h"


int actor_system_create(actor_id_t *actor, role_t *const role) {
    init_messages(actor, role);
    init_tp();
}

int send_message(actor_id_t actor, message_t message) {
    int err;
    switch (message.message_type) {
        case MSG_SPAWN:
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

            break;
        case MSG_GODIE:

            break;
        case MSG_HELLO:

            break;

        default:

    }
}