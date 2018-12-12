#include "chan.h"
#include "app.h"
#include <string.h>
#include <time.h>

#define mychan  (&mydata->chan)


static int chan_buf_write(struct buf *buf, addr_t addr,
                          uint8_t *data, uint8_t size)
{
    uint8_t raw[PDU_MAX];

    if (size > SDU_MAX)
        return -1;
    raw[0] = addr;
    raw[1] = size;
    memcpy(raw + 2, data, size);
    return buf_write(buf, raw, size + 2);
}

static int chan_buf_read(struct buf *buf, addr_t *addr,
                         uint8_t *data, uint8_t *size)
{
    int res;
    uint8_t raw[PDU_MAX];
    uint8_t pkt_size;

    if ((res = buf_read(buf, raw, 2)) < 0)
        return res;
    if (addr != NULL)
        *addr = raw[0];
    pkt_size = raw[1];
    if (size != NULL) {
        if (*size < pkt_size)
            pkt_size = *size;
        *size = pkt_size;
    }
    return buf_read(buf, data, pkt_size);
}


static int ack_send(addr_t dst)
{
    int res;

#ifdef VERBOSE_CHAN
    TRACE("ACK SEND to %u\n", dst);
#endif
    if ((res = buf_write(&mychan->ack_buf, &dst, 1)) < 0)
        TRACE("ACK FAIL\n");
    return res;
}


int chan_send(addr_t dst, uint8_t *data, uint8_t size)
{
    int res;

    if ((res = chan_buf_write(&mychan->tx_buf, dst, data, size)) < 0)
        TRACE("TX FAIL\n");
    return res;
}

int chan_recv(addr_t *src, uint8_t *data, uint8_t *size)
{
    int res;
    uint8_t rsize = PDU_MAX;
    uint8_t rdata[PDU_MAX];

    if ((res = chan_buf_read(&mychan->rx_buf, src, rdata, &rsize)) == 0) {
        rsize--;
        if (size != NULL) {
            if (*size < rsize)
                rsize = *size;
            *size = rsize;
        }
        memcpy(data, rdata + 1, rsize);
        mychan->dist = rdata[0];
    }
    return res;
}


static message_t *message_tx(void)
{
    message_t *msg;
    uint8_t flg = 0;
    addr_t dst;

    /* Retry mechanism */
    if (mychan->state == CHAN_STATE_WAIT_ACK &&
            kilo_ticks > mychan->sent + (mychan->count + 1) * ACK_TIMEOUT) {
        if (mychan->count < RETRY_MAX) {
            mychan->sent = kilo_ticks;
            mychan->count++;
            msg = &mychan->msg;
            TRACE("Retry (%u)\n", mychan->count);
        } else {
            TRACE("Give up\n");

#ifdef VERBOSE_CHAN
            TRACE("Give up\n");
#endif
            msg = NULL;
            mychan->state = CHAN_STATE_IDLE;
            mychan->count = 0;
#ifdef VISUAL_CHAN
            SET_IDLE();
#endif
            if (mychan->timeout_cb != NULL) {
                mychan->timeout_cb(mychan->msg.data[1],
                                   mychan->msg.data + HEAD_SIZE,
                                   mychan->msg.data[2] & PDU_LEN_MASK);
            }
        }
        return msg;
    }

    if (buf_read(&mychan->ack_buf, &dst, 1) == 0) {
        flg |= PDU_FLAG_ACK;
        if (mychan->rx_seq[dst] != 0)
            flg |= PDU_FLAG_SEQ;
        mychan->rx_seq[dst] = 1 - mychan->rx_seq[dst];
        msg = &mychan->msg2;
        msg->type = NORMAL;
        msg->data[0] = kilo_uid;
        msg->data[1] = dst;
        msg->data[2] = flg;
        msg->crc = message_crc(msg);
#ifdef VERBOSE_CHAN
        TRACE("SEND: src=%u, dst=%u (CON=%d,ACK=%d,SEQ=%u)\n",
            kilo_uid, dst, 0, 1,(flg & PDU_FLAG_SEQ) != 0);
#endif
        return msg;
    }

    msg = &mychan->msg;
    flg = SDU_MAX;
    if (mychan->state != CHAN_STATE_IDLE ||
        chan_buf_read(&mychan->tx_buf, &dst, msg->data + HEAD_SIZE, &flg) < 0) {
#ifdef VISUAL_CHAN
        SET_IDLE();
#endif
        return NULL;
    }

    if (dst != BROADCAST_ADDR) {
        flg |= PDU_FLAG_CON;
        if (mychan->tx_seq[dst] != 0)
            flg |= PDU_FLAG_SEQ;
        mychan->state = CHAN_STATE_WAIT_ACK;
    } else {
        mychan->state = CHAN_STATE_WAIT_TX;
    }
    mychan->sent = kilo_ticks;
    mychan->count = 0;

    msg->type = NORMAL;
    msg->data[0] = kilo_uid;
    msg->data[1] = dst;
    msg->data[2] = flg;
    msg->crc = message_crc(msg);

#ifdef VERBOSE_CHAN
    TRACE("SEND: src=%u, dst=%u (CON=%d,ACK=%d,SEQ=%u,LEN=%u)\n",
            kilo_uid, dst,
            (flg & PDU_FLAG_CON) != 0,
            (flg & PDU_FLAG_ACK) != 0,
            (flg & PDU_FLAG_SEQ) != 0,
            (flg & PDU_LEN_MASK));
#endif
#ifdef VISUAL_CHAN
    SET_ACTIVE();
#endif

    return msg;
}

static void message_tx_success(void)
{
    if (mychan->state == CHAN_STATE_WAIT_TX)
        mychan->state = CHAN_STATE_IDLE;
}

static void message_rx(message_t *m, distance_measurement_t *d)
{
    addr_t dst, src;
    uint8_t flg, dist;

    src = m->data[0];
    dst = m->data[1];
    flg = m->data[2];

    if (dst != kilo_uid && dst != BROADCAST_ADDR)
        return; /* not our business */

    dist = estimate_distance(d);
#ifdef VERBOSE_CHAN
    TRACE("RECV: src=%u, dst=%u (CON=%d,ACK=%d,DIST=%u)\n",
            src, dst,
            (flg & PDU_FLAG_CON) != 0,
            (flg & PDU_FLAG_ACK) != 0,
            dist);
#endif

    /* Check if is an ACK */
    if ((flg & PDU_FLAG_ACK) != 0) {
        if (mychan->state == CHAN_STATE_WAIT_ACK) {
            addr_t exp_addr; /* Expected ack address */

            exp_addr = mychan->msg.data[1]; /* read from last sent message */
            if (exp_addr == src) {
                if (((flg & PDU_FLAG_SEQ) != 0) != mychan->tx_seq[src]) {
                    TRACE("IGNORE DUPLICATE ACK\n");
                    ASSERT(0);  /* Should never happen */
                    return;     /* Wrong ack no */
                }
                mychan->tx_seq[src] = 1 ^ mychan->tx_seq[src];
                mychan->state = CHAN_STATE_IDLE; /* Ack received */
#ifdef VISUAL_CHAN
                SET_IDLE();
#endif
            }
        }
        return;
    }

    if ((flg & PDU_FLAG_CON) != 0) {
        if (((flg & PDU_FLAG_SEQ) != 0) != mychan->rx_seq[src]) {
            TRACE(">>> IGNORE DUPLICATE INFO (send ack)\n");
            /* This change the ack number to the correct one */
            mychan->rx_seq[src] = 1 - mychan->rx_seq[src];
            ack_send(src);
            return;
        }
    }

    /* Information fetch */
    /* Use flg position to store dist */
    uint8_t data[PDU_MAX];
    uint8_t len = flg & PDU_LEN_MASK;

    if (len > SDU_MAX) {
        TRACE(">>> IGNORE MSG WITH BAD LEN\n");
        return;
    }
    data[0] = dist;
    memcpy(data + 1, m->data + HEAD_SIZE, len);
    if (chan_buf_write(&mychan->rx_buf, src, data, len + 1) != 0) {
        TRACE("LOST INFO (hope for retry)\n");
        return; /* Drop for exhausted capacity */
    }

    /* Check if requires confirmation */
    if ((flg & PDU_FLAG_CON) != 0) {
        ack_send(src);
    }
}

void chan_flush(void)
{
    mychan->state = CHAN_STATE_IDLE;
    buf_flush(&mychan->ack_buf);
    buf_flush(&mychan->rx_buf);
    buf_flush(&mychan->tx_buf);
}

void chan_init(uint8_t flags, timeout_func timeout_cb)
{
    memset(mychan, 0, sizeof(*mychan));
    mychan->flags = flags;
    mychan->timeout_cb = timeout_cb;
    kilo_message_tx = message_tx;
    kilo_message_rx = message_rx;
    kilo_message_tx_success = message_tx_success;
}
