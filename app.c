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
        if (mydata->spt.state == SPT_STATE_DONE) {
            mydata->gid = (uint8_t)mydata->spt.root;
            mydata->proto = APP_PROTO_WSC;
            mydata->neighbors[0] = mydata->spt.parent;
            memcpy(mydata->neighbors + 1, mydata->spt.childs,
                   mydata->spt.nchilds * sizeof(mydata->neighbors[0]));
            mydata->nneighbors = mydata->spt.nchilds + 1;
            wsc_init();
        }
        break;
    case APP_PROTO_WSC:
        wsc_loop();
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

    mydata->uid = kilo_uid;

#ifndef SKIP_ELECTION
    mydata->gid = BROADCAST_ADDR;
    mydata->proto = APP_PROTO_DIS;
    discover_init();
#else
    wsc_init();
    mydata->gid = DEFAULT_WSC_GID;
    mydata->proto = APP_PROTO_WSC;
    if (mydata->uid == mydata->gid) {
        /*
         * Hack to let the wsc work as expected.
         * The kilobot with uid equal to DEFAULT_WSC_GID will be both the
         * first witch and the target.
         */
        mydata->nneighbors = 1;
        mydata->neighbors[0] = mydata->uid;
        mydata->wsc.dist = 0;
    }
    mydata->wsc.target = DEFAULT_WSC_GID;
    mydata->wsc.state  = WSC_STATE_ACTIVE;
#endif
}

int main(void)
{
    srand(time(NULL));
    kilo_init();
    kilo_start(setup, loop);
    return 0;
}
