#ifndef COLOR_H_
#define COLOR_H_

#include "misc.h"

#define COLOR_STATE_IDLE    0
#define COLOR_STATE_ACTIVE  1
#define COLOR_STATE_DONE    2

struct color_ctx {
    uint32_t    start;
    uint8_t     state;
    uint8_t     idx;
};

void color_init(void);

void color_loop(void);

#endif
