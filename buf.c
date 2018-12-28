#include "buf.h"
#include "misc.h"


int buf_write(buf_t *buf, uint8_t *dat, uint8_t siz)
{
    if (buf_avail(buf) < siz)
        return -1;
    if (buf->nwp >= buf->nrp) {
        uint8_t n;

        n = MIN(BUF_MAX_SIZE + 1 - buf->nwp, siz);
        if (dat != NULL)
            memcpy(buf->dat + buf->nwp, dat, n);
        else
            memset(buf->dat + buf->nwp, 0, n);
        buf->nwp += n;
        siz -= n;
        dat += n;
        if (buf->nwp == BUF_MAX_SIZE + 1)
            buf->nwp = 0;
    }
    if (siz > 0) {
        if (dat != NULL)
            memcpy(buf->dat + buf->nwp, dat, siz);
        else
            memset(buf->dat + buf->nwp, 0, siz);
        buf->nwp += siz;
    }
    return 0;
}

int buf_read(buf_t *buf, uint8_t *dat, uint8_t siz)
{
    if (buf_size(buf) < siz)
        return -1;
    if (buf->nrp > buf->nwp) {
        uint8_t n;

        n = MIN(BUF_MAX_SIZE + 1 - buf->nrp, siz);
        if (dat != NULL)
            memcpy(dat, buf->dat + buf->nrp, n);
        buf->nrp += n;
        siz -= n;
        dat += n;
        if (buf->nrp == BUF_MAX_SIZE + 1)
            buf->nrp = 0;
    }
    if (siz > 0) {
        if (dat != NULL)
            memcpy(dat, buf->dat + buf->nrp, siz);
        buf->nrp += siz;
    }
    return 0;
}
