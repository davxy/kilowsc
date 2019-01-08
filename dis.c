#include "app.h"
#include "dis.h"


#define mydis (&mydata->dis)

/* Minimum time dedicated to the discovery protocol */
#define DIS_MIN_TIME    (10 * KILO_TICKS_PER_SEC)
/* Time that should be elapsed without new discoveries before quit */
#define DIS_QUIET_TIME  (10 * KILO_TICKS_PER_SEC)
/* Time, in kilo ticks, between discovery messages */
#define DIS_RAND_OFF    (rand() % (3 * KILO_TICKS_PER_SEC))


static void neighbor_add(addr_t addr)
{
    uint8_t nneigh = mydata->nneigh;

    app_neighbor_add(addr);
    if (mydata->nneigh > nneigh) {
        mydis->last = kilo_ticks;
        app_neighbor_print();
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
