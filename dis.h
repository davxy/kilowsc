#ifndef DIS_H_
#define DIS_H_

#include "misc.h"



/**
 * Discovery protocol state @{
 */
/** Protocol not started */
#define DIS_STATE_IDLE     0
/** Protocol started */
#define DIS_STATE_ACTIVE   1
/** Protocol finished */
#define DIS_STATE_DONE     2
/** @} */

/** Discovery protocol context */
struct dis_ctx {
    uint8_t  state;    /* Current state */
    uint32_t start;    /* Protocol start time */
    uint32_t next;     /* Next discovery time */
};

typedef struct dis_ctx dis_ctx_t;

void dis_init(void);

void dis_loop(void);

#endif /* DIS_H_ */
