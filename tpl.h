/*
 * Transport Protocol Layer
 *
 * Implementation of a reliable communication channel above the unreliable
 * infrared kilobot physical layer.
 *
 * Serveces:
 * - Unicast and Broadcast.
 * - Promiscuous mode.
 * - Datagram service.
 * - Reliable communication channel.
 *
 * When a destination address is specified to be a UNICAST address then every
 * outgoing packet is acknownledged unless the context flag DATAGRAM is not set.
 *
 * Unicast messages that are not destinated to a given peer and that are not
 * BROADCAST are silently discarded unless the PROMISC flag is not set.
 *
 * Only one outstanding confirmed packet is allowed. Meaning that if we are
 * waiting for a packet ACK then the other sent messages are internally enqueued.
 *
 * TPDU structure:
 *
 *   [ src(1) | dst(1) | flg(1) | SDU(6) ]
 *
 *   SDU: service data unit (information)
 */

#ifndef TPL_H_
#define TPL_H_

#include "misc.h"
#include "buf.h"

/*
 * Even though kilo_uid type is instanced as a uint16_t we set the address
 * type to be an uint8_t to save some space in the contexts data and in
 * the communication channel fragment header.
 * 256 addresses are more than sufficient for practical applications.
 */
typedef uint8_t addr_t;

/**
 * Protocol parameters @{
 */
/** Broadcast address */
#define TPL_BROADCAST_ADDR      ((addr_t)-1)
/** Maximum valid address value */
#define TPL_ADDR_MAX            TPL_BROADCAST_ADDR
/** Maximum Protocol Data Unit size */
#define TPL_PDU_MAX             9
/** Protocol Control Information (Header) size */
#define TPL_PCI_SIZE            (1+2*sizeof(addr_t))
/** Maximum Service Data Unit (PDU payload information) size. */
#define TPL_SDU_MAX             (TPL_PDU_MAX - TPL_PCI_SIZE)
/** Maximum number of retries for acknowledged packets */
#define TPL_RETRY_MAX           10
/** Number of seconds between retries (linear strategy) */
#define TPL_ACK_TIMEOUT         (3*KILO_TICKS_PER_SEC)
/** @} */

/**
 * Context states @{
 */
/** Nothing to do */
#define TPL_STATE_IDLE          0
/** Waiting for local message tx callback */
#define TPL_STATE_WAIT_TX       1
/** Waiting for remote acknowledge */
#define TPL_STATE_WAIT_ACK      2
/** @} */

/**
 * Context flags @{
 */
/* Send not acknowledged unicast packets */
#define TPL_FLAG_DATAGRAM       0x01
/* Set the interface in promiscuous mode */
#define TPL_FLAG_PROMISC        0x02
/** @} */


#define TPL_BITMAP_BYTES       ((TPL_ADDR_MAX >> 3) + 1)
typedef uint8_t bitmap_t[TPL_BITMAP_BYTES];

#define TPL_BITMAP_GET(map, bit) \
        ((map[bit >> 3] & 1 << (bit & 0x7)) != 0)
#define TPL_BITMAP_SET(map, bit) \
        (map[bit >> 3] |= 1 << (bit & 0x07))
#define TPL_BITMAP_TOGGLE(map, bit) \
        (map[bit >> 3] ^= 1 << (bit & 0x07))


/* Invoked if no ack is received after all the retries */
typedef void (* timeout_fun)(addr_t dst, uint8_t *data, uint8_t siz);

struct tpl_ctx {
    uint8_t         state;      /** Context state */
    uint8_t         count;      /** Number of TX retries for acked PDUs */
    uint8_t         flags;      /** Behavior flags */
    uint8_t         dist;       /** Last RX message distance */
    uint32_t        sent;       /** Last TX message time (kilo_ticks) */
    message_t       msg;        /** Last TX message */
    message_t       msg2;       /** Spare message used for emergency acks */
    timeout_fun     timeout_cb;
    buf_t           ack_buf;
    buf_t           rx_buf;
    buf_t           tx_buf;
    bitmap_t        rx_map;
    bitmap_t        tx_map;
};

typedef struct tpl_ctx tpl_ctx_t;

void tpl_init(uint8_t flags, timeout_fun timeout_cb);

void tpl_flush(void);

int tpl_send(addr_t dst, uint8_t *data, uint8_t size);

int tpl_recv(addr_t *src, uint8_t *data, uint8_t *size);


#endif /* TPL_H_ */
