#include "chan.h"
#include "app.h"
#include <string.h>
#include <time.h>

#define mychan  (&mydata->chan)

#define PDU_FLAG_CON            0x80
#define PDU_FLAG_ACK            0x40
#define PDU_FLAG_SEQ            0x20
#define PDU_LEN_MASK            0x07


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

    TRACE_CHAN("ACK SEND to %u\n", dst);
    if ((res = buf_write(&mychan->ack_buf, &dst, 1)) < 0)
        TRACE_CHAN("ACK SEND FAIL\n");
    return res;
}


int chan_send(addr_t dst, uint8_t *data, uint8_t size)
{
    int res;

    if ((res = chan_buf_write(&mychan->tx_buf, dst, data, size)) < 0)
        TRACE_CHAN("TX FAIL\n");
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

/*
 * Callback invoked by the kilobot communication sub-system to send a packet
 * Priority:
 * 1. Packet Retry
 * 2. Packet ACK
 * 3. Normal Packet
 */
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
            TRACE_CHAN("Retry (%u)\n", mychan->count);
        } else {
            TRACE_CHAN("Give up\n");
            msg = NULL;
            mychan->state = CHAN_STATE_IDLE;
            mychan->count = 0;
            COLOR_CHAN(WHITE);
            if (mychan->timeout_cb != NULL) {
                mychan->timeout_cb(mychan->msg.data[1],
                                   mychan->msg.data + HEAD_SIZE,
                                   mychan->msg.data[2] & PDU_LEN_MASK);
            }
        }
        return msg;
    }

    /* First check if there is any pending ACK to send */
    if (buf_read(&mychan->ack_buf, &dst, 1) == 0) {
        flg |= PDU_FLAG_ACK;
        if (CHAN_BITMAP_GET(mychan->rx_map, dst) != 0)
            flg |= PDU_FLAG_SEQ;
        CHAN_BITMAP_TOGGLE(mychan->rx_map, dst);
        msg = &mychan->msg2;
        msg->type = NORMAL;
        msg->data[0] = kilo_uid;
        msg->data[1] = dst;
        msg->data[2] = flg;
        msg->crc = message_crc(msg);
        TRACE_CHAN("SEND: src=%u, dst=%u (CON=%d,ACK=%d,SEQ=%u)\n",
                   kilo_uid, dst, 0, 1,(flg & PDU_FLAG_SEQ) != 0);
        return msg;
    }

    /* Normal packet send */
    msg = &mychan->msg;
    flg = SDU_MAX;
    if (mychan->state != CHAN_STATE_IDLE ||
        chan_buf_read(&mychan->tx_buf, &dst, msg->data + HEAD_SIZE, &flg) < 0) {
        COLOR_CHAN(WHITE);
        return NULL;
    }

    if (dst != BROADCAST_ADDR && (mychan->flags & CHAN_FLAG_DATAGRAM) == 0) {
        flg |= PDU_FLAG_CON;
        if (CHAN_BITMAP_GET(mychan->tx_map, dst) != 0)
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

    TRACE_CHAN("SEND: src=%u, dst=%u (CON=%d,ACK=%d,SEQ=%u,LEN=%u)\n",
            kilo_uid, dst, (flg & PDU_FLAG_CON) != 0, (flg & PDU_FLAG_ACK) != 0,
            (flg & PDU_FLAG_SEQ) != 0, (flg & PDU_LEN_MASK));
    COLOR_CHAN(RED);

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
    addr_t exp_addr;
    uint8_t flg, dist;
    uint8_t promisc = 0;
    uint8_t len;
    uint8_t data[PDU_MAX];

    src = m->data[0];
    dst = m->data[1];
    flg = m->data[2];

    if (dst != kilo_uid && dst != BROADCAST_ADDR) {
        if ((mychan->flags & CHAN_FLAG_PROMISC) != 0)
            promisc = 1;
        else
            return; /* not our business */
    }

    dist = estimate_distance(d);
    TRACE_CHAN("RECV: src=%u, dst=%u (CON=%d,ACK=%d,SEQ=%u,LEN=%u,DIST=%u)\n",
            src, dst, (flg & PDU_FLAG_CON) != 0, (flg & PDU_FLAG_ACK) != 0,
            (flg & PDU_FLAG_SEQ) != 0, (flg & PDU_LEN_MASK), dist);

    /* Check if is an ACK */
    if ((flg & PDU_FLAG_ACK) != 0 && promisc == 0) {
        if (mychan->state == CHAN_STATE_WAIT_ACK) {
            /* Read expected address from last sent message */
            exp_addr = mychan->msg.data[1];
            if (exp_addr == src) {
                if (((flg & PDU_FLAG_SEQ) != 0) != CHAN_BITMAP_GET(mychan->tx_map, src)) {
                    TRACE_CHAN("IGNORE DUPLICATE ACK\n");
                    ASSERT(0);  /* Should never happen */
                    return;     /* Wrong ack no */
                }
                CHAN_BITMAP_TOGGLE(mychan->tx_map, src);
                mychan->state = CHAN_STATE_IDLE; /* Ack received */
                COLOR_CHAN(WHITE);
            }
        }
        return;
    }

    if ((flg & PDU_FLAG_CON) != 0 && promisc == 0) {
        if (((flg & PDU_FLAG_SEQ) != 0) != CHAN_BITMAP_GET(mychan->rx_map, src)) {
            TRACE_CHAN(">>> IGNORE DUPLICATE INFO (send ack)\n");
            /* This change the ack number to the correct one */
            CHAN_BITMAP_TOGGLE(mychan->rx_map, src);
            ack_send(src);
            return;
        }
    }

    /* Information fetch */
    len = flg & PDU_LEN_MASK;
    if (len > SDU_MAX) {
        TRACE_CHAN("Ignore msg with bad len field\n");
        return;
    }
    data[0] = dist;
    memcpy(data + 1, m->data + HEAD_SIZE, len);
    if (chan_buf_write(&mychan->rx_buf, src, data, len + 1) != 0) {
        TRACE_CHAN("Definitely lost rx info (hope for retry)\n");
        return; /* Drop for exhausted capacity */
    }

    /* Check if requires confirmation */
    if ((flg & PDU_FLAG_CON) != 0 && promisc == 0)
        ack_send(src);
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
