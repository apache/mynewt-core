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

#include "os/mynewt.h"

#if MYNEWT_VAL(SHELL_NEWTMGR)
#include <stddef.h>
#include "base64/base64.h"
#include "crc/crc16.h"
#include "console/console.h"
#include "shell/shell.h"
#include "shell_priv.h"

static shell_nlip_input_func_t g_shell_nlip_in_func;
static void *g_shell_nlip_in_arg;
static struct os_mqueue g_shell_nlip_mq;
static struct os_mbuf *g_nlip_mbuf;
static uint16_t g_nlip_expected_len;

void
shell_nlip_clear_pkt(void)
{
    if (g_nlip_mbuf) {
        os_mbuf_free_chain(g_nlip_mbuf);
        g_nlip_mbuf = NULL;
    }
    g_nlip_expected_len = 0;
}

int
shell_nlip_process(char *data, int len)
{
    uint16_t copy_len;
    int rc;
    struct os_mbuf *m;
    uint16_t crc;

    rc = base64_decode(data, data);
    if (rc < 0) {
        goto err;
    }
    len = rc;

    if (g_nlip_mbuf == NULL) {
        if (len < 2) {
            rc = -1;
            goto err;
        }

        g_nlip_expected_len = ntohs(*(uint16_t *) data);
        g_nlip_mbuf = os_msys_get_pkthdr(g_nlip_expected_len, 0);
        if (!g_nlip_mbuf) {
            rc = -1;
            goto err;
        }

        data += sizeof(uint16_t);
        len -= sizeof(uint16_t);
    }

    copy_len = min(g_nlip_expected_len - OS_MBUF_PKTHDR(g_nlip_mbuf)->omp_len,
            len);

    rc = os_mbuf_copyinto(g_nlip_mbuf, OS_MBUF_PKTHDR(g_nlip_mbuf)->omp_len,
            data, copy_len);
    if (rc != 0) {
        goto err;
    }

    if (OS_MBUF_PKTHDR(g_nlip_mbuf)->omp_len == g_nlip_expected_len) {
        if (g_shell_nlip_in_func) {
            crc = CRC16_INITIAL_CRC;
            for (m = g_nlip_mbuf; m; m = SLIST_NEXT(m, om_next)) {
                crc = crc16_ccitt(crc, m->om_data, m->om_len);
            }
            if (crc == 0 && g_nlip_expected_len >= sizeof(crc)) {
                os_mbuf_adj(g_nlip_mbuf, -sizeof(crc));
                g_shell_nlip_in_func(g_nlip_mbuf, g_shell_nlip_in_arg);
            } else {
                os_mbuf_free_chain(g_nlip_mbuf);
            }
        } else {
            os_mbuf_free_chain(g_nlip_mbuf);
        }
        g_nlip_mbuf = NULL;
        g_nlip_expected_len = 0;
    }

    return (0);
err:
    return (rc);
}

static int
shell_nlip_mtx(struct os_mbuf *m)
{
#define SHELL_NLIP_MTX_BUF_SIZE (12)
    uint8_t readbuf[SHELL_NLIP_MTX_BUF_SIZE];
    char encodebuf[BASE64_ENCODE_SIZE(SHELL_NLIP_MTX_BUF_SIZE)];
    char pkt_seq[3] = { '\n', SHELL_NLIP_PKT_START1, SHELL_NLIP_PKT_START2 };
    char esc_seq[2] = { SHELL_NLIP_DATA_START1, SHELL_NLIP_DATA_START2 };
    uint16_t totlen;
    uint16_t dlen;
    uint16_t off;
    uint16_t crc;
    int rb_off;
    int elen;
    uint16_t nwritten;
    uint16_t linelen;
    int rc;
    struct os_mbuf *tmp;
    void *ptr;

    /* Convert the mbuf into a packet.
     *
     * starts with 06 09
     * base64 encode:
     *  - total packet length (uint16_t)
     *  - data
     *  - crc
     * base64 encoded data must be less than 122 bytes per line to
     * avoid overflows and adhere to convention.
     *
     * continuation packets are preceded by 04 20 until the entire
     * buffer has been sent.
     */
    crc = CRC16_INITIAL_CRC;
    for (tmp = m; tmp; tmp = SLIST_NEXT(tmp, om_next)) {
        crc = crc16_ccitt(crc, tmp->om_data, tmp->om_len);
    }
    crc = htons(crc);
    ptr = os_mbuf_extend(m, sizeof(crc));
    if (!ptr) {
        rc = -1;
        goto err;
    }
    memcpy(ptr, &crc, sizeof(crc));

    totlen = OS_MBUF_PKTHDR(m)->omp_len;
    nwritten = 0;
    off = 0;

    /* Start a packet */
    console_write(pkt_seq, sizeof(pkt_seq));

    linelen = 0;

    rb_off = 2;
    dlen = htons(totlen);
    memcpy(readbuf, &dlen, sizeof(dlen));

    while (totlen > 0) {
        dlen = min(SHELL_NLIP_MTX_BUF_SIZE - rb_off, totlen);

        rc = os_mbuf_copydata(m, off, dlen, readbuf + rb_off);
        if (rc != 0) {
            goto err;
        }
        off += dlen;

        /* If the next packet will overwhelm the line length, truncate
         * this line.
         */
        if (linelen +
                BASE64_ENCODE_SIZE(min(SHELL_NLIP_MTX_BUF_SIZE - rb_off,
                        totlen - dlen)) >= 120) {
            elen = base64_encode(readbuf, dlen + rb_off, encodebuf, 1);
            console_write(encodebuf, elen);
            console_write("\n", 1);
            console_write(esc_seq, sizeof(esc_seq));
            linelen = 0;
        } else {
            elen = base64_encode(readbuf, dlen + rb_off, encodebuf, 0);
            console_write(encodebuf, elen);
            linelen += elen;
        }

        rb_off = 0;

        nwritten += elen;
        totlen -= dlen;
    }

    elen = base64_pad(encodebuf, linelen);
    console_write(encodebuf, elen);

    console_write("\n", 1);

    return (0);
err:
    return (rc);
}

int
shell_nlip_input_register(shell_nlip_input_func_t nf, void *arg)
{
    g_shell_nlip_in_func = nf;
    g_shell_nlip_in_arg = arg;

    return (0);
}

int
shell_nlip_output(struct os_mbuf *m)
{
    return os_mqueue_put(&g_shell_nlip_mq, os_eventq_dflt_get(), m);
}

static void
shell_event_data_in(struct os_event *ev)
{
    struct os_mbuf *m;

    /* Copy data out of the mbuf 12 bytes at a time and write it to
     * the console.
     */
    while (1) {
        m = os_mqueue_get(&g_shell_nlip_mq);
        if (!m) {
            break;
        }

        (void) shell_nlip_mtx(m);

        os_mbuf_free_chain(m);
    }
}

void
shell_nlip_init(void)
{
    os_mqueue_init(&g_shell_nlip_mq, shell_event_data_in, NULL);
}
#endif
