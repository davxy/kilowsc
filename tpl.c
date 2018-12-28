#include "tpl.h"

#include "app.h"
#include <string.h>
#include <time.h>

#define mytpl  (&mydata->tpl)

#define PDU_FLAG_CON            0x80
#define PDU_FLAG_ACK            0x40
#define PDU_FLAG_SEQ            0x20
#define PDU_LEN_MASK            0x07



static int tpl_buf_write(buf_t *buf, addr_t addr,
                          uint8_t *data, uint8_t size)
{
    uint8_t raw[TPL_PDU_MAX];

    if (size > TPL_SDU_MAX)
        return -1;
    raw[0] = addr;
    raw[1] = size;
    memcpy(raw + 2, data, size);
    return buf_write(buf, raw, size + 2);
}

static int tpl_buf_read(buf_t *buf, addr_t *addr,
                         uint8_t *data, uint8_t *size)
{
    int res;
    uint8_t raw[TPL_PDU_MAX];
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

    TRACE_TPL("ACK SEND to %u\n", dst);
    if ((res = buf_write(&mytpl->ack_buf, &dst, 1)) < 0)
        TRACE_TPL("ACK SEND FAIL\n");
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

    /* Retry metplism */
    if (mytpl->state == TPL_STATE_WAIT_ACK &&
            kilo_ticks > mytpl->sent + (mytpl->count + 1) * TPL_ACK_TIMEOUT) {
        if (mytpl->count < TPL_RETRY_MAX) {
            mytpl->sent = kilo_ticks;
            mytpl->count++;
            msg = &mytpl->msg;
            TRACE_TPL("Retry (%u)\n", mytpl->count);
        } else {
            TRACE_TPL("Give up\n");
            msg = NULL;
            mytpl->state = TPL_STATE_IDLE;
            mytpl->count = 0;
            COLOR_TPL(WHITE);
            if (mytpl->timeout_cb != NULL) {
                mytpl->timeout_cb(mytpl->msg.data[1],
                                   mytpl->msg.data + TPL_PCI_SIZE,
                                   mytpl->msg.data[2] & PDU_LEN_MASK);
            }
        }
        return msg;
    }

    /* First check if there is any pending ACK to send */
    if (buf_read(&mytpl->ack_buf, &dst, 1) == 0) {
        flg |= PDU_FLAG_ACK;
        if (TPL_BITMAP_GET(mytpl->rx_map, dst) != 0)
            flg |= PDU_FLAG_SEQ;
        TPL_BITMAP_TOGGLE(mytpl->rx_map, dst);
        msg = &mytpl->msg2;
        msg->type = NORMAL;
        msg->data[0] = mydata->uid;
        msg->data[1] = dst;
        msg->data[2] = flg;
        msg->crc = message_crc(msg);
        TRACE_TPL("SEND: src=%u, dst=%u (CON=%d,ACK=%d,SEQ=%u)\n",
                   mydata->uid, dst, 0, 1,(flg & PDU_FLAG_SEQ) != 0);
        return msg;
    }

    /* Normal packet send */
    msg = &mytpl->msg;
    flg = TPL_SDU_MAX;
    if (mytpl->state != TPL_STATE_IDLE ||
        tpl_buf_read(&mytpl->tx_buf, &dst, msg->data + TPL_PCI_SIZE, &flg) < 0) {
        COLOR_TPL(WHITE);
        return NULL;
    }

    if (dst != TPL_BROADCAST_ADDR && (mytpl->flags & TPL_FLAG_DATAGRAM) == 0) {
        flg |= PDU_FLAG_CON;
        if (TPL_BITMAP_GET(mytpl->tx_map, dst) != 0)
            flg |= PDU_FLAG_SEQ;
        mytpl->state = TPL_STATE_WAIT_ACK;
    } else {
        mytpl->state = TPL_STATE_WAIT_TX;
    }
    mytpl->sent = kilo_ticks;
    mytpl->count = 0;

    msg->type = NORMAL;
    msg->data[0] = mydata->uid;
    msg->data[1] = dst;
    msg->data[2] = flg;
    msg->crc = message_crc(msg);

    TRACE_TPL("SEND: src=%u, dst=%u (CON=%d,ACK=%d,SEQ=%u,LEN=%u)\n",
            mydata->uid, dst, (flg & PDU_FLAG_CON) != 0, (flg & PDU_FLAG_ACK) != 0,
            (flg & PDU_FLAG_SEQ) != 0, (flg & PDU_LEN_MASK));
    COLOR_TPL(RED);

    return msg;
}

static void message_tx_success(void)
{
    if (mytpl->state == TPL_STATE_WAIT_TX)
        mytpl->state = TPL_STATE_IDLE;
}

static void message_rx(message_t *m, distance_measurement_t *d)
{
    addr_t dst, src;
    addr_t exp_addr;
    uint8_t flg, dist;
    uint8_t promisc = 0;
    uint8_t len;
    uint8_t data[TPL_PDU_MAX];

    src = m->data[0];
    dst = m->data[1];
    flg = m->data[2];

    if (dst != mydata->uid && dst != TPL_BROADCAST_ADDR) {
        if ((mytpl->flags & TPL_FLAG_PROMISC) != 0)
            promisc = 1;
        else
            return; /* not our business */
    }

    dist = estimate_distance(d);
    TRACE_TPL("RECV: src=%u, dst=%u (CON=%d,ACK=%d,SEQ=%u,LEN=%u,DIST=%u)\n",
            src, dst, (flg & PDU_FLAG_CON) != 0, (flg & PDU_FLAG_ACK) != 0,
            (flg & PDU_FLAG_SEQ) != 0, (flg & PDU_LEN_MASK), dist);

    /* Check if is an ACK */
    if ((flg & PDU_FLAG_ACK) != 0 && promisc == 0) {
        if (mytpl->state == TPL_STATE_WAIT_ACK) {
            /* Read expected address from last sent message */
            exp_addr = mytpl->msg.data[1];
            if (exp_addr == src) {
                if (((flg & PDU_FLAG_SEQ) != 0) != TPL_BITMAP_GET(mytpl->tx_map, src)) {
                    TRACE_TPL("IGNORE DUPLICATE ACK\n");
                    ASSERT(0);  /* Should never happen */
                    return;     /* Wrong ack no */
                }
                TPL_BITMAP_TOGGLE(mytpl->tx_map, src);
                mytpl->state = TPL_STATE_IDLE; /* Ack received */
                COLOR_TPL(WHITE);
            }
        }
        return;
    }

    if ((flg & PDU_FLAG_CON) != 0 && promisc == 0) {
        if (((flg & PDU_FLAG_SEQ) != 0) != TPL_BITMAP_GET(mytpl->rx_map, src)) {
            TRACE_TPL(">>> IGNORE DUPLICATE INFO (send ack)\n");
            /* This tplge the ack number to the correct one */
            TPL_BITMAP_TOGGLE(mytpl->rx_map, src);
            ack_send(src);
            return;
        }
    }

    /* Information fetch */
    len = flg & PDU_LEN_MASK;
    if (len > TPL_SDU_MAX) {
        TRACE_TPL("Ignore msg with bad len field\n");
        return;
    }
    data[0] = dist;
    memcpy(data + 1, m->data + TPL_PCI_SIZE, len);
    if (tpl_buf_write(&mytpl->rx_buf, src, data, len + 1) != 0) {
        TRACE_TPL("Definitely lost rx info (hope for retry)\n");
        return; /* Drop for exhausted capacity */
    }

    /* Check if requires confirmation */
    if ((flg & PDU_FLAG_CON) != 0 && promisc == 0)
        ack_send(src);
}



int tpl_send(addr_t dst, uint8_t *data, uint8_t size)
{
    int res;
    uint8_t rdata[TPL_PDU_MAX] = {0};

    if (data == NULL && size > 0)
        data = rdata;
    if ((res = tpl_buf_write(&mytpl->tx_buf, dst, data, size)) < 0)
        TRACE_TPL("TX FAIL\n");
    return res;
}

int tpl_recv(addr_t *src, uint8_t *data, uint8_t *size)
{
    int res;
    uint8_t rsize = TPL_PDU_MAX;
    uint8_t rdata[TPL_PDU_MAX];

    if ((res = tpl_buf_read(&mytpl->rx_buf, src, rdata, &rsize)) == 0) {
        rsize--;
        if (size != NULL) {
            if (*size < rsize)
                rsize = *size;
            *size = rsize;
        }
        if (data != NULL)
            memcpy(data, rdata + 1, rsize);
        mytpl->dist = rdata[0];
    }
    return res;
}

void tpl_flush(void)
{
    mytpl->state = TPL_STATE_IDLE;
    buf_flush(&mytpl->ack_buf);
    buf_flush(&mytpl->rx_buf);
    buf_flush(&mytpl->tx_buf);
    memset(mytpl->rx_map, 0, sizeof(mytpl->rx_map));
    memset(mytpl->tx_map, 0, sizeof(mytpl->tx_map));
}

void tpl_init(uint8_t flags, timeout_fun timeout_cb)
{
    memset(mytpl, 0, sizeof(*mytpl));
    mytpl->flags = flags;
    mytpl->timeout_cb = timeout_cb;
    kilo_message_tx = message_tx;
    kilo_message_rx = message_rx;
    kilo_message_tx_success = message_tx_success;
}
