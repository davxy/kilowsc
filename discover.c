#include "discover.h"
#include "app.h"


#define mydisc (&mydata->discover)


static void neighbor_print(void)
{
    int i;

    TRACE("N(x) = { ");
    for (i = 0; i < mydata->nneighbors; i++)
        TRACE2("P%03u ", mydata->neighbors[i]);
    TRACE2("}\n");
}

static void neighbor_add(addr_t addr)
{
    int i;

    for (i = 0; i < mydata->nneighbors; i++) {
        if (mydata->neighbors[i] == addr)
            break;
    }
    if (i == mydata->nneighbors) {
        mydata->neighbors[i] = addr;
        mydata->nneighbors++;
    }
}

void discover_loop(void)
{
    addr_t src;

    if (mydisc->start == 0) {
        mydisc->start = kilo_ticks; /*First run */
        mydisc->next = kilo_ticks + DISCOVERY_RAND_OFF;
    }

    if (kilo_ticks >= mydisc->start + DISCOVERY_TIME) {
        mydisc->state = DISCOVER_STATE_DONE;
        APP_COLOR(GREEN);
        return;
    }

    if (mydisc->state == DISCOVER_STATE_IDLE) {
        /* some rand to try to avoid collisions */
        if (kilo_ticks > mydisc->next) {
            mydisc->state = DISCOVER_STATE_ACTIVE;
            APP_COLOR(RED);
            chan_send(BROADCAST_ADDR, NULL, 0);
            mydisc->next = kilo_ticks + DISCOVERY_RAND_OFF;
            neighbor_print();
        }
    } else {
        mydisc->state = DISCOVER_STATE_IDLE;
        APP_COLOR(WHITE);
    }

    if (chan_recv(&src, NULL, NULL) >= 0)
        neighbor_add(src);
}

void discover_init(void)
{
    memset(mydisc, 0, sizeof(*mydisc));
    mydisc->start = kilo_ticks;
    mydisc->state = DISCOVER_STATE_IDLE;
    APP_COLOR(WHITE);
}
