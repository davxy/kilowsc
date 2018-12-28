#include "app.h"
#include "dis.h"


#define mydis (&mydata->dis)


#define DIS_TIME          (10 * KILO_TICKS_PER_SEC)

#define DIS_RAND_OFF \
        (rand() % (3 * KILO_TICKS_PER_SEC))


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

void dis_loop(void)
{
    addr_t src;

    if (mydis->start == 0) {
        mydis->start = kilo_ticks; /*First run */
        mydis->next = kilo_ticks + DIS_RAND_OFF;
    }

    if (kilo_ticks >= mydis->start + DIS_TIME) {
        mydis->state = DIS_STATE_DONE;
        COLOR_APP(GREEN);
        return;
    }

    if (mydis->state == DIS_STATE_IDLE) {
        /* some rand to try to avoid collisions */
        if (kilo_ticks > mydis->next) {
            mydis->state = DIS_STATE_ACTIVE;
            COLOR_APP(RED);
            tpl_send(TPL_BROADCAST_ADDR, NULL, 0);
            mydis->next = kilo_ticks + DIS_RAND_OFF;
            neighbor_print();
        }
    } else {
        mydis->state = DIS_STATE_IDLE;
        COLOR_APP(WHITE);
    }

    if (tpl_recv(&src, NULL, NULL) >= 0)
        neighbor_add(src);
}

void dis_init(void)
{
    memset(mydis, 0, sizeof(*mydis));
    mydis->start = kilo_ticks;
    mydis->state = DIS_STATE_IDLE;
    COLOR_APP(WHITE);
}
