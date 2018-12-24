#include "wsc.h"
#include "app.h"


/* Temporary: Assume that the witch is the first COLOR to be nominated... */

#define mywsc (&mydata->wsc)

#define DIST_MIN        40
#define DIST_MAX        0xFF

#define BLINK_MIN       10
#define BLINK_MAX       300

#define ENV_DYN_PERIOD          3
#define ECHO_PERIOD             1

#define ENV_DYN_PERIOD_TICKS    (ENV_DYN_PERIOD * KILO_TICKS_PER_SEC)
#define ECHO_PERIOD_TICKS       (ECHO_PERIOD * KILO_TICKS_PER_SEC)

#define ENV_DYN_QUANTITY        10

#define COLLISION_DIST  (DIST_MIN + 10)

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


void print_dist(void)
{
    TRACE_APP("DIST: %u\n", mywsc->dist);
}

int wsc_send(void)
{
    uint8_t data[2];

    data[0] = mywsc->target;
    data[1] = mywsc->dist;
    return chan_send(BROADCAST_ADDR, data, 2);
}

int wsc_recv(uint8_t *src, uint8_t *col, uint8_t *dist)
{
    int res;
    uint8_t data[2];
    uint8_t len = 2;

    if ((res = chan_recv(src, data, &len)) == 0) {
        *col = data[0];
        *dist = data[1];
    }
    return res;
}


#define SUBSTATE_HUNTING            0
#define SUBSTATE_AVOID_COLLISION    1
#define SUBSTATE_FINISH             2
#define SUBSTATE_APPROACHING        3

#define UTURN_TIME                  15
#define UTURN_TICKS                 (UTURN_TIME * KILO_TICKS_PER_SEC)


#define RAND_TURN() do { \
    if (rand() % 2 == 0) \
        MOVE_LEFT(); \
    else \
        MOVE_STOP(); \
} while (0)


static void update_hunter(uint8_t src, uint8_t dist, uint8_t force)
{
    uint8_t prev_dist = mywsc->dist;

    /* Eventually update distance */
    if (force == 1 || dist < prev_dist) {
        mywsc->dist = dist;
        mywsc->dist_src = src;
        mywsc->dina_tick = kilo_ticks + ENV_DYN_PERIOD_TICKS;
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
        //mywsc->sstate = SUBSTATE_AVOID_COLLISION;
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


#define M   (((float)BLINK_MAX - BLINK_MIN) / (DIST_MAX - DIST_MIN))
#define MX1 (M * DIST_MIN)

static void active_hunter(void)
{
    uint32_t offset;

    /* Environment dynamics */
    if (mywsc->state == WSC_STATE_ACTIVE && mywsc->dina_tick < kilo_ticks) {
        uint8_t dist;

        mywsc->dina_tick = kilo_ticks + ENV_DYN_PERIOD_TICKS;
        dist = ((uint16_t)mywsc->dist + ENV_DYN_QUANTITY < DIST_MAX) ?
                mywsc->dist + ENV_DYN_QUANTITY : DIST_MAX;
        update_hunter(mywsc->dist_src, dist, 1);
    }

    if (mydata->color != mywsc->target && mywsc->blink_tick < kilo_ticks) {
        if ((mywsc->flags & WSC_FLAG_COLOR_ON) && mywsc->dist != DIST_MAX) {
            COLOR_APP(WHITE);
            mywsc->flags &= ~WSC_FLAG_COLOR_ON;
        } else {
            COLOR_APP(mydata->color);
            mywsc->flags |= WSC_FLAG_COLOR_ON;
        }
        offset = M * mywsc->dist - MX1 + BLINK_MIN;
        //TRACE_APP("OFFSET (%u): %u\n", mywsc->dist, offset);
        mywsc->blink_tick = kilo_ticks + offset;
    }

    /* Message */
    if (mywsc->echo_tick < kilo_ticks) {
        mywsc->echo_tick = kilo_ticks + ECHO_PERIOD_TICKS;
        wsc_send();
    }
}

static void active_target(void)
{
    /* Movement strategy */

    /* Message */
    if (mywsc->echo_tick < kilo_ticks) {
        mywsc->echo_tick = kilo_ticks + KILO_TICKS_PER_SEC;
        COLOR_APP((uint8_t)kilo_ticks);
        wsc_send();
    }
}

static void idle(uint8_t src, uint8_t col, uint8_t dist)
{
    mywsc->target = col;
    //mywsc->dist = dist;
    //mywsc->dist_src = src;
    mywsc->state = WSC_STATE_ACTIVE;
    update_hunter(src, dist, 1);
    //wsc_send();
}

static void spontaneous(void)
{
    mywsc->target = rand() % mydata->nodes;
    if (mywsc->target == mydata->color)
        mywsc->target = (mywsc->target + 1) % mydata->nodes;
    mywsc->state = WSC_STATE_ACTIVE;
    mywsc->dist = (mywsc->target != mydata->color) ? DIST_MAX : 0;
    mywsc->dist_src = kilo_uid;
    TRACE_APP("WSC: %u\n", mywsc->target);
    wsc_send();
}


void wsc_loop(void)
{
    uint8_t col, dist, src;

    /* spontaneous event for the root */
    if (mywsc->state == WSC_STATE_IDLE && kilo_uid == mydata->neighbors[0]) {
        spontaneous();
        return;
    }

    /* Fetch a message (target drops them) */
    if (wsc_recv(&src, &col, &dist) == 0 && mydata->color != mywsc->target) {
        /* Eventually refresh the env dynamics timer */
        if (src == mywsc->dist_src)
            mywsc->dina_tick = kilo_ticks + 5 * KILO_TICKS_PER_SEC;
        /*
         * Distance from target is computed as the one transported within
         * the message plus the distance detected at channel level.
         */
        dist = ((uint16_t)dist + mydata->chan.dist < DIST_MAX) ?
                dist + mydata->chan.dist : DIST_MAX;

        switch (mywsc->state) {
        case WSC_STATE_IDLE:
            idle(src, col, dist);
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
        if (mydata->color != mywsc->target)
            active_hunter();
        else
            active_target();
    }
}

void wsc_init(void)
{
    memset(mywsc, 0, sizeof(*mywsc));
    mywsc->dist =  DIST_MAX;
    mywsc->dist_src = kilo_uid;
}
