#ifndef APP_H_
#define APP_H_

#include "dis.h"
#include "spt.h"
#include "wsc.h"
#include "misc.h"
#include "tpl.h"

/**
 * Application protocols. @{
 */
/** Neighbor discovery */
#define APP_PROTO_DIS       0
/** Spanning tree construction */
#define APP_PROTO_SPT       1
/** Witch Says Color */
#define APP_PROTO_WSC       2
/** @} */

/** Max number of allowed node neighbors. */
#define APP_NEIGHBORS_MAX   32

/** Default group id. Used when the SPT construction is skipped */
#define APP_DEFAULT_GID     0

/** Application context */
struct app_ctx {
    addr_t      uid;        /** Node unique identifier */
    addr_t      gid;        /** Node group identifier */
    uint8_t     nneigh;     /** Number of neighbors */
    addr_t      neigh[APP_NEIGHBORS_MAX]; /** Neighbors array */
    uint8_t     proto;      /** Current application protocol */
    tpl_ctx_t   tpl;        /** Transport protocol context */
    union {                 /** Application layer protocols */
        dis_ctx_t dis;      /** Discovery protocol context */
        spt_ctx_t spt;      /** Spanning tree protocol context */
        wsc_ctx_t wsc;      /** Witch says color context */
    };
};

/** Application context type alias. */
typedef struct app_ctx app_ctx_t;
/** Application context instance pointer. */
extern struct app_ctx *mydata;
/** Application context instance pointer alias. */
#define myapp mydata

#endif /* APP_H_ */
