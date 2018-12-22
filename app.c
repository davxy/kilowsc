#include "app.h"
#include <string.h>
#include <time.h>


REGISTER_USERDATA(app_ctx_t)

static void loop(void)
{
    switch (mydata->proto) {
    case APP_PROTO_DIS:
        discover_loop();
        if (mydata->discover.state == DISCOVER_STATE_DONE) {
            chan_flush();
            if (mydata->nneighbors > 0) {
                mydata->proto = APP_PROTO_SPT;
                spt_init();
            } else {
                mydata->proto = 0xFF;
                COLOR_APP(WHITE);
            }
        }
        break;
    case APP_PROTO_SPT:
        spt_loop();
#if 0
        if (mydata->spt.state == SPT_STATE_DONE &&
                mydata->spt.notify_num == 0 &&
                buf_size(&mydata->chan.tx_buf) == 0) {
            mydata->proto = APP_PROTO_SPT;
            COLOR_APP(WHITE);
            mydata->neighbors[0] = mydata->spt.parent;
            memcpy(mydata->neighbors + 1, mydata->spt.childs,
                    mydata->spt.nchilds * sizeof(mydata->neighbors[0]));
            mydata->nneighbors = mydata->spt.nchilds + 1;
            wsc_init();
        }
#endif
        break;
    case APP_PROTO_WSC:
        //wsc_loop();
        break;
    default:
        break;
    }
}


static void setup(void)
{
    set_motors(0, 0);
    set_color(WHITE);

    /* Communication channel init */
    chan_init(0, NULL);

    /* Application data init */
    memset(mydata, 0, sizeof(*mydata));

#ifndef SKIP_ELECTION
    mydata->proto = APP_PROTO_DIS;
    discover_init();
#else
    mydata->proto = APP_PROTO_WSC;
    mydata->color = kilo_uid;
    mydata->nodes = 1; /* Prevents div by zero */
    mydata->neighbors[0] =(kilo_uid == 0) ? kilo_uid : BROADCAST_ADDR;
    wsc_init();
#endif
}

int main(void)
{
    srand(time(NULL));
    kilo_init();
    kilo_start(setup, loop);
    return 0;
}
