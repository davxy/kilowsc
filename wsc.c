#include "wsc.h"
#include "app.h"


/* Temporary: Assume that the witch is the first COLOR to be nominated... */

#define mywsc (&mydata->wsc)

#define DIST_MIN        40
#define DIST_MAX        0xFF

#define BLINK_MIN       10
#define BLINK_MAX       300

#define ENV_DYN_PERIOD          1
#define ECHO_PERIOD             3

#define ENV_DYN_PERIOD_TICKS    (ENV_DYN_PERIOD * KILO_TICKS_PER_SEC)
#define ECHO_PERIOD_TICKS       (ECHO_PERIOD * KILO_TICKS_PER_SEC)

void print_dist(void)
{
    TRACE("DIST: %u\n", mywsc->dist);
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

#define MOVE_STRAIGHT   0
#define MOVE_LEFT       1
#define MOVE_RIGHT      2

#define THRESOLD 10

#define SUBSTATE_HUNTING            0
#define SUBSTATE_AVOID_COLLISION    1
#define SUBSTATE_FINISH             2
#define SUBSTATE_APPROACHING        3

#define UTURN_TIME                  15

static void rand_turn(void)
{
    if (rand() % 2 == 0) {
        set_motors(kilo_turn_left, 0);
        mywsc->move = MOVE_LEFT;
    } else {
        set_motors(0, kilo_turn_right);
        mywsc->move = MOVE_RIGHT;
    }
}


static void update_hunter(uint8_t src, uint8_t dist, uint8_t force)
{
    if (mywsc->dist <= DIST_MIN) {
        mywsc->sstate = SUBSTATE_FINISH;
        set_motors(0, 0);
        return;
    }

    /* First collision avoidance */
    if (dist != mydata->chan.dist && mydata->chan.dist <= (DIST_MIN + 5)) {
        rand_turn();
        mywsc->move_tick = kilo_ticks + (3 + rand() % 10) * KILO_TICKS_PER_SEC;
        mywsc->sstate = SUBSTATE_AVOID_COLLISION;
        TRACE("!!!!!!!!!!AVOID COLLISION!!!!\n");
        return;
    }

    if (dist > mywsc->dist || mywsc->dist == DIST_MAX) {
        if (mywsc->sstate == SUBSTATE_APPROACHING) {
            if (src == mywsc->dist_src) {
                set_motors(kilo_turn_left, 0);
                mywsc->move = MOVE_LEFT;
                mywsc->move_tick = kilo_ticks + UTURN_TIME * KILO_TICKS_PER_SEC;
                TRACE("U TURN\n");
            } else {
                TRACE("Ignore (src=%u, approaching=%u)\n", src, mywsc->dist_src);
            }
        } else if (mywsc->move_tick < kilo_ticks) {
            if (mywsc->move == MOVE_STRAIGHT) {
                set_motors(kilo_straight_left, 0);
                mywsc->move = MOVE_LEFT;
                mywsc->move_tick = kilo_ticks + 6 * KILO_TICKS_PER_SEC;
            } else {
                set_motors(kilo_straight_left, kilo_straight_right);
                mywsc->move = MOVE_STRAIGHT;
                mywsc->move_tick = kilo_ticks + 10 * KILO_TICKS_PER_SEC;
            }
            TRACE("HUNT\n");
        }
        mywsc->sstate = SUBSTATE_HUNTING;
    } else if ((dist < mywsc->dist && (mywsc->dist - dist) > 5)
            || mywsc->move_tick < kilo_ticks) {
        set_motors(kilo_straight_left, kilo_straight_right);
        mywsc->move = MOVE_STRAIGHT;
        mywsc->sstate = SUBSTATE_APPROACHING;
        mywsc->dist_src = src;
        TRACE("APPROACH to %u\n", src);
    }


    if (force || dist < mywsc->dist) {
        mywsc->dist = dist;
        TRACE("DIST: %u\n", mywsc->dist);
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
        dist = (mywsc->dist + 10 < DIST_MAX) ? mywsc->dist + 10 : DIST_MAX;
        update_hunter(mywsc->dist_src, dist, 1);
    }

    if (mydata->color != mywsc->target && mywsc->blink_tick < kilo_ticks) {
        if (mywsc->color_enabled == 1 && mywsc->dist != DIST_MAX) {
            set_color(0);
            mywsc->color_enabled = 0;
        } else {
            set_color(mydata->color);
            mywsc->color_enabled = 1;
        }
        offset = M * mywsc->dist - MX1 + BLINK_MIN;
        //TRACE("OFFSET (%u): %u\n", mywsc->dist, offset);
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
        set_color((uint8_t)kilo_ticks);
        wsc_send();
    }
}

static void idle(uint8_t src, uint8_t col, uint8_t dist)
{
    mywsc->target = col;
    mywsc->dist = (mywsc->target != mydata->color) ? dist : 0;
    mywsc->dist_src = src;
    mywsc->state = WSC_STATE_ACTIVE;
    wsc_send();
}

static void spontaneous(void)
{
    //mywsc->target = rand() % mydata->nodes;
    //if (mywsc->target == mydata->color)
    //    mywsc->target = (mywsc->target + 1) % mydata->nodes;
    mywsc->target = 0;
    TRACE("WSC: %u\n", mywsc->target);
    idle(kilo_uid, mywsc->target, 0);
}


void wsc_loop(void)
{
    uint8_t col, dist, src;

    /* spontaneous event for the root */
    if (mywsc->state == WSC_STATE_IDLE && kilo_uid == 0) {
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
    if (kilo_uid != 0) {
        mywsc->dist =  DIST_MAX;
        mywsc->color_enabled = 0;
    }
    mywsc->move = MOVE_STRAIGHT;
}
