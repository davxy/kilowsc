#ifndef DISCOVER_H_
#define DISCOVER_H_

#include "misc.h"


#define DISCOVERY_TIME          (10 * KILO_TICKS_PER_SEC)

#define DISCOVERY_RAND_OFF \
        (rand() % (3 * KILO_TICKS_PER_SEC))

#define DISCOVER_STATE_IDLE     0
#define DISCOVER_STATE_ACTIVE   1
#define DISCOVER_STATE_DONE     2

struct discover_ctx {
    uint8_t  state;    /* Current state */
    uint32_t start;    /* Protocol start time */
    uint32_t next;     /* Next discovery time */
};

typedef struct discover_ctx discover_ctx_t;

void discover_init(void);

void discover_loop(void);


#endif /* DISCOVER_H_ */
