/*
 * Transport Protocol Layer.
 *
 * Implementation of a reliable communication channel above the unreliable
 * infrared kilobot physical layer.
 *
 * Services:
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
 *   [ src(1) | dst(1) | ctrl(1) | SDU(6) ]
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
 * the communication channel packet header.
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
#define TPL_RETRY_MAX           16
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

/**
 * Bitmap data used to keep track of a binary sequence number. @{
 */
/** Number of bytes required by the bitmap */
#define TPL_BITMAP_BYTES       ((TPL_ADDR_MAX >> 3) + 1)
/** Bitmap type alias */
typedef uint8_t bitmap_t[TPL_BITMAP_BYTES];
/** Get a bit value (returns 0 or 1). */
#define TPL_BITMAP_GET(map, bit) \
        ((map[bit >> 3] & 1 << (bit & 0x7)) != 0)
/** Set a bit value (0 or 1). */
#define TPL_BITMAP_SET(map, bit, val) \
        ((val != 0) ? \
        (map[bit >> 3] |= (1 << (bit & 0x07))) : \
        (map[bit >> 3] &= ~(1 << (bit & 0x07))))
/** Invert a bit value */
#define TPL_BITMAP_TOGGLE(map, bit) \
        (map[bit >> 3] ^= 1 << (bit & 0x07))
/** @} */

/* Invoked if no ack is received after all the retries */
typedef void (* timeout_fun)(addr_t dst, uint8_t *data, uint8_t siz);

/**
 * Buffers raw back-buffers sizes @{
 *
 * The following values were empirically deduced using Kilombo simulator with
 * success rate = 0.9 and 10 kilobots.
 * For different parameters fine tune by first run the application with
 * VERBOSE_BUF compilation flag.
 */
/** Ack raw buffer size */
#define TPL_ACK_BUF_SIZE    4
/** RX raw buffer size */
#define TPL_RX_BUF_SIZE     32
/** TX raw buffer size */
#define TPL_TX_BUF_SIZE     64

/** Transport Protocol Layer context */
struct tpl_ctx {
    uint8_t         state;      /**< Context state */
    uint8_t         count;      /**< Number of TX retries for acked PDUs */
    uint8_t         flags;      /**< Behavior flags */
    uint8_t         dist;       /**< Last RX message distance */
    uint32_t        sent;       /**< Last TX message time (kilo_ticks) */
    message_t       msg;        /**< Last TX message */
    message_t       msg2;       /**< Spare message used for emergency acks */
    timeout_fun     timeout_cb; /**< Callback for confirmed unicast failures */
    bitmap_t        rx_map;     /**< RX sequence bitmap */
    bitmap_t        tx_map;     /**< TX sequence bitmap */
    buf_t           ack_buf;    /**< TX acks buffer */
    buf_t           rx_buf;     /**< RX data buffer */
    buf_t           tx_buf;     /**< TX data buffer */
    uint8_t         ack_raw[TPL_ACK_BUF_SIZE];  /**< ACK back buffer */
    uint8_t         rx_raw[TPL_RX_BUF_SIZE];    /**< RX back buffer */
    uint8_t         tx_raw[TPL_TX_BUF_SIZE];    /**< TX back buffer */
#ifdef VERBOSE_BUF
    uint8_t         rx_max;
    uint8_t         tx_max;
    uint8_t         ack_max;
#endif
};

/** Transport protocol context type alias */
typedef struct tpl_ctx tpl_ctx_t;

/**
 * Transport protocol initialization.
 *
 * @param flags         Context flags.
 * @param timeout_cb    Callback for confirmed unicast failures.
 */
void tpl_init(uint8_t flags, timeout_fun timeout_cb);

/**
 * Pending messages discard.
 *
 * Both received and transmitted messaged that are waiting to be effectively
 * processed by the upper and lower layer, respectively, are discarded.
 * The ACK buffer is cleared as well.
 */
void tpl_drop(void);

/**
 * Send application information.
 *
 * All or nothing send function.
 *
 * @param dst   Destination peer address.
 * @param data  Data raw buffer pointer.
 * @param size  Data raw buffer size.
 * @return      0 on success, -1 on failure.
 */
int tpl_send(addr_t dst, uint8_t *data, uint8_t size);

/**
 * Receive application information.
 *
 * The 'size' parameter is used for both input and output.
 *
 * @param src   Source peer address.
 * @param data  Data raw buffer pointer.
 * @param size  Data raw buffer size.
 *              On success this is changed to the number of bytes received.
 * @return      0 on success, -1 on failure.
 */
int tpl_recv(addr_t *src, uint8_t *data, uint8_t *size);

#endif /* TPL_H_ */
