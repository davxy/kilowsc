#include "spt.h"
#include "app.h"

/*
 * Every initiator start the construction of its own uniquely identified
 * spanning-tree, but then suppressing some of these constructions, allowing
 * only one to complete. In this approach, an entity faced with two different
 * spt-constructions, will select and act on only one, “killing” the other;
 * the entity continues this selection process as long as it receives
 * conflicting requests.
 *
 * The criterion commonly used is based on min-id: since each spt-construction
 * has a unique id (that of its initiator), when faced with different
 * spt-constructions an entity will choose the one with the smallest id,
 * and terminate all the others.
 *
 * An entity U, at any time, participates in the construction of just one
 * spanning tree rooted in some initiator X. It will ignore all messages
 * referring to the construction of other spanning trees where the initiators
 * have larger ids than X. If instead U receives a message referring to the
 * construction of a spanning-tree rooted in an initiator Y with an id smaller
 * than X’s, then U will stop working for X and start working for Y.
 *
 * It is possible that an entity has already terminated its part of the
 * construction of a spanning tree when it receives a message from another
 * initiator (possibly, with a smaller id). In other words, when an entity
 * has terminated a construction, it does not know whether it might have
 * to restart again. Thus, it is necessary to include in the protocol a
 * mechanism that ensures an effective local termination for each entity.
 *
 * Local termination can be achieved by ensuring that we use, as a building
 * block, a unique-initiator spt-protocol in which the initiator will know
 * when its spanning tree has been completely constructed.
 *
 * NOTE: Q messages should be periodic... until we've not receive:
 * - Y (with our current myspt->root)
 * - Q (with a root value <= myspt->root)
 * From every neighbor.
 *
 * NOTE: ignore messages not from neighbors
 */

#define myspt (&mydata->spt)


#define PDU_TYPE_Q          1
#define PDU_TYPE_YES        2
#define PDU_TYPE_CHECK      3
#define PDU_TYPE_TERM_REQ   4
#define PDU_TYPE_TERM_RES   5

#define APDU_SIZE  2

struct spt_pdu {
    uint8_t  type;
    addr_t   root;
};

static int pdu_send(struct spt_pdu *pdu, addr_t dst)
{
    uint8_t data[APDU_SIZE];
    uint8_t size = 1;

    data[0] = pdu->type;
    if (pdu->type != PDU_TYPE_TERM_REQ && pdu->type != PDU_TYPE_TERM_RES) {
        data[1] = pdu->root;
        size++;
    }
    return chan_send(dst, data, size);
}

static int pdu_recv(struct spt_pdu *pdu, addr_t *src)
{
    uint8_t data[APDU_SIZE];
    uint8_t siz = APDU_SIZE;
    int res = 0;

    if ((res = chan_recv(src, data, &siz)) < 0)
        return res;

    pdu->type = data[0];
    switch (pdu->type) {
    case PDU_TYPE_Q:
    case PDU_TYPE_YES:
    case PDU_TYPE_CHECK:
        if (siz != 2)
            res = -1;
        pdu->root = data[1];
        break;
    case PDU_TYPE_TERM_REQ:
    case PDU_TYPE_TERM_RES:
        if (siz != 1)
            res = -1;
        pdu->root = BROADCAST_ADDR; /* Not used */
        break;
    default:
        res = -1;
        break;
    }
    return res;
}

/*
 * Move the system from an initial configuration where each entity x is just
 * aware of its own neighbors N(x) to a configuration where:
 * - each entity x has selected a subnet of N(x)
 * - the collection of all the corresponding links form a spanning tree.
 *
 * The entity starts in IDLE state and will (eventually) switch to ACTIVE
 * after a random number of seconds.
 */

#define SPT_RAND_INIT_OFF \
        (3 * KILO_TICKS_PER_SEC + (rand() % (15 * KILO_TICKS_PER_SEC)))

static void child_add(addr_t id)
{
    int i;

    for (i = 0; i < myspt->nchilds; i++) {
        if (myspt->childs[i] == id)
            break;
    }
    ASSERT(i == myspt->nchilds); /* Should never happen */
    if (i == myspt->nchilds) {
        myspt->childs[i] = id;
        myspt->nchilds++;
    }
}

static void term_res(void)
{
    struct spt_pdu pdu;

    TRACE_APP("TX TERM-RES <dst=%u>\n", myspt->parent);
    pdu.type = PDU_TYPE_TERM_RES;
    pdu_send(&pdu, myspt->parent);
}

static void try_term(void)
{
    struct spt_pdu pdu;

    if (myspt->root == kilo_uid) {
        TRACE_APP("ROOT FOUND send TERM to (%u) childs\n", myspt->nchilds);
        COLOR_APP(BLUE);
        ASSERT(myspt->notify_num == 0);
        myspt->notify_num = myspt->nchilds;
        myspt->notify_skp = BROADCAST_ADDR; /* nothing to skip */
        myspt->state = SPT_STATE_TERM;
        myspt->checks = 0; /* use checks to count received childs res */
    } else {
        pdu.root = myspt->root;
        pdu.type = PDU_TYPE_CHECK;
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
    if (myspt->checks == myspt->nchilds) {
        pdu.root = myspt->root;
        pdu.type = PDU_TYPE_CHECK;
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
    myspt->nchilds = 0;

    pdu.type = PDU_TYPE_YES;
    pdu.root = root;
    TRACE_APP("TX Y <dst=%u ,root=%u>\n", src, myspt->root);
    if (pdu_send(&pdu, src) < 0) {
        TRACE_APP("Error: pdu send\n");
        ASSERT(0);
        return;
    }

    myspt->counter = 1;
    if (myspt->counter == mydata->nneighbors) {
        check();
    } else {
        ASSERT(myspt->notify_num == 0);
        myspt->notify_num = mydata->nneighbors;
        myspt->notify_skp = src;
        COLOR_APP(RED);
    }
}

static void active(struct spt_pdu *pdu, addr_t src)
{
    switch (pdu->type) {
    case PDU_TYPE_Q:
        TRACE_APP("RX Q <src=%u ,root=%u>\n", src, pdu->root);
        if (myspt->root == pdu->root) {
            myspt->counter++;
            if (myspt->counter == mydata->nneighbors)
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
            if (myspt->counter == mydata->nneighbors)
                check();
        }
        break;
    case PDU_TYPE_CHECK:
        TRACE_APP("RX CHECK <src=%u ,root=%u>\n", src, pdu->root);
        if (myspt->root == pdu->root) {
            myspt->checks++;
            if (myspt->counter == mydata->nneighbors &&
                    myspt->checks == myspt->nchilds)
                try_term();
        }
        break;
    case PDU_TYPE_TERM_REQ:
        TRACE_APP("RX TERM-REQ <src=%u>\n", src);
        ASSERT(myspt->notify_num == 0);
        if (myspt->nchilds > 0) {
            COLOR_APP(BLUE);
            TRACE_APP("TERM to (%u) childs\n", myspt->nchilds);
            myspt->notify_num = myspt->nchilds;
            myspt->notify_skp = BROADCAST_ADDR; /* nothing to skip */
            myspt->state = SPT_STATE_TERM;
            myspt->checks = 0; /* use checks to count received childs res */
        } else {
            TRACE_APP("DONE\n");
            myspt->state = SPT_STATE_DONE;
            COLOR_APP(GREEN);
            term_res();
        }
        break;
    default:
        break;
    }
}

static void idle(struct spt_pdu *pdu, addr_t src)
{
    if (pdu->type == PDU_TYPE_Q) {
        TRACE_APP("RX Q <src=%u ,root=%u>\n", src, pdu->root);
        myspt->state = SPT_STATE_ACTIVE;
        construct(src, pdu->root);
    }
}


static void start_protocol(void)
{
    COLOR_APP(RED);
    myspt->state = SPT_STATE_ACTIVE;
    myspt->root = kilo_uid;
    myspt->notify_num = mydata->nneighbors;
    myspt->notify_skp = BROADCAST_ADDR; /* nothing to skip */
}

static int is_neighbor(addr_t addr)
{
    int i;

    for (i = 0; i < mydata->nneighbors; i++) {
        if (mydata->neighbors[i] == addr)
            break;
    }
    return (i < mydata->nneighbors);
}

static void timeout(addr_t dst, uint8_t *data, uint8_t siz)
{
    TRACE_APP("TIMEOUT sending to %u\n", dst);
    // TODO: remove from neighbor list
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
        myspt->childs[myspt->notify_num] :
        mydata->neighbors[myspt->notify_num];
    if (dst == myspt->notify_skp)
        goto next;

    if (myspt->state == SPT_STATE_TERM) {
        TRACE_APP("TX TERM-REQ <dst=%u>\n", dst);
        pdu.type = PDU_TYPE_TERM_REQ;
    } else {
        TRACE_APP("TX Q <dst=%u ,root=%u>\n", dst, myspt->root);
        pdu.type = PDU_TYPE_Q;
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
    ASSERT(is_neighbor(src));

    switch (myspt->state) {
    case SPT_STATE_IDLE:
        idle(&pdu, src);
        break;
    case SPT_STATE_ACTIVE:
        active(&pdu, src);
        break;
    case SPT_STATE_TERM:
        if (pdu.type == PDU_TYPE_TERM_RES) {
            TRACE_APP("RX TERM-RES <src=%u>\n", src);
            myspt->checks++;
            if (myspt->checks == myspt->nchilds) {
                TRACE_APP("DONE\n");
                myspt->state = SPT_STATE_DONE;
                COLOR_APP(GREEN);
                if (mydata->uid != myspt->root)
                    term_res();
            }
        }
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
    mydata->chan.timeout_cb = timeout;
    myspt->start = kilo_ticks + SPT_RAND_INIT_OFF; /* random start offset */
}
