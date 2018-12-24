#ifndef WSC_H_
#define WSC_H_

#include "misc.h"

/** Begin state, game not started. */
#define WSC_STATE_IDLE      0
/** Playing */
#define WSC_STATE_ACTIVE    1

/** Color is set */
#define WSC_FLAG_COLOR_ON   (1 << 0)
/** Hunter approaching to the target */
#define WSC_FLAG_APPROACH   (1 << 1)

/**Protocol context */
struct wsc_ctx {
    uint32_t    dina_tick;
    uint32_t    echo_tick;
    uint32_t    blink_tick;
    uint32_t    move_tick;
    uint8_t     flags;
    uint8_t     state;
    uint8_t     nodes;
    uint8_t     target;
    uint8_t     dist;
    uint8_t     dist_src;
    uint8_t     move;
};

/** Protocol initialization */
void wsc_init(void);

/** Protocol loop */
void wsc_loop(void);

#endif
