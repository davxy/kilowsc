/**
 * FIFO Ring Buffer.
 */

#ifndef BUF_H_
#define BUF_H_

#include <stdint.h>



/** Buffer structure. */
struct buf {
    uint8_t     nrp;    /**< Next read position. */
    uint8_t     nwp;    /**< Next write position. */
    uint8_t     siz;    /**< Raw back buffer size. */
    uint8_t    *raw;    /**< Raw back buffer pointer. */
};

/** Buffer type alias. */
typedef struct buf buf_t;

/** Buffer max payload size. */
#define buf_max_size(b) ((b)->siz - 1)

/** Number of available (free) bytes. */
#define buf_avail(b) \
    (buf_max_size(b) - buf_size(b))

/** Number of used bytes. */
#define buf_size(b) \
    (((b)->nwp >= (b)->nrp) ? \
    ((b)->nwp - (b)->nrp) : \
    ((buf_max_size(b) + 1) - ((b)->nrp - (b)->nwp)))

/** Discard buffer content */
#define buf_flush(b) \
    ((b)->nrp = (b)->nwp = 0)

/**
 * Initialize the ring buffer with a given raw back-buffer.
 *
 * @param buf       Buffer structure.
 * @param raw_buf   Raw back buffer pointer.
 * @param raw_siz   Raw back buffer size.
 */
#define buf_init(buf, raw_buf, raw_siz) do { \
    (buf)->raw = raw_buf; \
    (buf)->siz = raw_siz; \
    (buf)->nrp = 0; \
    (buf)->nwp = 0; \
} while(0)

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
