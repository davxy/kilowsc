#include "hnt.h"

#include "app.h"


/* Temporary: Assume that the witch is the first COLOR to be nominated... */

#define myhnt (&mydata->hnt)

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
#define UTURN_SECONDS           25
#define UTURN_TICKS             (UTURN_SECONDS * KILO_TICKS_PER_SEC)

#define RADIUS_MIN              3
#define RADIUS_MAX              50
#define RADIUS_STEP             5


/* Current movement */
#define MOVING_STOP     0
#define MOVING_FRONT    1
#define MOVING_LEFT     2
#define MOVING_RIGHT    3

#define MOVE_STOP() do { \
    myhnt->move = MOVING_STOP; \
    set_motors(0, 0); \
} while (0)

#define MOVE_FRONT() do { \
    myhnt->move = MOVING_FRONT; \
    set_motors(kilo_straight_left, kilo_straight_right); \
} while (0)

#define MOVE_LEFT() do { \
    myhnt->move = MOVING_LEFT; \
    set_motors(kilo_turn_left, 0); \
} while (0)

#define MOVE_RIGHT() do { \
    myhnt->move = MOVING_RIGHT; \
    set_motors(0, kilo_turn_right); \
} while (0)


#define COLOR_ON() do { \
    COLOR_APP(mydata->uid); \
    myhnt->flags |= HNT_FLAG_COLOR_ON; \
} while (0)

#define COLOR_OFF() do { \
    COLOR_APP(WHITE); \
    myhnt->flags &= ~HNT_FLAG_COLOR_ON; \
} while (0)


struct hnt_pdu {
    uint8_t gid;    /* Group identifier */
    uint8_t mch;    /* Match counter */
    uint8_t tar;    /* Target id */
    uint8_t dis;    /* Distance */
};

static int hnt_send(uint8_t dst, struct hnt_pdu *pdu)
{
    uint8_t data[4];

    data[0] = pdu->gid;
    data[1] = pdu->mch;
    data[2] = pdu->tar;
    data[3] = pdu->dis;
    return app_send(dst, data, sizeof(data));
}

static int hnt_recv(uint8_t *src, struct hnt_pdu *pdu)
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
    struct hnt_pdu pdu;

    pdu.gid = mydata->gid;
    pdu.mch = myhnt->match_cnt;
    pdu.tar = myhnt->target;
    pdu.dis = myhnt->dist;
    return hnt_send(dst, &pdu);
}

/* Returns 1 if imminent collision has been detected */
static int collision_avoid(uint8_t src, uint8_t match_cnt)
{
    int res = 0;

    /*
     * If the source is the target and we are playing the right match then
     * don't try to avoid collisions
     */
    if (src == myhnt->target && match_cnt == myhnt->match_cnt)
        return 0;

    if (mydata->tpl.dist < COLLISION_DIST ) {
        TRACE_APP("AVOID COLLISION!!!\n");
        if (myhnt->min_dist_src == src &&
            mydata->tpl.dist < myhnt->min_dist) {
            if (myhnt->move == MOVING_STOP) {
                MOVE_LEFT();
                myhnt->move_tick = kilo_ticks + UTURN_TICKS;
            } else {
                MOVE_STOP();
            }
        } else {
            TRACE("NEW\n");
            myhnt->min_dist_src = src;
            if (mydata->uid < src)
                MOVE_LEFT();
            else
                MOVE_STOP();
        }
        myhnt->min_dist = mydata->tpl.dist;
        res = (mydata->uid != myhnt->target) ? 1: 0;
    }
    return res;
}

static void update_hunter(uint8_t src, uint8_t dist, uint8_t force)
{
    uint8_t prev_dist = myhnt->dist;
    uint8_t approach;

    /* Eventually update distance */
    if (force == 1 || dist < prev_dist) {
        myhnt->dist = dist;
        myhnt->dist_src = src;
        myhnt->aging_tick = kilo_ticks + AGING_PERIOD_TICKS;
    }

    approach = dist + 5 <= prev_dist;
    if (approach == 0 || dist == DIST_MAX || src == kilo_uid) {
        /* Distance has increased */
        if ((myhnt->flags & HNT_FLAG_APPROACH) != 0) {
            /* Was approaching */
            if (src == myhnt->dist_src) {
                myhnt->move_tick = kilo_ticks + UTURN_TICKS;
                MOVE_LEFT();
                TRACE_APP("U TURN\n");
            } else {
                TRACE_APP("IGNORE (src=%u, appr=%u)\n", src, myhnt->dist_src);
            }
            myhnt->flags &= ~HNT_FLAG_APPROACH;
            myhnt->radius = RADIUS_MIN;
        } else if (myhnt->move_tick < kilo_ticks) {
            if (myhnt->move == MOVING_FRONT) {
                MOVE_LEFT();
                myhnt->move_tick = kilo_ticks + 5 * KILO_TICKS_PER_SEC;
            } else {
                MOVE_FRONT();
                myhnt->move_tick = kilo_ticks + myhnt->radius * KILO_TICKS_PER_SEC;
                TRACE(">>> ticks %u, radius %u\n", myhnt->move_tick, myhnt->radius);
                myhnt->radius = ((uint16_t)myhnt->radius + RADIUS_STEP < RADIUS_MAX) ?
                                myhnt->radius + RADIUS_STEP : RADIUS_MAX;
            }
            TRACE_APP("HUNTING\n");
            myhnt->flags &= ~HNT_FLAG_APPROACH;
        }
    } else if (approach == 1) {
        MOVE_FRONT();
        myhnt->flags |= HNT_FLAG_APPROACH;
        TRACE_APP("APPROACHING to %u\n", src);
    }

}


static void blink(void)
{
    if ((myhnt->flags & HNT_FLAG_COLOR_ON) != 0)
        COLOR_OFF();
    else
        COLOR_ON();
}

static void active_hunter(void)
{
    uint8_t dist;

    /* Distance information aging */
    if (myhnt->aging_tick > kilo_ticks)
        return;

    myhnt->aging_tick = kilo_ticks + AGING_PERIOD_TICKS;
    dist = ((uint16_t)myhnt->dist + AGING_QUANTITY < DIST_MAX) ?
            myhnt->dist + AGING_QUANTITY : DIST_MAX;
    update_hunter(myhnt->dist_src, dist, 1);
}

static void active_target(void)
{
    uint8_t dir;

    if (myhnt->move_tick > kilo_ticks)
        return;
    myhnt->move_tick = kilo_ticks + (5 + rand() % 10) * TICKS_PER_SEC;

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
    myhnt->match_cnt++;
    if (myhnt->match_cnt < HNT_MATCH_MAX) {
        myhnt->target = target;
        myhnt->dist = mydata->tpl.dist;
        myhnt->dist_src = target;
        myhnt->state = HNT_STATE_SLEEP;
        /* Give him some time to escape */
        //myhnt->move_tick = kilo_ticks + SLEEP_TIME_TICKS;
        MOVE_STOP();
        COLOR_ON();
        TRACE_APP("SLEEP...\n");
    } else {
        myhnt->state = HNT_STATE_DONE;
        myhnt->match_cnt = HNT_MATCH_MAX;
    }
    MOVE_STOP();
    update_send(TPL_BROADCAST_ADDR);
}

/* Execuited to elect the target at the beginning of the first match */
static void spontaneous(void)
{
    myhnt->target = TPL_BROADCAST_ADDR;
    myhnt->state = HNT_STATE_ACTIVE;
    myhnt->dist = DIST_MAX;
    myhnt->dist_src = mydata->uid;
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
        myhnt->target = mydata->uid;
        myhnt->dist = 0;
        myhnt->dist_src = mydata->uid;
        myhnt->state = HNT_STATE_ACTIVE;
        myhnt->match_cnt++;
        update_send(TPL_BROADCAST_ADDR);
    }
    return;
}



void hnt_loop(void)
{
    uint8_t dist, src;
    struct hnt_pdu pdu;

    /* Spontaneous event from the group leader. */
    if (myhnt->state == HNT_STATE_IDLE && mydata->uid == mydata->gid) {
        spontaneous();
        return;
    }

    if (myhnt->state == HNT_STATE_DONE) {
        if (myhnt->echo_tick <= kilo_ticks) {
            myhnt->echo_tick = kilo_ticks + 3*ECHO_PERIOD_TICKS;
            update_send(TPL_BROADCAST_ADDR);
        }
        return;
    }

    if (myhnt->target == mydata->uid && myhnt->blink_tick < kilo_ticks) {
        myhnt->blink_tick = myhnt->blink_tick + BLINK_TIME_TICKS;
        blink();
    }

    /* Fetch a message (target silently ignores messages) */
    if (hnt_recv(&src, &pdu) == 0) {
        /* The captured target sleeps a bit before start the new match */
        if (myhnt->state == HNT_STATE_SLEEP) {
            if (myhnt->echo_tick <= kilo_ticks) {
                myhnt->echo_tick = kilo_ticks + ECHO_PERIOD_TICKS;
                update_send(TPL_BROADCAST_ADDR);
            }
            if (myhnt->move_tick <= kilo_ticks) {
                TRACE_APP("WAKEUP!!!\n");
                myhnt->state = HNT_STATE_ACTIVE;
            } else {
                return;
            }
        }

        /* Avoid collisions */
        if (myhnt->state != HNT_STATE_IDLE &&
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
        if (pdu.tar == TPL_BROADCAST_ADDR && myhnt->target == TPL_BROADCAST_ADDR) {
            target_search(src);
            return;
        }

        /*
         * Target is captured if the distance from the last message source
         * is less than a given threshold.
         */
        if (mydata->uid == myhnt->target && mydata->tpl.dist <= DIST_MIN
            && pdu.mch == myhnt->match_cnt) {
            TRACE_APP(">>> TARGET CAPTURED <<<\n");
            new_match(src); /* Start a new match */
            return;
        }

        /* Eventually refresh the distance aging timer */
        if (src == myhnt->dist_src)
            myhnt->aging_tick = kilo_ticks + 5 * KILO_TICKS_PER_SEC;

        /*
         * Distance from target is computed as the one transported within
         * the message plus the distance detected at tplnel level.
         */
        dist = ((uint16_t)pdu.dis + mydata->tpl.dist < DIST_MAX) ?
                pdu.dis + mydata->tpl.dist : DIST_MAX;

        if (myhnt->state != HNT_STATE_IDLE) {
            /* Check if a new match is started */
            if (pdu.mch == myhnt->match_cnt) {
                if (mydata->uid != myhnt->target)
                    update_hunter(src, dist, 0);
            } else if (pdu.mch > myhnt->match_cnt) {
                if (pdu.tar != mydata->uid) {
                    myhnt->dist = dist;
                    myhnt->dist_src = src;
                } else {
                    myhnt->dist = 0;
                    myhnt->dist_src = mydata->uid;
                    MOVE_LEFT();
                    myhnt->move_tick = kilo_ticks + 15;
                }
                myhnt->match_cnt = pdu.mch;
                myhnt->target = pdu.tar;
                myhnt->flags &= ~HNT_FLAG_APPROACH;
                if (pdu.mch == HNT_MATCH_MAX) {
                    MOVE_STOP();
                    myhnt->state = HNT_STATE_DONE;
                    myhnt->match_cnt = HNT_MATCH_MAX;
                }
            } else {
                TRACE("IGNORE old %u (current %u)\n",
                        pdu.mch, myhnt->match_cnt);
            }
        } else {
            myhnt->state = HNT_STATE_ACTIVE;
            myhnt->match_cnt = pdu.mch;
            myhnt->target = pdu.tar;
            if (pdu.tar != mydata->uid) {
                myhnt->dist = dist;
                myhnt->dist_src = src;
            } else {
                myhnt->dist = 0;
                myhnt->dist_src = mydata->uid;
            }
        }
    }

    /* Active loop */
    if (myhnt->state == HNT_STATE_ACTIVE && myhnt->target != TPL_BROADCAST_ADDR) {
        if (myhnt->target != mydata->uid)
            active_hunter();
        else
            active_target();
        if (myhnt->echo_tick < kilo_ticks) {
            myhnt->echo_tick = kilo_ticks + ECHO_PERIOD_TICKS;
            update_send(TPL_BROADCAST_ADDR);
            TRACE_APP("STAT: gid=%u, tar=%u, mch=%u, dis=: %u\n",
                    mydata->gid, myhnt->target, myhnt->match_cnt, myhnt->dist);
        }
    }
}

void hnt_init(void)
{
    memset(myhnt, 0, sizeof(*myhnt));
    myhnt->dist =  DIST_MAX;
    myhnt->dist_src = mydata->uid;
    myhnt->min_dist = DIST_MAX;
    myhnt->min_dist_src = TPL_BROADCAST_ADDR;
    myhnt->target = TPL_BROADCAST_ADDR;
    COLOR_ON();
    myhnt->radius = RADIUS_MIN;
}
