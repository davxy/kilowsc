#include "discover.h"
#include "app.h"


#define mydis (&mydata->dis)


static void neighbor_print(void)
{
    int i;

    TRACE_APP("N(x) = { ");
    for (i = 0; i < mydata->nneigh; i++)
        TRACE2_APP("P%03u ", mydata->neigh[i]);
    TRACE2_APP("}\n");
}

static void neighbor_add(addr_t addr)
{
    int i;

    for (i = 0; i < mydata->nneigh; i++) {
        if (mydata->neigh[i] == addr)
            break;
    }
    if (i == mydata->nneigh) {
        mydata->neigh[i] = addr;
        mydata->nneigh++;
    }
}

void discover_loop(void)
{
    addr_t src;

    if (mydis->start == 0) {
        mydis->start = kilo_ticks; /*First run */
        mydis->next = kilo_ticks + DISCOVERY_RAND_OFF;
    }

    if (kilo_ticks >= mydis->start + DISCOVERY_TIME) {
        mydis->state = DISCOVER_STATE_DONE;
        COLOR_APP(GREEN);
        return;
    }

    if (mydis->state == DISCOVER_STATE_IDLE) {
        /* some rand to try to avoid collisions */
        if (kilo_ticks > mydis->next) {
            mydis->state = DISCOVER_STATE_ACTIVE;
            COLOR_APP(RED);
            tpl_send(TPL_BROADCAST_ADDR, NULL, 0);
            mydis->next = kilo_ticks + DISCOVERY_RAND_OFF;
            neighbor_print();
        }
    } else {
        mydis->state = DISCOVER_STATE_IDLE;
        COLOR_APP(WHITE);
    }

    if (tpl_recv(&src, NULL, NULL) >= 0)
        neighbor_add(src);
}

void discover_init(void)
{
    memset(mydis, 0, sizeof(*mydis));
    mydis->start = kilo_ticks;
    mydis->state = DISCOVER_STATE_IDLE;
    COLOR_APP(WHITE);
}
