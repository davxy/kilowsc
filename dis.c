#include "app.h"
#include "dis.h"


#define mydis (&mydata->dis)

/* Minimum time dedicated to the discovery protocol */
#define DIS_MIN_TIME    (10 * KILO_TICKS_PER_SEC)
/* Time that should be elapsed without new discoveries before quit */
#define DIS_QUIET_TIME  (10 * KILO_TICKS_PER_SEC)
/* Time, in kilo ticks, between discovery messages */
#define DIS_RAND_OFF    (rand() % (3 * KILO_TICKS_PER_SEC))


static void neighbor_print(void)
{
    int i;

    TRACE_APP("N(x) = { ");
    for (i = 0; i < mydata->nneigh; i++)
        TRACE2_APP("%u ", mydata->neigh[i]);
    TRACE2_APP("}\n");
}

static void neighbor_add(addr_t addr)
{
    uint8_t i;

    for (i = 0; i < mydata->nneigh; i++) {
        if (mydata->neigh[i] == addr)
            break;
    }
    if (i == mydata->nneigh) {
        if (i < APP_NEIGHBORS_MAX) {
            mydata->neigh[i] = addr;
            mydata->nneigh++;
            mydis->last = kilo_ticks;
            neighbor_print();
        } else {
            TRACE("NEIGHBOR %u ignored, max capacity reached\n", addr);
        }
    }
}

void dis_loop(void)
{
    addr_t src;

    if (mydis->start == 0) {
        mydis->start = kilo_ticks; /*First run */
        mydis->next = kilo_ticks + DIS_RAND_OFF;
    }

    if (kilo_ticks - mydis->start >= DIS_MIN_TIME &&
        kilo_ticks - mydis->last >= DIS_QUIET_TIME) {
        mydis->state = DIS_STATE_DONE;
        COLOR_APP(GREEN);
        TRACE_APP("DIS DONE\n");
        return;
    }

    if (mydis->state == DIS_STATE_IDLE) {
        if (kilo_ticks >= mydis->next) {
            mydis->state = DIS_STATE_ACTIVE;
            COLOR_APP(RED);
            app_send(TPL_BROADCAST_ADDR, NULL, 0);
            mydis->next = kilo_ticks + DIS_RAND_OFF;
        }
    } else {
        mydis->state = DIS_STATE_IDLE;
        COLOR_APP(WHITE);
    }

    if (app_recv(&src, NULL, NULL) == 0)
        neighbor_add(src);
}

void dis_init(void)
{
    memset(mydis, 0, sizeof(*mydis));
    mydis->start = kilo_ticks;
    mydis->state = DIS_STATE_IDLE;
    COLOR_APP(WHITE);
}
