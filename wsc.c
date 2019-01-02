#include "wsc.h"
#include "app.h"


/* Temporary: Assume that the witch is the first COLOR to be nominated... */

#define mywsc (&mydata->wsc)

#define DIST_MAX    255
#define DIST_MIN    40

#define AGING_PERIOD_SECONDS    2
#define ECHO_PERIOD_SECONDS     1
#define SLEEP_TIME_SECONDS      15

#define AGING_PERIOD_TICKS      (AGING_PERIOD_SECONDS * KILO_TICKS_PER_SEC)
#define ECHO_PERIOD_TICKS       (ECHO_PERIOD_SECONDS * KILO_TICKS_PER_SEC)
#define SLEEP_TIME_TICKS        (SLEEP_TIME_SECONDS * KILO_TICKS_PER_SEC)
#define BLINK_TIME_TICKS        15

#define AGING_QUANTITY          30

#define COLLISION_DIST          (DIST_MIN + 10)

/* Time to take a U turn */
#define UTURN_SECONDS           10
#define UTURN_TICKS             (UTURN_SECONDS * KILO_TICKS_PER_SEC)

#define RADIUS_MIN              3
#define RADIUS_MAX              20
#define RADIUS_STEP             1


/* Current movement */
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
    return app_send(dst, data, sizeof(data));
}

static int wsc_recv(uint8_t *src, struct wsc_pdu *pdu)
{
    int res;
    uint8_t data[4];
    uint8_t len = sizeof(data);

    if ((res = app_recv(src, data, &len)) == 0) {
        pdu->gid = data[0];
        pdu->mch = data[1];
        pdu->tar = data[2];
        pdu->dis = data[3];
    }
    return res;
}

/* Send an update message */
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

    /*
     * If the source is the target and we are playing the right match then
     * don't try to avoid collisions
     */
    if (src == mywsc->target && match_cnt == mywsc->match_cnt)
        return 0;

    if (mydata->tpl.dist < COLLISION_DIST ) {
        TRACE_APP("AVOID COLLISION!!!\n");
        if (mywsc->min_dist_src == src &&
            mydata->tpl.dist < mywsc->min_dist) {
            if (mywsc->move == MOVING_STOP) {
                MOVE_LEFT();
                mywsc->move_tick = kilo_uid + UTURN_TICKS;
            } else {
                MOVE_STOP();
            }
        } else {
            TRACE("NEW\n");
            mywsc->min_dist_src = src;
            if (mydata->uid < src)
                MOVE_LEFT();
            else
                MOVE_STOP();
        }
        mywsc->min_dist = mydata->tpl.dist;
        res = (mydata->uid != mywsc->target) ? 1: 0;
    }
    return res;
}

static void update_hunter(uint8_t src, uint8_t dist, uint8_t force)
{
    uint8_t prev_dist = mywsc->dist;
    uint8_t approach;

    /* Eventually update distance */
    if (force == 1 || dist < prev_dist) {
        mywsc->dist = dist;
        mywsc->dist_src = src;
        mywsc->aging_tick = kilo_ticks + AGING_PERIOD_TICKS;
    }

    approach = dist <= prev_dist;
    if (approach == 0 || dist == DIST_MAX || src == kilo_uid) {
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
            mywsc->flags &= ~WSC_FLAG_APPROACH;
            mywsc->radius = RADIUS_MIN;
        } else if (mywsc->move_tick < kilo_ticks) {
            if (mywsc->move == MOVING_FRONT) {
                MOVE_LEFT();
                mywsc->move_tick = kilo_ticks + 5 * KILO_TICKS_PER_SEC;
            } else {
                MOVE_FRONT();
                mywsc->move_tick = kilo_ticks + 10 * KILO_TICKS_PER_SEC;
                mywsc->radius = ((uint16_t)mywsc->radius + RADIUS_STEP < RADIUS_MAX) ?
                                mywsc->radius + RADIUS_STEP : RADIUS_MAX;
            }
            TRACE_APP("HUNTING\n");
            mywsc->flags &= ~WSC_FLAG_APPROACH;
        }
    } else if (approach == 1) {
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


static void new_match(uint8_t target)
{
    mywsc->match_cnt++;
    if (mywsc->match_cnt < WSC_MATCH_MAX) {
        mywsc->target = target;
        mywsc->dist = mydata->tpl.dist;
        mywsc->dist_src = target;
        mywsc->state = WSC_STATE_SLEEP;
        /* Give him some time to escape */
        mywsc->move_tick = kilo_ticks + SLEEP_TIME_TICKS;
        COLOR_APP(mydata->uid);
        TRACE_APP("SLEEP...\n");
    } else {
        mywsc->state = WSC_STATE_DONE;
        mywsc->match_cnt = WSC_MATCH_MAX;
    }
    MOVE_STOP();
    update_send(TPL_BROADCAST_ADDR);
}

/* Execuited to elect the target at the beginning of the first match */
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

static void target_search(uint8_t src)
{
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



void wsc_loop(void)
{
    uint8_t dist, src;
    struct wsc_pdu pdu;

    /* Spontaneous event from the group leader. */
    if (mywsc->state == WSC_STATE_IDLE && mydata->uid == mydata->gid) {
        spontaneous();
        return;
    }

    if (mywsc->state == WSC_STATE_DONE) {
        if (mywsc->echo_tick <= kilo_ticks) {
            mywsc->echo_tick = kilo_ticks + 3*ECHO_PERIOD_TICKS;
            update_send(TPL_BROADCAST_ADDR);
        }
        return;
    }

    if (mywsc->target == mydata->uid && mywsc->blink_tick < kilo_ticks) {
        mywsc->blink_tick = mywsc->blink_tick + BLINK_TIME_TICKS;
        blink();
    }

    /* Fetch a message (target silently ignores messages) */
    if (wsc_recv(&src, &pdu) == 0) {
        /* The captured target sleeps a bit before start the new match */
        if (mywsc->state == WSC_STATE_SLEEP) {
            if (mywsc->echo_tick <= kilo_ticks) {
                mywsc->echo_tick = kilo_ticks + ECHO_PERIOD_TICKS;
                update_send(TPL_BROADCAST_ADDR);
            }
            if (mywsc->move_tick <= kilo_ticks) {
                TRACE_APP("WAKEUP!!!\n");
                mywsc->state = WSC_STATE_ACTIVE;
            } else {
                return;
            }
        }

        /* Avoid collisions */
        if (mywsc->state != WSC_STATE_IDLE &&
                collision_avoid(src, pdu.mch) != 0)
            return; /* Imminent collision... Nothing to do */

        /* Ignore messages from other groups */
        if (pdu.gid != mydata->gid) {
            TRACE_APP("IGNORE other group\n");
            return;
        }

        /*
         * Executed once at the beginning of the first match to elect the
         * target as the leftmost tree child.
         */
        if (pdu.tar == TPL_BROADCAST_ADDR && mywsc->target == TPL_BROADCAST_ADDR) {
            target_search(src);
            return;
        }

        /*
         * Target is captured if the distance from the last message source
         * is less than a given threshold.
         */
        if (mydata->uid == mywsc->target && mydata->tpl.dist <= DIST_MIN
            && pdu.mch == mywsc->match_cnt) {
            TRACE_APP(">>> TARGET CAPTURED <<<\n");
            new_match(src); /* Start a new match */
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
            if (pdu.mch == mywsc->match_cnt) {
                update_hunter(src, dist, 0);
            } else if (pdu.mch > mywsc->match_cnt) {
                if (pdu.tar != mydata->uid) {
                    mywsc->dist = dist;
                    mywsc->dist_src = src;
                } else {
                    mywsc->dist = 0;
                    mywsc->dist_src = mydata->uid;
                }
                mywsc->match_cnt = pdu.mch;
                mywsc->target = pdu.tar;
                mywsc->flags &= ~WSC_FLAG_APPROACH;
                if (pdu.mch == WSC_MATCH_MAX) {
                    MOVE_STOP();
                    mywsc->state = WSC_STATE_DONE;
                    mywsc->match_cnt = WSC_MATCH_MAX;
                }
            } else {
                TRACE("IGNORE old %u (current %u)\n",
                        pdu.mch, mywsc->match_cnt);
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
            update_send(TPL_BROADCAST_ADDR);
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
    mywsc->min_dist = DIST_MAX;
    mywsc->min_dist_src = TPL_BROADCAST_ADDR;
    mywsc->target = TPL_BROADCAST_ADDR;
    mydata->tpl.timeout_cb = NULL;
    COLOR_APP(mydata->uid);
    mywsc->radius = RADIUS_MIN;
}
