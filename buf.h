/**
 * FIFO Ring Buffer.
 */

#ifndef BUF_H_
#define BUF_H_

#include <stdint.h>

/** Buffer max size. */
#define BUF_MAX_SIZE    64

/** Buffer structure. */
struct buf {
    uint8_t     nrp;                    /**< Next read position. */
    uint8_t     nwp;                    /**< Next write position. */
    uint8_t     dat[BUF_MAX_SIZE + 1];  /**< Data raw buffer. */
};

/** Buffer type alias. */
typedef struct buf buf_t;

/** Number of available (free) bytes. */
#define buf_avail(b) \
    (BUF_MAX_SIZE - buf_size(b))

/** Number of used bytes. */
#define buf_size(b) \
    (((b)->nwp >= (b)->nrp) ? \
    ((b)->nwp - (b)->nrp) : \
    ((BUF_MAX_SIZE + 1) - ((b)->nrp - (b)->nwp)))

/** Discard buffer content */
#define buf_flush(b) \
    ((b)->nrp = (b)->nwp = 0)

/**
 * Insert data to the buffer tail.
 *
 * @param buf   Buffer structure.
 * @param dat   Data source raw buffer.
 * @param siz   Number of bytes to insert.
 * @return      0 on success, -1 on error.
 *              If the buffer doesn't have at least siz free bytes then
 *              returns -1 and 0 bytes are inserted (all or nothing).
 */
int buf_write(buf_t *buf, uint8_t *dat, uint8_t siz);

/**
 * Extract data from the buffer front.
 *
 * @param buf   Buffer structure.
 * @param dat   Data destination raw buffer.
 * @param siz   Number of bytes to extract.
 * @return      0 on success, -1 on error.
 *              If the buffer doesn't have at least siz busy bytes then
 *              returns -1 and 0 bytes are extracted (all or nothing).
 */
int buf_read(buf_t *buf, uint8_t *dat, uint8_t siz);

#endif /* BUF_H_ */
