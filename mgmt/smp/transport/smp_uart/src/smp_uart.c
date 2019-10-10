/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <inttypes.h>
#include <assert.h>

#include "os/mynewt.h"
#include <bsp/bsp.h>

#include <mgmt/mgmt.h>
#include <mynewt_smp/smp.h>
#include <uart/uart.h>

#include <crc/crc16.h>
#include <base64/base64.h>

/**
 * \addtogroup SMP SMP
 * @{
 */

/* NLIP packets sent over serial are fragmented into frames of 127 bytes or
 * fewer. This 127-byte maximum applies to the entire frame, including header,
 * CRC, and terminating newline.
 */
#define MGMT_NLIP_MAX_FRAME     127

#define SHELL_NLIP_PKT          0x0609
#define SHELL_NLIP_DATA         0x0414

#define NUS_EV_TO_STATE(ptr)                                            \
    (struct smp_uart_state *)((uint8_t *)ptr -                         \
      (int)&(((struct smp_uart_state *)0)->sus_cb_ev))

struct smp_uart_state {
    struct smp_transport sus_transport; /* keep first in struct */
    struct os_event sus_cb_ev;
    struct uart_dev *sus_dev;
    struct os_mbuf *sus_tx;
    int sus_tx_off;
    struct os_mbuf_pkthdr *sus_rx_pkt;
    struct os_mbuf_pkthdr *sus_rx_q;
    struct os_mbuf_pkthdr *sus_rx;
};

/**
 * Header for frames arriving over serial.
 */
struct smp_ser_hdr {
    uint16_t nsh_seq;
    uint16_t nsh_len;
};

static struct smp_uart_state smp_uart_state;

static uint16_t
smp_uart_mtu(struct os_mbuf *m)
{
    return MGMT_MAX_MTU;
}

/**
 * Called by mgmt to queue packet out to UART.
 */
static int
smp_uart_out(struct os_mbuf *m)
{
    struct smp_uart_state *sus = &smp_uart_state;
    struct os_mbuf_pkthdr *mpkt;
    struct os_mbuf *n;
    uint16_t tmp_buf[6];
    char *dst;
    int off;
    int boff;
    int slen;
    int sr;
    int rc;
    int last;
    int tx_sz;

    assert(OS_MBUF_IS_PKTHDR(m));
    mpkt = OS_MBUF_PKTHDR(m);

    /*
     * Compute CRC-16 and append it to end.
     */
    off = 0;
    tmp_buf[0] = CRC16_INITIAL_CRC;
    for (n = m; n; n = SLIST_NEXT(n, om_next)) {
        tmp_buf[0] = crc16_ccitt(tmp_buf[0], n->om_data, n->om_len);
    }
    tmp_buf[0] = htons(tmp_buf[0]);
    dst = os_mbuf_extend(m, sizeof(uint16_t));
    if (!dst) {
        goto err;
    }
    memcpy(dst, tmp_buf, sizeof(uint16_t));

    /*
     * Create another mbuf chain with base64 encoded data.
     */
    n = os_msys_get(MGMT_NLIP_MAX_FRAME, 0);
    if (!n || OS_MBUF_TRAILINGSPACE(n) < 32) {
        goto err;
    }

    while (off < mpkt->omp_len) {
        /*
         * First fragment has a different header, and length of the full frame
         * (need to base64 encode that).
         */
        if (off == 0) {
            tmp_buf[0] = htons(SHELL_NLIP_PKT);
        } else {
            tmp_buf[0] = htons(SHELL_NLIP_DATA);
        }
        rc = os_mbuf_append(n, tmp_buf, sizeof(uint16_t));
        if (rc) {
            goto err;
        }
        tx_sz = 2;

        if (off == 0) {
            tmp_buf[0] = htons(mpkt->omp_len);
            boff = sizeof(uint16_t);
        } else {
            boff = 0;
        }

        while (off < mpkt->omp_len) {
            slen = mpkt->omp_len - off;
            last = 1;
            if (slen > sizeof(tmp_buf) + boff) {
                slen = sizeof(tmp_buf) - boff;
                last = 0;
            }
            if (tx_sz + BASE64_ENCODE_SIZE(slen + boff) >= 124) {
                break;
            }
            rc = os_mbuf_copydata(m, off, slen, (uint8_t *)tmp_buf + boff);
            assert(rc == 0);

            off += slen;
            slen += boff;

            dst = os_mbuf_extend(n, BASE64_ENCODE_SIZE(slen));
            if (!dst) {
                goto err;
            }
            tx_sz += base64_encode(tmp_buf, slen, dst, last);
            boff = 0;
        }

        if (os_mbuf_append(n, "\n", 1)) {
            goto err;
        }
    }

    os_mbuf_free_chain(m);
    OS_ENTER_CRITICAL(sr);
    if (!sus->sus_tx) {
        sus->sus_tx = n;
        uart_start_tx(sus->sus_dev);
    } else {
        os_mbuf_concat(sus->sus_tx, n);
    }
    OS_EXIT_CRITICAL(sr);

    return 0;
err:
    os_mbuf_free_chain(m);
    os_mbuf_free_chain(n);
    return -1;
}

/**
 * Called by UART driver to send out next character.
 *
 * Interrupts disabled when smp_uart_tx_char/smp_uart_rx_char are called.
 */
static int
smp_uart_tx_char(void *arg)
{
    struct smp_uart_state *sus = (struct smp_uart_state *)arg;
    struct os_mbuf *m;
    uint8_t ch;

    if (!sus->sus_tx) {
        /*
         * Out of data. Return -1 makes UART stop asking for more.
         */
        return -1;
    }
    while (sus->sus_tx->om_len == sus->sus_tx_off) {
        /*
         * If no active mbuf, move to next one.
         */
        m = SLIST_NEXT(sus->sus_tx, om_next);
        os_mbuf_free(sus->sus_tx);
        sus->sus_tx = m;

        sus->sus_tx_off = 0;
        if (!sus->sus_tx) {
            return -1;
        }
    }

    os_mbuf_copydata(sus->sus_tx, sus->sus_tx_off++, 1, &ch);

    return ch;
}

/**
 * Check for full packet. If frame is not right, free the mbuf.
 */
static void
smp_uart_rx_pkt(struct smp_uart_state *sus, struct os_mbuf_pkthdr *rxm)
{
    struct os_mbuf *m;
    struct smp_ser_hdr *nsh;
    uint16_t crc;
    int rc;

    m = OS_MBUF_PKTHDR_TO_MBUF(rxm);

    if (rxm->omp_len <= sizeof(uint16_t) + sizeof(crc)) {
        goto err;
    }

    nsh = (struct smp_ser_hdr *)m->om_data;
    switch (nsh->nsh_seq) {
    case htons(SHELL_NLIP_PKT):
        if (sus->sus_rx_pkt) {
            os_mbuf_free_chain(OS_MBUF_PKTHDR_TO_MBUF(sus->sus_rx_pkt));
            sus->sus_rx_pkt = NULL;
        }
        break;
    case htons(SHELL_NLIP_DATA):
        if (!sus->sus_rx_pkt) {
            goto err;
        }
        break;
    default:
        goto err;
    }

    if (os_mbuf_append(m, "\0", 1)) {
        /*
         * Null-terminate the line for base64_decode's sake.
         */
        goto err;
    }
    m = os_mbuf_pullup(m, rxm->omp_len);
    if (!m) {
        /*
         * Make data contiguous for base64_decode's sake.
         */
        goto err;
    }
    rxm = OS_MBUF_PKTHDR(m);
    rc = base64_decode((char *)m->om_data + 2, (char *)m->om_data + 2);
    if (rc < 0) {
        goto err;
    }
    rxm->omp_len = m->om_len = rc + 2;
    if (sus->sus_rx_pkt) {
        os_mbuf_adj(m, 2);
        os_mbuf_concat(OS_MBUF_PKTHDR_TO_MBUF(sus->sus_rx_pkt), m);
    } else {
        sus->sus_rx_pkt = rxm;
    }

    m = OS_MBUF_PKTHDR_TO_MBUF(sus->sus_rx_pkt);
    nsh = (struct smp_ser_hdr *)m->om_data;
    if (sus->sus_rx_pkt->omp_len - sizeof(*nsh) == ntohs(nsh->nsh_len)) {
        os_mbuf_adj(m, 4);
        os_mbuf_adj(m, -2);
        smp_rx_req(&sus->sus_transport, m);
        sus->sus_rx_pkt = NULL;
    }
    return;
err:
    os_mbuf_free_chain(m);
}

/**
 * Callback from mgmt task context.
 */
static void
smp_uart_rx_frame(struct os_event *ev)
{
    struct smp_uart_state *sus = NUS_EV_TO_STATE(ev);
    struct os_mbuf_pkthdr *m;
    int sr;

    OS_ENTER_CRITICAL(sr);
    m = sus->sus_rx_q;
    sus->sus_rx_q = NULL;
    OS_EXIT_CRITICAL(sr);
    if (m) {
        smp_uart_rx_pkt(sus, m);
    }
}

/**
 * Receive a character from UART.
 */
static int
smp_uart_rx_char(void *arg, uint8_t data)
{
    struct smp_uart_state *sus = (struct smp_uart_state *)arg;
    struct os_mbuf *m;
    int rc;

    if (!sus->sus_rx) {
        m = os_msys_get_pkthdr(MGMT_NLIP_MAX_FRAME, 0);
        if (!m) {
            return 0;
        }
        sus->sus_rx = OS_MBUF_PKTHDR(m);
        if (OS_MBUF_TRAILINGSPACE(m) < MGMT_NLIP_MAX_FRAME) {
            /*
             * mbuf is too small.
             */
            os_mbuf_free_chain(m);
            sus->sus_rx = NULL;
            return 0;
        }
    }

    m = OS_MBUF_PKTHDR_TO_MBUF(sus->sus_rx);
    if (data == '\n') {
        /*
         * Full line of input. Process it outside interrupt context.
         */
        assert(!sus->sus_rx_q);
        sus->sus_rx_q = sus->sus_rx;
        sus->sus_rx = NULL;
        os_eventq_put(mgmt_evq_get(), &sus->sus_cb_ev);
        return 0;
    } else {
        rc = os_mbuf_append(m, &data, 1);
        if (rc == 0) {
            return 0;
        }
    }
    /* failed */
    sus->sus_rx->omp_len = 0;
    m->om_len = 0;
    os_mbuf_free_chain(SLIST_NEXT(m, om_next));
    SLIST_NEXT(m, om_next) = NULL;
    return 0;
}

void
smp_uart_pkg_init(void)
{
    struct smp_uart_state *sus = &smp_uart_state;
    int rc;
    struct uart_conf uc = {
        .uc_speed = MYNEWT_VAL(SMP_UART_SPEED),
        .uc_databits = 8,
        .uc_stopbits = 1,
        .uc_parity = UART_PARITY_NONE,
        .uc_flow_ctl = UART_FLOW_CTL_NONE,
        .uc_tx_char = smp_uart_tx_char,
        .uc_rx_char = smp_uart_rx_char,
        .uc_cb_arg = sus
    };

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = smp_transport_init(&sus->sus_transport, smp_uart_out, smp_uart_mtu);
    assert(rc == 0);

    sus->sus_dev =
      (struct uart_dev *)os_dev_open(MYNEWT_VAL(SMP_UART), 0, &uc);
    assert(sus->sus_dev);

    sus->sus_cb_ev.ev_cb = smp_uart_rx_frame;
}

/**
 * @} SMP
 */
