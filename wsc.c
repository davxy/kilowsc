#include "wsc.h"
#include "app.h"


/* Temporary: Assume that the witch is the first COLOR to be nominated... */

#define mywsc (&mydata->wsc)

#define DIST_MAX    255
#define DIST_MIN    40

#define AGING_PERIOD_SECONDS    2
#define ECHO_PERIOD_SECONDS     1
#define ESCAPE_TIME_SECONDS     10

#define AGING_PERIOD_TICKS      (AGING_PERIOD_SECONDS * KILO_TICKS_PER_SEC)
#define ECHO_PERIOD_TICKS       (ECHO_PERIOD_SECONDS * KILO_TICKS_PER_SEC)
#define ESCAPE_TIME_TICKS       (ESCAPE_TIME_SECONDS * KILO_TICKS_PER_SEC)

#define AGING_QUANTITY          30

#define COLLISION_DIST          (DIST_MIN + 20)

/* Time to take a U turn */
#define UTURN_SECONDS           10
#define UTURN_TICKS             (UTURN_SECONDS * KILO_TICKS_PER_SEC)


#define MOVING_STOP     0
#define MOVING_FRONT    1
#define MOVING_LEFT     2
#define MOVING_RIGHT    3

#define MOVE_STOP() do { \
    mywsc->move = MOVING_STOP; \
    set_motors(0, 0); \
} while (0)

#define MOVE_FRONT() do { \
    mywsc->move = MOVING_FRONT; \
    set_motors(kilo_straight_left, kilo_straight_right); \
} while (0)

#define MOVE_LEFT() do { \
    mywsc->move = MOVING_LEFT; \
    set_motors(kilo_turn_left, 0); \
} while (0)

#define MOVE_RIGHT() do { \
    mywsc->move = MOVING_RIGHT; \
    set_motors(0, kilo_turn_right); \
} while (0)


struct wsc_pdu {
    uint8_t gid;    /* Group identifier */
    uint8_t mch;    /* Match counter */
    uint8_t tar;    /* Target id */
    uint8_t dis;    /* Distance */
};



static int wsc_send(uint8_t dst, struct wsc_pdu *pdu)
{
    uint8_t data[4];

    data[0] = pdu->gid;
    data[1] = pdu->mch;
    data[2] = pdu->tar;
    data[3] = pdu->dis;
    return tpl_send(dst, data, sizeof(data));
}

static int wsc_recv(uint8_t *src, struct wsc_pdu *pdu)
{
    int res;
    uint8_t data[4];
    uint8_t len = sizeof(data);

    if ((res = tpl_recv(src, data, &len)) == 0) {
        pdu->gid = data[0];
        pdu->mch = data[1];
        pdu->tar = data[2];
        pdu->dis = data[3];
    }
    return res;
}

static int update_send(addr_t dst)
{
    struct wsc_pdu pdu;

    pdu.gid = mydata->gid;
    pdu.mch = mywsc->match_cnt;
    pdu.tar = mywsc->target;
    pdu.dis = mywsc->dist;
    return wsc_send(dst, &pdu);
}

/* Returns 1 if imminent collision has been detected */
static int collision_avoid(uint8_t src, uint8_t match_cnt)
{
    int res = 0;

    if ((mydata->uid == mywsc->target || src == mywsc->target) &&
            match_cnt == mywsc->match_cnt)
        return 0;

    if (mydata->tpl.dist < COLLISION_DIST ) {
        TRACE_APP("AVOID COLLISION!!!\n");
        if (mywsc->move == MOVING_STOP) {
            if (mydata->uid < src)
                MOVE_LEFT();
            //else
            //    MOVE_LEFT();
        } else {
            MOVE_STOP();
        }
        res = 1;
    }
    return res;
}

static void update_hunter(uint8_t src, uint8_t dist, uint8_t force)
{
    uint8_t prev_dist = mywsc->dist;

    /* Eventually update distance */
    if (force == 1 || dist < prev_dist) {
        mywsc->dist = dist;
        mywsc->dist_src = src;
        mywsc->aging_tick = kilo_ticks + AGING_PERIOD_TICKS;
    }

    if ((dist > prev_dist && dist - prev_dist > 10) ||
            dist == DIST_MAX || src == kilo_uid) {
        /* Distance has increased */
        if ((mywsc->flags & WSC_FLAG_APPROACH) != 0) {
            /* Was approaching */
            if (src == mywsc->dist_src) {
                mywsc->move_tick = kilo_ticks + UTURN_TICKS;
                MOVE_LEFT();
                TRACE_APP("U TURN\n");
            } else {
                TRACE_APP("IGNORE (src=%u, appr=%u)\n", src, mywsc->dist_src);
            }
        } else if (mywsc->move_tick < kilo_ticks) {
            if (mywsc->move == MOVING_FRONT) {
                MOVE_LEFT();
                mywsc->move_tick = kilo_ticks + 2 * KILO_TICKS_PER_SEC;
            } else {
                MOVE_FRONT();
                mywsc->move_tick = kilo_ticks + 6 * KILO_TICKS_PER_SEC;
            }
            TRACE_APP("HUNTING\n");
        }
        mywsc->flags &= ~WSC_FLAG_APPROACH;
    } else if (dist < prev_dist) {
        MOVE_FRONT();
        mywsc->flags |= WSC_FLAG_APPROACH;
        TRACE_APP("APPROACHING to %u\n", src);
    }

}

static void blink(void)
{
    if ((mywsc->flags & WSC_FLAG_COLOR_ON) != 0) {
        COLOR_APP(WHITE);
        mywsc->flags &= ~WSC_FLAG_COLOR_ON;
    } else {
        COLOR_APP(mydata->uid);
        mywsc->flags |= WSC_FLAG_COLOR_ON;
    }
}

static void active_hunter(void)
{
    uint8_t dist;

    /* Distance information aging */
    if (mywsc->aging_tick > kilo_ticks)
        return;

    mywsc->aging_tick = kilo_ticks + AGING_PERIOD_TICKS;
    dist = ((uint16_t)mywsc->dist + AGING_QUANTITY < DIST_MAX) ?
            mywsc->dist + AGING_QUANTITY : DIST_MAX;
    update_hunter(mywsc->dist_src, dist, 1);
}

static void active_target(void)
{
    uint8_t dir;

    if (mywsc->move_tick > kilo_ticks)
        return;
    mywsc->move_tick = kilo_ticks + (1 + rand() % 10) *TICKS_PER_SEC;

    dir = rand() % 4;
    switch (dir) {
    case 0:
        MOVE_FRONT();
        break;
    case 1:
        MOVE_LEFT();
        break;
    case 2:
        MOVE_RIGHT();
        break;
    case 3:
    default:
        MOVE_STOP();
        break;
    }
}

static void spontaneous(void)
{
    mywsc->target = TPL_BROADCAST_ADDR;
    mywsc->state = WSC_STATE_ACTIVE;
    mywsc->dist = DIST_MAX;
    mywsc->dist_src = mydata->uid;
    ASSERT(mydata->nneigh != 0);
    TRACE_APP("TARGET SEARCH to %u\n", mydata->neigh[0]);
    update_send(mydata->neigh[0]);
}

static void wsc_match(addr_t target)
{
    mywsc->match_cnt++;
    mywsc->target = target;
    mywsc->dist = mydata->tpl.dist;
    mywsc->dist_src = target;
    /* Give him some time to escape */
    //mywsc->echo_tick = kilo_ticks + ESCAPE_TIME_TICKS;
    //mywsc->move_tick = kilo_ticks + ESCAPE_TIME_TICKS;
    MOVE_STOP();
    COLOR_APP(mydata->uid);
    update_send(TPL_BROADCAST_ADDR);
}

void wsc_loop(void)
{
    uint8_t dist, src;
    //uint8_t prev_dist, new_match = 0;
    struct wsc_pdu pdu;

    //prev_dist = mywsc->dist;

    /* spontaneous event for the group leader */
    if (mywsc->state == WSC_STATE_IDLE && mydata->uid == mydata->gid) {
        spontaneous();
        return;
    }

    /* Fetch a message (target silently ignores messages) */
    if (wsc_recv(&src, &pdu) == 0) {

        /* Safety first */
        if (mywsc->state != WSC_STATE_IDLE &&
                collision_avoid(src, pdu.mch) != 0)
            return; /* Imminent collision... Nothing to do */

        /* Ignore messages from other groups */
        if (pdu.gid != mydata->gid) {
            TRACE_APP("IGNORE other group\n");
            return;
        }

        if (pdu.tar == TPL_BROADCAST_ADDR && mywsc->target == TPL_BROADCAST_ADDR) {
            if (mydata->nneigh != 1) {
                TRACE_APP("TARGET SEARCH from %u\n", src);
                spontaneous();
            } else {
                /* Leaf */
                TRACE_APP("TARGET\n");
                mywsc->target = mydata->uid;
                mywsc->dist = 0;
                mywsc->dist_src = mydata->uid;
                mywsc->state = WSC_STATE_ACTIVE;
                mywsc->match_cnt++;
                update_send(TPL_BROADCAST_ADDR);
            }
            return;
        }

        if (mydata->uid == mywsc->target && mydata->tpl.dist <= DIST_MIN) {
            TRACE_APP(">>> CATCHED <<<\n");
            wsc_match(src);
            return;
        }

        /* Eventually refresh the distance aging timer */
        if (src == mywsc->dist_src)
            mywsc->aging_tick = kilo_ticks + 5 * KILO_TICKS_PER_SEC;

        /*
         * Distance from target is computed as the one transported within
         * the message plus the distance detected at tplnel level.
         */
        dist = ((uint16_t)pdu.dis + mydata->tpl.dist < DIST_MAX) ?
                pdu.dis + mydata->tpl.dist : DIST_MAX;

        if (mywsc->state != WSC_STATE_IDLE) {
            /* Check if a new match is started */
            if (pdu.mch > mywsc->match_cnt) {
                COLOR_APP(mydata->uid);
                if (pdu.tar != mydata->uid) {
                    mywsc->dist = dist;
                    mywsc->dist_src = src;
                } else {
                    mywsc->dist = 0;
                    mywsc->dist_src = mydata->uid;
                    mywsc->echo_tick = kilo_ticks + ECHO_PERIOD_TICKS * 5;
                }
                mywsc->match_cnt = pdu.mch;
                mywsc->target = pdu.tar;
                mywsc->flags &= ~WSC_FLAG_APPROACH;
                //new_match = 1;
            } else if (mydata->uid != mywsc->target) {
                update_hunter(src, dist, 0);
            }
        } else {
            mywsc->state = WSC_STATE_ACTIVE;
            mywsc->match_cnt = pdu.mch;
            mywsc->target = pdu.tar;
            if (pdu.tar != mydata->uid) {
                mywsc->dist = dist;
                mywsc->dist_src = src;
            } else {
                mywsc->dist = 0;
                mywsc->dist_src = mydata->uid;
            }
        }
    }

    /* Active loop */
    if (mywsc->state == WSC_STATE_ACTIVE && mywsc->target != TPL_BROADCAST_ADDR) {
        if (mywsc->target != mydata->uid)
            active_hunter();
        else
            active_target();
        if (mywsc->echo_tick < kilo_ticks) {
            mywsc->echo_tick = kilo_ticks + ECHO_PERIOD_TICKS;
            if (mywsc->target == mydata->uid)
                blink();
#if 0
            if ((mywsc->dist != DIST_MAX && mywsc->dist <= prev_dist) ||
                    mywsc->target == mydata->uid || new_match == 1) {
                update_send(TPL_BROADCAST_ADDR);
            } else {
                struct wsc_pdu pdu;

                pdu.gid = TPL_BROADCAST_ADDR;
                pdu.mch = mywsc->match_cnt;
                pdu.tar = mywsc->target;
                pdu.dis = mywsc->dist;
                wsc_send(TPL_BROADCAST_ADDR, &pdu);
            }
#else
            update_send(TPL_BROADCAST_ADDR);
#endif
            TRACE_APP("STAT: gid=%u, tar=%u, mch=%u, dis=: %u\n",
                    mydata->gid, mywsc->target, mywsc->match_cnt, mywsc->dist);
        }
    }
}

void wsc_init(void)
{
    memset(mywsc, 0, sizeof(*mywsc));
    mywsc->dist =  DIST_MAX;
    mywsc->dist_src = mydata->uid;
    mywsc->target = TPL_BROADCAST_ADDR;
    mydata->tpl.timeout_cb = NULL;
    COLOR_APP(mydata->uid);
}
