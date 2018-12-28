#ifndef SPT_H_
#define SPT_H_

#include "misc.h"
#include "tpl.h" /* addr_t */

#define SPT_STATE_IDLE      0
#define SPT_STATE_ACTIVE    1
#define SPT_STATE_TERM      2
#define SPT_STATE_DONE      3

#define CHILDS_MAX          32

struct spt_ctx {
    uint32_t    start;
    uint8_t     state;
    addr_t      root;   /* Current root */
    uint8_t     counter;
    uint8_t     checks;
    addr_t      parent;
    addr_t      childs[CHILDS_MAX];
    uint8_t     nchilds;
    uint8_t     notify_num;
    addr_t      notify_skp;
};

typedef struct spt_ctx spt_ctx_t;

void spt_init(void);

void spt_loop(void);


#endif /* SPT_H_ */
