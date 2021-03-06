#include "app.h"
#include <string.h>
#include <time.h>

/* Application context data instance */
REGISTER_USERDATA(app_ctx_t)


/* Application wrapper layer data send */
int app_send(addr_t dst, uint8_t *data, uint8_t size)
{
    int res;
    uint8_t wdata[TPL_SDU_MAX];

    if (size + 1 > TPL_SDU_MAX)
        return -1;
    wdata[0] = mydata->proto;
    memcpy(wdata + 1, data, size);
    res = tpl_send(dst, wdata, size + 1);
    return res;
}

/* Application wrapper layer data recv */
int app_recv(addr_t *src, uint8_t *data, uint8_t *size)
{
    int res;
    uint8_t wdata[TPL_SDU_MAX];
    uint8_t wsize = TPL_SDU_MAX;

    if ((res = tpl_recv(src, wdata, &wsize)) < 0)
        return res;
    if (wsize == 0 || wdata[0] != mydata->proto)
        return -1;
    if (size != NULL)
        *size = wsize - 1;
    if (data != NULL)
        memcpy(data, wdata + 1, wsize - 1);
    return res;
}


void app_neighbor_print(void)
{
    int i;

    TRACE_APP("N(x) = { ");
    for (i = 0; i < mydata->nneigh; i++)
        TRACE2_APP("%u ", mydata->neigh[i]);
    TRACE2_APP("}\n");
}

void app_neighbor_add(addr_t addr)
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
        } else {
            TRACE("NEIGHBOR %u ignored, max capacity reached\n", addr);
        }
    }
}

void app_neighbor_del(addr_t addr)
{
    uint8_t i;

    for (i = 0; i < mydata->nneigh; i++) {
        if (mydata->neigh[i] == addr) {
            memcpy(&mydata->neigh[i], &mydata->neigh[i+1],
                    mydata->nneigh - i);
            mydata->nneigh--;
            break;
        }
    }
}

int app_is_neighbor(addr_t addr)
{
    uint8_t i;

    for (i = 0; i < mydata->nneigh; i++) {
        if (mydata->neigh[i] == addr)
            break;
    }
    return (i < mydata->nneigh);
}

/* Application loop */
static void app_loop(void)
{
    switch (mydata->proto) {
    case APP_PROTO_DIS:
        dis_loop();
        if (mydata->dis.state == DIS_STATE_DONE) {
            tpl_drop();
            if (mydata->nneigh > 0) {
                mydata->proto = APP_PROTO_SPT;
                spt_init();
            } else {
                mydata->proto = 0xFF; /* Nothing to do, just sleep */
                COLOR_APP(WHITE);
            }
        }
        break;
    case APP_PROTO_SPT:
        spt_loop();
        if (mydata->spt.state == SPT_STATE_DONE) {
            mydata->gid = (uint8_t)mydata->spt.root;
            mydata->proto = APP_PROTO_WSC;
            mydata->nneigh = mydata->spt.nchild;
            memcpy(mydata->neigh, mydata->spt.child,
                   mydata->spt.nchild * sizeof(addr_t));
            if (mydata->uid != mydata->gid) {
                mydata->neigh[mydata->nneigh] = mydata->spt.parent;
                mydata->nneigh++;
            }
            hnt_init();
        }
        break;
    case APP_PROTO_WSC:
        hnt_loop();
        break;
    default:
        break;
    }
}

static void app_timeout(addr_t dst, uint8_t *data, uint8_t siz)
{
    TRACE_APP("TIMEOUT waiting ACK from %u\n", dst);
    ASSERT(siz > 0);

    if (data[0] != mydata->proto) {
        TRACE_APP("... inactive subsystem %u, current active is %u, ignore!\n",
                  data[0], mydata->proto);
        return;
    }
    /* SPT is the only protocol with a timeout handler */
    if (mydata->proto == APP_PROTO_SPT)
        spt_timeout(dst, data + 1, siz - 1);
}

/* Application initialization */
static void app_setup(void)
{
    set_motors(0, 0);
    set_color(WHITE);

    /* Application data erase */
    memset(mydata, 0, sizeof(*mydata));

    /* Communication channel init */
    tpl_init(0, app_timeout);

    mydata->uid = kilo_uid;

#ifndef SKIP_ELECTION
    dis_init();
    mydata->gid = TPL_BROADCAST_ADDR;
    mydata->proto = APP_PROTO_DIS;
#else
    hnt_init();
    mydata->gid = APP_DEFAULT_GID;
    mydata->proto = APP_PROTO_WSC;
    if (mydata->uid == mydata->gid) {
        /*
         * Hack to let the hnt work as expected.
         * The kilobot with uid equal to DEFAULT_WSC_GID will be both the
         * first witch and the target.
         */
        mydata->nneigh = 1;
        mydata->neigh[0] = mydata->uid;
        mydata->hnt.dist = 0;
    }
    mydata->hnt.target = APP_DEFAULT_GID;
    mydata->hnt.state  = WSC_STATE_ACTIVE;
#endif
}

int main(void)
{
    srand(time(NULL));
    kilo_init();
    kilo_start(app_setup, app_loop);
    return 0;
}
