#ifndef APP_H_
#define APP_H_

#include "discover.h"
#include "spt.h"
#include "wsc.h"
#include "misc.h"
#include "tpl.h"

#define APP_PROTO_DIS    0   /* Discovery */
#define APP_PROTO_SPT    1   /* Spanning tree construction */
#define APP_PROTO_WSC    2   /* Witch Says Color */

#define NEIGHBORS_MAX    32

#define DEFAULT_WSC_GID  0

/** Application context */
struct app_ctx {
    addr_t      uid;        /** Node unique identifier */
    addr_t      gid;        /** Node group identifier */
    uint8_t     nneigh;     /** Number of neighbors */
    addr_t      neigh[NEIGHBORS_MAX];   /** Neighbors array */
    uint8_t     proto;      /** Current application protocol */
    tpl_ctx_t   tpl;        /** Transport protocol context */
    union {                     /** Application layer protocols */
        discover_ctx_t dis;     /** Discovery protocol context */
        spt_ctx_t      spt;     /** Spanning tree protocol context */
        wsc_ctx_t      wsc;     /** Witch says color context */
    };
};

typedef struct app_ctx app_ctx_t;

extern struct app_ctx *mydata;

#define myapp mydata

#endif /* APP_H_ */
