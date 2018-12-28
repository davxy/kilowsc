#ifndef BUF_H_
#define BUF_H_

#include <stdint.h>

#define BUF_MAX_SIZE    128

struct buf {
    uint8_t     nrp;
    uint8_t     nwp;
    uint8_t     dat[BUF_MAX_SIZE + 1];
};

typedef struct buf buf_t;


#define buf_avail(b) \
    (BUF_MAX_SIZE - buf_size(b))

#define buf_size(b) \
    (((b)->nwp >= (b)->nrp) ? \
    ((b)->nwp - (b)->nrp) : \
    ((BUF_MAX_SIZE + 1) - ((b)->nrp - (b)->nwp)))

#define buf_flush(b) \
    ((b)->nrp = (b)->nwp = 0)

int buf_write(buf_t *buf, uint8_t *dat, uint8_t siz);

int buf_read(buf_t *buf, uint8_t *dat, uint8_t siz);

#endif /* BUF_H_ */
