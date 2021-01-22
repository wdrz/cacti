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

void actor_system_join(actor_id_t actor) {

}