#include "spt.h"
#include "app.h"

#define myspt (&mydata->spt)

/*
 * The entity starts in IDLE state and will (eventually) switch to ACTIVE
 * after a random number of seconds.
 */
#define SPT_RAND_START_TICKS \
        (3 * KILO_TICKS_PER_SEC + (rand() % (15 * KILO_TICKS_PER_SEC)))

/* PDU types */
#define PDU_TYPE_ASK        1
#define PDU_TYPE_YES        2
#define PDU_TYPE_CHK        3
#define PDU_TYPE_FIN_REQ    4
#define PDU_TYPE_FIN_RES    5

struct spt_pdu {
    uint8_t  type;
    addr_t   root;
};

static int pdu_send(struct spt_pdu *pdu, addr_t dst)
{
    uint8_t data[2];
    uint8_t size = 1;

    data[0] = pdu->type;
    if (pdu->type != PDU_TYPE_FIN_REQ && pdu->type != PDU_TYPE_FIN_RES) {
        data[1] = pdu->root;
        size++;
    }
    return app_send(dst, data, size);
}

static int pdu_recv(struct spt_pdu *pdu, addr_t *src)
{
    uint8_t data[2];
    uint8_t siz = 2;
    int res = 0;

    if ((res = app_recv(src, data, &siz)) < 0)
        return res;

    pdu->type = data[0];
    switch (pdu->type) {
    case PDU_TYPE_ASK:
    case PDU_TYPE_YES:
    case PDU_TYPE_CHK:
        if (siz != 2)
            res = -1;
        pdu->root = data[1];
        break;
    case PDU_TYPE_FIN_REQ:
    case PDU_TYPE_FIN_RES:
        if (siz != 1)
            res = -1;
        pdu->root = TPL_BROADCAST_ADDR; /* Not used */
        break;
    default:
        res = -1;
        break;
    }
    return res;
}

static void child_add(addr_t id)
{
    uint8_t i;

    for (i = 0; i < myspt->nchild; i++) {
        if (myspt->child[i] == id)
            break;
    }
    ASSERT(i == myspt->nchild); /* Should never happen */
    if (i == myspt->nchild) {
        myspt->child[i] = id;
        myspt->nchild++;
    }
}

static void term_res(void)
{
    struct spt_pdu pdu;

    TRACE_APP("TX TERM-RES <dst=%u>\n", myspt->parent);
    pdu.type = PDU_TYPE_FIN_RES;
    pdu_send(&pdu, myspt->parent);
}

static void try_term(void)
{
    struct spt_pdu pdu;

    if (myspt->root == kilo_uid) {
        TRACE_APP("ROOT FOUND");
        TRACE_APP("TERM to %u children\n", myspt->nchild);
        COLOR_APP(BLUE);
        ASSERT(myspt->notify_num == 0);
        myspt->notify_num = myspt->nchild;
        myspt->notify_skp = TPL_BROADCAST_ADDR; /* nothing to skip */
        myspt->state = SPT_STATE_TERM;
        myspt->checks = 0; /* received child term-res */
    } else {
        pdu.root = myspt->root;
        pdu.type = PDU_TYPE_CHK;
        /* Check departs from leafs */
        ASSERT(myspt->parent != kilo_uid);
        TRACE_APP("TX CHECK <dst=%u ,root=%u>\n", myspt->parent, myspt->root);
        if (pdu_send(&pdu, myspt->parent) < 0)
            ASSERT(0);
        COLOR_APP(RGB(0,1,0));
    }
}

static void check(void)
{
    struct spt_pdu pdu;

    COLOR_APP(RGB(1,0,0));
    if (myspt->checks == myspt->nchild) {
        pdu.root = myspt->root;
        pdu.type = PDU_TYPE_CHK;
        /* Check departs from leafs */
        TRACE_APP("TX CHECK <dst=%u ,root=%u>\n", myspt->parent, myspt->root);
        if (pdu_send(&pdu, myspt->parent) < 0) {
            ASSERT(0);
        }
        COLOR_APP(RGB(0,1,0));
    }
}

static void construct(addr_t src, addr_t root)
{
    struct spt_pdu pdu;

    myspt->root = root;
    myspt->parent = src;    /* sender */
    ASSERT(myspt->notify_num == 0);
    myspt->notify_num = 0;  /* reset */
    myspt->checks = 0;
    myspt->nchild = 0;

    pdu.type = PDU_TYPE_YES;
    pdu.root = root;
    TRACE_APP("TX Y <dst=%u ,root=%u>\n", src, myspt->root);
    if (pdu_send(&pdu, src) < 0) {
        TRACE_APP("Error: pdu send\n");
        ASSERT(0);
        return;
    }

    myspt->counter = 1;
    if (myspt->counter == mydata->nneigh) {
        check();
    } else {
        ASSERT(myspt->notify_num == 0);
        myspt->notify_num = mydata->nneigh;
        myspt->notify_skp = src;
        COLOR_APP(RED);
    }
}

static void active_state(struct spt_pdu *pdu, addr_t src)
{
    switch (pdu->type) {
    case PDU_TYPE_ASK:
        TRACE_APP("RX Q <src=%u ,root=%u>\n", src, pdu->root);
        if (myspt->root == pdu->root) {
            myspt->counter++;
            if (myspt->counter == mydata->nneigh)
                check();
        } else if (myspt->root > pdu->root) {
            construct(src, pdu->root);
        }
        break;
    case PDU_TYPE_YES:
        TRACE_APP("RX Y <src=%u ,root=%u>\n", src, pdu->root);
        if (myspt->root == pdu->root) {
            child_add(src);
            myspt->counter++;
            if (myspt->counter == mydata->nneigh)
                check();
        }
        break;
    case PDU_TYPE_CHK:
        TRACE_APP("RX CHECK <src=%u ,root=%u>\n", src, pdu->root);
        if (myspt->root == pdu->root) {
            myspt->checks++;
            if (myspt->counter == mydata->nneigh &&
                    myspt->checks == myspt->nchild)
                try_term();
        }
        break;
    case PDU_TYPE_FIN_REQ:
        TRACE_APP("RX TERM-REQ <src=%u>\n", src);
        ASSERT(myspt->notify_num == 0);
        if (myspt->nchild > 0) {
            COLOR_APP(BLUE);
            TRACE_APP("TERM to %u children\n", myspt->nchild);
            myspt->notify_num = myspt->nchild;
            myspt->notify_skp = TPL_BROADCAST_ADDR; /* nothing to skip */
            myspt->state = SPT_STATE_TERM;
            myspt->checks = 0; /* Received child term-res */
        } else {
            TRACE_APP("SPT DONE\n");
            myspt->state = SPT_STATE_DONE;
            COLOR_APP(GREEN);
            term_res();
        }
        break;
    default:
        TRACE("INVALID PDU (state=%u, pdu=%u)\n", myspt->state, pdu->type);
        break;
    }
}

static void idle_state(struct spt_pdu *pdu, addr_t src)
{
    if (pdu->type == PDU_TYPE_ASK) {
        TRACE_APP("RX Q <src=%u ,root=%u>\n", src, pdu->root);
        myspt->state = SPT_STATE_ACTIVE;
        construct(src, pdu->root);
    } else {
        TRACE("INVALID PDU (state=%u, pdu=%u)\n", myspt->state, pdu->type);
    }
}

static void term_state(struct spt_pdu *pdu, addr_t src)
{
    if (pdu->type == PDU_TYPE_FIN_RES) {
        TRACE_APP("RX TERM-RES <src=%u>\n", src);
        myspt->checks++;
        if (myspt->checks == myspt->nchild) {
            TRACE_APP("DONE\n");
            myspt->state = SPT_STATE_DONE;
            COLOR_APP(GREEN);
            if (mydata->uid != myspt->root)
                term_res();
        }
    } else {
        TRACE("INVALID PDU (state=%u, pdu=%u)\n", myspt->state, pdu->type);
    }
}


static void start_protocol(void)
{
    COLOR_APP(RED);
    myspt->state = SPT_STATE_ACTIVE;
    myspt->root = kilo_uid;
    myspt->notify_num = mydata->nneigh;
    myspt->notify_skp = TPL_BROADCAST_ADDR; /* nothing to skip */
}

static int is_neighbor(addr_t addr)
{
    uint8_t i;

    for (i = 0; i < mydata->nneigh; i++) {
        if (mydata->neigh[i] == addr)
            break;
    }
    return (i < mydata->nneigh);
}

static void timeout(addr_t dst, uint8_t *data, uint8_t siz)
{
    // TODO: remove from neighbor list
    TRACE_APP("TIMEOUT waiting ACK from %u\n", dst);
    ASSERT(0);
}

static void send_pending(void)
{
    struct spt_pdu pdu;
    addr_t dst;

    if (myspt->start > kilo_ticks)
        return;
next:
    if (myspt->notify_num == 0)
        return;
    myspt->notify_num--;
    dst = (myspt->state == SPT_STATE_TERM) ?
        myspt->child[myspt->notify_num] :
        mydata->neigh[myspt->notify_num];
    if (dst == myspt->notify_skp)
        goto next;

    if (myspt->state == SPT_STATE_TERM) {
        TRACE_APP("TX TERM-REQ <dst=%u>\n", dst);
        pdu.type = PDU_TYPE_FIN_REQ;
    } else {
        TRACE_APP("TX ASK <dst=%u ,root=%u>\n", dst, myspt->root);
        pdu.type = PDU_TYPE_ASK;
        pdu.root = myspt->root;
    }
    if (pdu_send(&pdu, dst) < 0) {
        myspt->notify_num++;
        myspt->start = kilo_ticks + 5 * KILO_TICKS_PER_SEC;
        TRACE_APP("Q/TERM send fail\n");
    }
}


void spt_loop(void)
{
    struct spt_pdu pdu;
    addr_t src;

    /* Spontaneous event */
    if (myspt->state == SPT_STATE_IDLE &&
            kilo_ticks > myspt->start) {
        start_protocol();
    }

    if (myspt->notify_num > 0) {
        send_pending();
        return;
    }

    if (pdu_recv(&pdu, &src) < 0)
        return; /* Nothing to do */

    if (!is_neighbor(src)) {
        TRACE_APP("Warning: SPT message from not neighbor node %u\n", src);
        /*
         * Hack: send back an ASK message with the same root.
         * The message let the source think that we are already part of the
         * tree but with another parent.
         */
        TRACE_APP("TX ASK <dst=%u ,root=%u>\n", src, pdu.root);
        pdu.type = PDU_TYPE_ASK;
        pdu_send(&pdu, src);
        return;
    }

    switch (myspt->state) {
    case SPT_STATE_IDLE:
        idle_state(&pdu, src);
        break;
    case SPT_STATE_ACTIVE:
        active_state(&pdu, src);
        break;
    case SPT_STATE_TERM:
        term_state(&pdu, src);
        break;
    case SPT_STATE_DONE:
        break;
    default:
        break;
    }
}

void spt_init(void)
{
    memset(myspt, 0, sizeof(*myspt));
    COLOR_APP(WHITE);
    myspt->state = SPT_STATE_IDLE;
    myspt->parent = kilo_uid;
    mydata->tpl.timeout_cb = timeout;
    myspt->start = kilo_ticks + SPT_RAND_START_TICKS;
}
