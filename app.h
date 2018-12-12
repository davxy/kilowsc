#ifndef APP_H_
#define APP_H_

#include "chan.h"
#include "discover.h"
#include "spt.h"
#include "color.h"
#include "wsc.h"
#include "misc.h"

#define APP_PROTO_DIS   0   /* Discovery */
#define APP_PROTO_SPT   1   /* Spanning tree construction (multi-shout) */
#define APP_PROTO_COL   2   /* Colors distribution */
#define APP_PROTO_WSC   3   /* Witch says color (The game) */

#define NEIGHBORS_MAX   32

struct app_ctx {
    struct chan_ctx chan;
    uint8_t         nneighbors;
    addr_t          neighbors[NEIGHBORS_MAX];
    uint8_t         color;
    uint8_t         nodes;
    uint8_t         proto;
    union {
        struct discover_ctx  discover;
        struct spt_ctx       spt;
        struct color_ctx     col;
        struct wsc_ctx       wsc;
    };
};

typedef struct app_ctx app_ctx_t;

extern struct app_ctx *mydata;

#define myapp mydata

#endif /* APP_H_ */
