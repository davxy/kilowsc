#include "wsc.h"
#include "app.h"


/* Temporary: Assume that the witch is the first COLOR to be nominated... */

#define mywsc (&mydata->wsc)

#define DIST_MAX    255
#define DIST_MIN    40

#define AGING_PERIOD_SECONDS    3
#define ECHO_PERIOD_SECONDS     1
#define ESCAPE_TIME_SECONDS     5

#define AGING_PERIOD_TICKS      (AGING_PERIOD_SECONDS * KILO_TICKS_PER_SEC)
#define ECHO_PERIOD_TICKS       (ECHO_PERIOD_SECONDS * KILO_TICKS_PER_SEC)
#define ESCAPE_TIME_TICKS       (ESCAPE_TIME_SECONDS * KILO_TICKS_PER_SEC)

#define AGING_QUANTITY          10

#define COLLISION_DIST          DIST_MIN

/* Time to take a U turn */
#define UTURN_SECONDS           15
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
    return chan_send(dst, data, sizeof(data));
}

static int wsc_recv(uint8_t *src, struct wsc_pdu *pdu)
{
    int res;
    uint8_t data[4];
    uint8_t len = sizeof(data);

    if ((res = chan_recv(src, data, &len)) == 0) {
        pdu->gid = data[0];
        pdu->mch = data[1];
        pdu->tar = data[2];
        pdu->dis = data[3];
    }
    return res;
}

static int update_send(void)
{
    struct wsc_pdu pdu;

    pdu.gid = mydata->gid;
    pdu.mch = mywsc->match_cnt;
    pdu.tar = mywsc->target;
    pdu.dis = mywsc->dist;
    return wsc_send(BROADCAST_ADDR, &pdu);
}

/* Returns 1 if imminent collision has been detected */
static int collision_avoid(uint8_t src)
{
    int res = 0;

    /* FIXME: temporary code... */
    if (mydata->uid == mywsc->target)
        return 0;

    /* First collision avoidance (FIXME) */
    if (mydata->chan.dist <= COLLISION_DIST) {
        if (src < kilo_uid)
            MOVE_LEFT();
        else
            MOVE_STOP();
        mywsc->move_tick = kilo_ticks + (3 + rand() % 10) * KILO_TICKS_PER_SEC;
        TRACE_APP("AVOID COLLISION (%s)!!!!\n",
                (mywsc->move == MOVING_LEFT) ? "left" : "stop");
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
        TRACE_APP("DISTANCE: %u\n", mywsc->dist);
    }

    if (mywsc->dist <= DIST_MIN) {
        MOVE_STOP();
        return;
    }

    /* First collision avoidance (FIXME) */
    if (dist != mydata->chan.dist && mydata->chan.dist <= COLLISION_DIST) {
        if (src < kilo_uid)
            MOVE_LEFT();
        else
            MOVE_STOP();
        mywsc->move_tick = kilo_ticks + (3 + rand() % 10) * KILO_TICKS_PER_SEC;
        TRACE_APP("AVOID COLLISION (%s)!!!!\n",
                (mywsc->move == MOVING_LEFT) ? "left" : "stop");
        return;
    }

    if (dist > prev_dist) {// || prev_dist == DIST_MAX) {
        if (mywsc->flags & WSC_FLAG_APPROACH) {
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
                mywsc->move_tick = kilo_ticks + 6 * KILO_TICKS_PER_SEC;
            } else {
                MOVE_FRONT();
                mywsc->move_tick = kilo_ticks + 10 * KILO_TICKS_PER_SEC;
            }
            TRACE_APP("HUNTING\n");
        }
        mywsc->flags &= ~WSC_FLAG_APPROACH;
    } else if ((dist < prev_dist && (prev_dist - dist) > 3)
            || mywsc->move_tick < kilo_ticks) {
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
    /* Distance information aging */
    if (mywsc->aging_tick < kilo_ticks) {
        uint8_t dist;

        mywsc->aging_tick = kilo_ticks + AGING_PERIOD_TICKS;
        dist = ((uint16_t)mywsc->dist + AGING_QUANTITY < DIST_MAX) ?
                mywsc->dist + AGING_QUANTITY : DIST_MAX;
        update_hunter(mywsc->dist_src, dist, 1);
    }
}

static void active_target(void)
{
    /* Movement strategy */
    MOVE_STOP();
}

static void idle(uint8_t src, uint8_t target, uint8_t dist)
{
    mywsc->target = target;
    mywsc->state = WSC_STATE_ACTIVE;
    update_hunter(src, dist, 1);
}

static void spontaneous(void)
{
    ASSERT(mydata->nodes != 0);
    mywsc->target = rand() % mydata->nodes;
    mywsc->state = WSC_STATE_ACTIVE;
    mywsc->dist = (mywsc->target != mydata->uid) ? DIST_MAX : 0;
    mywsc->dist_src = kilo_uid;
    TRACE_APP("WSC: %u\n", mywsc->target);
    update_send();
}


void wsc_loop(void)
{
    uint8_t dist, src;
    struct wsc_pdu pdu;

    /* spontaneous event for the root */
    if (mywsc->state == WSC_STATE_IDLE && mydata->uid == mydata->neighbors[0]) {
        spontaneous();
        return;
    }

    /* Fetch a message (target silently ignores messages) */
    if (wsc_recv(&src, &pdu) == 0 && mydata->uid != mywsc->target) {
        /*
         * Distance from target is computed as the one transported within
         * the message plus the distance detected at channel level.
         */
        dist = ((uint16_t)pdu.dis + mydata->chan.dist < DIST_MAX) ?
                pdu.dis + mydata->chan.dist : DIST_MAX;

        if (mydata->uid == mywsc->target && mydata->chan.dist <= DIST_MIN) {
            TRACE(">>> CATCHED <<<\n");
            mywsc->match_cnt++;
            mywsc->target = src;
            mywsc->dist = mydata->chan.dist;
            mywsc->dist_src = src;
            /* Give him some time to escape */
            mywsc->echo_tick = kilo_ticks + ESCAPE_TIME_TICKS;
            return;
        }

        /* Safety first */
        if (mywsc->target != src && mywsc->state == WSC_STATE_ACTIVE) {
            if (collision_avoid(src) != 0)
                return; /* Imminent collision... Nothing to do */
        }

        /* Ignore messages of other groups */
        if (pdu.gid != mydata->gid) {
            TRACE("IGNORE other group\n");
            return;
        }

        /* Eventually refresh the distance aging timer */
        if (src == mywsc->dist_src)
            mywsc->aging_tick = kilo_ticks + 5 * KILO_TICKS_PER_SEC;

        switch (mywsc->state) {
        case WSC_STATE_IDLE:
            idle(src, pdu.tar, dist);
            break;
        case WSC_STATE_ACTIVE:
            update_hunter(src, dist, 0);
            break;
        default:
            break;
        }
    }

    /* Active loop */
    if (mywsc->state == WSC_STATE_ACTIVE) {
        if (mywsc->echo_tick < kilo_ticks) {
            mywsc->echo_tick = kilo_ticks + ECHO_PERIOD_TICKS;
            if (mywsc->target == mydata->uid)
                blink();
            update_send();
        }
        if (mywsc->target != mydata->uid)
            active_hunter();
        else
            active_target();
    }
}

void wsc_init(void)
{
    memset(mywsc, 0, sizeof(*mywsc));
    mywsc->dist =  DIST_MAX;
    mywsc->dist_src = mydata->uid;
    mywsc->target = BROADCAST_ADDR;
    COLOR_APP(mydata->uid);
}
