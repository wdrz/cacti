#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "cacti.h"
#include "err.h"
#include "queue.h"
#include "pool.h"
#include "messages.h"

#define UNUSED_PARAMETER(x) (void)(x)

int actor_system_create(actor_id_t *actor, role_t *const role) {
    init_actors_system(actor, role);
    init_tp();
    return 0;
}

void actor_system_join(actor_id_t actor) {
    UNUSED_PARAMETER(actor);
}