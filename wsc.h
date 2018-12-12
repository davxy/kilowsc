#ifndef WSC_H_
#define WSC_H_

#include "misc.h"

#define WSC_STATE_IDLE    0
#define WSC_STATE_ACTIVE  1
#define WSC_STATE_DONE    2



struct wsc_ctx {
    uint8_t     state;
    uint8_t     sstate;
    uint8_t     nodes;
    uint8_t     target;
    uint32_t    dina_tick;
    uint32_t    echo_tick;
    uint32_t    blink_tick;
    uint32_t    move_tick;
    uint8_t     color_enabled;
    uint8_t     idx;
    uint8_t     dist;
    uint8_t     dist_src;
    uint8_t     move;
    uint8_t     pmove;
};

void wsc_init(void);

void wsc_loop(void);

#endif
