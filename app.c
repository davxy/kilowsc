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
        if (mydata->spt.state == SPT_STATE_DONE &&
                mydata->spt.notify_num == 0 &&
                buf_size(&mydata->chan.tx_buf) == 0) {
            mydata->proto = APP_PROTO_COL;
            COLOR_APP(WHITE);
            mydata->neighbors[0] = mydata->spt.parent;
            memcpy(mydata->neighbors + 1, mydata->spt.childs,
                    mydata->spt.nchilds * sizeof(mydata->neighbors[0]));
            mydata->nneighbors = mydata->spt.nchilds + 1;
            color_init();
        }
        break;
    case APP_PROTO_COL:
        color_loop();
        if (mydata->col.state == COLOR_STATE_DONE) {
            mydata->proto = APP_PROTO_WSC;
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
    memset(mydata, 0, sizeof(*mydata));

    chan_init(0, NULL);

    discover_init();
    mydata->proto = APP_PROTO_DIS;

    set_motors(0, 0);
    set_color(WHITE);

    /* FIXME : Temporary code... JUMP TO WSC */
    wsc_init();
    mydata->proto = APP_PROTO_WSC;
    mydata->color = kilo_uid;
    mydata->nodes = 10;
}

int main(void)
{
    srand(time(NULL));
    kilo_init();
    kilo_start(setup, loop);
    return 0;
}
