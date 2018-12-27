/*
 * The following implements a reliable communication channel above
 * the rough unreliable infrared kilobot channel.
 *
 * When a destination address is specified to be a UNICAST address
 * then every outgoing packet is acknownledged unless the context
 * flag CHAN_DATAGRAM is not set within the context.
 *
 * Messages that are not destinated to us or that are not BROADCAST
 * are silently discarded unless the CHAN_PROMISC flag is not set.
 *
 * Communication channel PDU structure:
 *
 * [ src(1) | dst(1) | flg(1) | SDU(6) ]
 *  SDU: service data unit (information)
 */

#ifndef CHAN_H_
#define CHAN_H_

#include "misc.h"
#include "buf.h"

/*
 * Even though kilo_uid type is instanced as a uint16_t we set the address
 * type to be an uint8_t to save some space in the contexts data and in
 * the communication channel fragment header.
 * 256 addresses are more than sufficient for practical applications.
 */
typedef uint8_t addr_t;

#define BROADCAST_ADDR  0xFF

#define PDU_MAX         9
#define HEAD_SIZE       (1+2*sizeof(addr_t))
#define SDU_MAX         (PDU_MAX - HEAD_SIZE)

#define RETRY_MAX       10
#define ACK_TIMEOUT     (2*KILO_TICKS_PER_SEC)

#define CHAN_STATE_IDLE         0
#define CHAN_STATE_WAIT_TX      1
#define CHAN_STATE_WAIT_ACK     2

#define PDU_FLAG_CON            0x80
#define PDU_FLAG_ACK            0x40
#define PDU_FLAG_SEQ            0x20
#define PDU_LEN_MASK            0x07

/* TODO */
#define CHAN_FLAG_DATAGRAM      0x01
#define CHAN_FLAG_PROMISC       0x02

#define CHAN_ADDR_MAX           255
#define CHAN_BITMAP_BYTES       ((CHAN_ADDR_MAX >> 3) + 1)
typedef uint8_t bitmap_t[CHAN_BITMAP_BYTES];
#define CHAN_BITMAP_GET(map, bit) \
        ((map[bit >> 3] & 1 << (bit & 0x7)) != 0)
#define CHAN_BITMAP_SET(map, bit) \
        (map[bit >> 3] |= 1 << (bit & 0x07))
#define CHAN_BITMAP_TOGGLE(map, bit) \
        (map[bit >> 3] ^= 1 << (bit & 0x07))


/* Invoked if no ack is received after all the retries */
typedef void (* timeout_func)(addr_t dst, uint8_t *data, uint8_t siz);

struct chan_ctx {
    uint8_t         state;
    uint8_t         count;
    uint8_t         flags;
    uint8_t         dist;       /* Last message distance */
    uint32_t        sent;       /* sent time */
    message_t       msg;        /* last sent msg */
    message_t       msg2;
    timeout_func    timeout_cb;
    struct buf      rx_buf;
    struct buf      tx_buf;
    struct buf      ack_buf;
    bitmap_t        rx_map;
    bitmap_t        tx_map;
};

void chan_init(uint8_t flags, timeout_func timeout_cb);

void chan_flush(void);

int chan_send(addr_t dst, uint8_t *data, uint8_t size);

int chan_recv(addr_t *src, uint8_t *data, uint8_t *size);


#endif /* TRANSPORT_CHAN_H_ */
