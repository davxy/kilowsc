#ifndef WSC_H_
#define WSC_H_

#include "misc.h"

/**
 * Protocol states. @{
 */
/** Begin state, game not started. */
#define WSC_STATE_IDLE      0
/** Playing */
#define WSC_STATE_ACTIVE    1
/** Relax after catch */
#define WSC_STATE_SLEEP     2
/** Protocol finished */
#define WSC_STATE_DONE      3
/** @} */

/**
 * Protocol flags. @{
 */
/** Color is set */
#define WSC_FLAG_COLOR_ON   (1 << 0)
/** Hunter approaching to the target */
#define WSC_FLAG_APPROACH   (1 << 1)
/** @} */

/** Last match number */
#define WSC_MATCH_MAX       255

/**Hunting context */
struct wsc_ctx {
    uint32_t aging_tick;    /**< Distance aging tick value. */
    uint32_t echo_tick;     /**< Periodic echo event time. */
    uint32_t move_tick;     /**< Next movement tick (hunter). */
    uint32_t blink_tick;    /**< Blink tick (target). */
    uint8_t  flags;         /**< Behavior flags. */
    uint8_t  state;         /**< Protocol state. */
    uint8_t  target;        /**< Target identifier. */
    uint8_t  dist;          /**< Target distance. */
    uint8_t  dist_src;      /**< Target distance source. */
    uint8_t  min_dist;      /**< Closer bot distance. */
    uint8_t  min_dist_src;  /**< Closer bot distance source. */
    uint8_t  move;          /**< Current movement. */
    uint8_t  match_cnt;     /**< Match counter. */
    uint8_t  radius;
};

/** Hunting protocol context type alias. */
typedef struct wsc_ctx wsc_ctx_t;

/** Protocol initialization */
void wsc_init(void);

/** Protocol loop */
void wsc_loop(void);

#endif
