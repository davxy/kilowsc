#include "color.h"
#include "app.h"

#define mycol (&mydata->col)

static void active(uint8_t color)
{
    addr_t dst;

    /* should be a child returning */
    if (mycol->idx == mydata->nneighbors) {
        /* finished with the childs, return to parent */
        mycol->state = COLOR_STATE_DONE;
        if (mydata->neighbors[0] == BROADCAST_ADDR) {
            mydata->nodes = color + 1;
            TRACE("FOUND %u participants\n", mydata->nodes);
            return;
        }
        dst = mydata->neighbors[0]; /* parent is the first neighbor */
    } else {
        /* send to next child */
        color++; /* increment the biggest child color */
        dst = mydata->neighbors[mycol->idx++];
    }
    if (chan_send(dst, &color, 1) < 0)
        ASSERT(0); /* Should never happen */
}

static void idle(uint8_t color)
{
    mycol->state = COLOR_STATE_ACTIVE;
    mydata->color = color;
    APP_COLOR(color);
    mycol->idx++; /* Position to the first children */
}

void color_loop(void)
{
    uint8_t color;
    uint8_t siz = 1;

    /* spontaneous event for the root */
    if (mycol->state == COLOR_STATE_IDLE &&
            mydata->neighbors[0] == BROADCAST_ADDR &&
            mycol->start < kilo_ticks) {
        idle(mydata->color);
        active(mydata->color);
        return;
    }

    if (chan_recv(NULL, &color, &siz) < 0)
        return;
 
    switch (mycol->state) {
    case COLOR_STATE_ACTIVE:
        active(color);
        break;
    case COLOR_STATE_IDLE:
        idle(color);
        active(color);
        break;
    case COLOR_STATE_DONE:
    default:
        break;
    }
}

void color_init(void)
{
    memset(mycol, 0, sizeof(*mycol));
    mycol->start = kilo_ticks + 10 * KILO_TICKS_PER_SEC;
}
