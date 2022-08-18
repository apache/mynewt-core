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

#include <os/mynewt.h>

#include <class/cdc/cdc_device.h>
#include <cdc/cdc.h>

#include <console/console.h>
#include <bsp/bsp.h>

static struct os_event rx_receive_event;
static struct os_event tx_flush_event;
static bool connected;

static const struct cdc_callbacks console_cdc_callback;

cdc_itf_t console_cdc_itf = {
    .callbacks = &console_cdc_callback
};

static void
cdc_schedule_tx_flush(void)
{
    os_eventq_put(os_eventq_dflt_get(), &tx_flush_event);
}

static void
cdc_write(int c)
{
    uint32_t written;

    written = tud_cdc_n_write_char(console_cdc_itf.cdc_num, c);
    if (tud_cdc_n_write_available(console_cdc_itf.cdc_num) == 0) {
        tud_cdc_n_write_flush(console_cdc_itf.cdc_num);
        if (written == 0) {
            tud_cdc_n_write_char(console_cdc_itf.cdc_num, c);
        }
    }
}

int
console_out_nolock(int c)
{
    cdc_write(c);

    if ('\n' == c) {
        cdc_write('\r');
    }

    cdc_schedule_tx_flush();

    return c;
}

void
console_rx_restart(void)
{
    os_eventq_put(os_eventq_dflt_get(), &rx_receive_event);
}

static void
tx_flush_ev_cb(struct os_event *ev)
{
#if MYNEWT_VAL(USBD_CDC_TX_BUFSIZE)
    if (connected && tud_cdc_n_write_available(console_cdc_itf.cdc_num) < CFG_TUD_CDC_TX_BUFSIZE) {
#else
    if (connected && tud_cdc_n_write_available(console_cdc_itf.cdc_num) < USBD_CDC_DATA_EP_SIZE) {
#endif
        if (tud_cdc_n_write_flush(console_cdc_itf.cdc_num) == 0) {
            /*
             * Previous data not sent yet.
             * There is no data sent notification in tinyusb/cdc, retry flush later.
             */
            cdc_schedule_tx_flush();
        }
    }
}

static void
rx_ev_cb(struct os_event *ev)
{
    static int console_rejected_char = -1;
    int ret;

    /* We may have unhandled character - try it first */
    if (console_rejected_char >= 0) {
        ret = console_handle_char(console_rejected_char);
        if (ret < 0) {
            return;
        }
    }

    while (tud_cdc_n_available(console_cdc_itf.cdc_num)) {
        console_rejected_char = tud_cdc_n_read_char(console_cdc_itf.cdc_num);
        if (console_rejected_char >= 0) {
            ret = console_handle_char(console_rejected_char);
            if (ret < 0) {
                return;
            }
        } else {
            break;
        }
    }

    console_rejected_char = -1;
}

static void
cdc_console_line_state_cb(cdc_itf_t *itf, bool dtr, bool rts)
{
    (void)itf;
    (void)rts;

    if (dtr != connected) {
        connected = dtr;
        cdc_schedule_tx_flush();
    }
}

/* Invoked when CDC interface received data from host */
static void
cdc_console_rx_cb(cdc_itf_t *itf)
{
    (void)itf;
    os_eventq_put(os_eventq_dflt_get(), &rx_receive_event);
}

static void
cdc_console_line_coding_cb(cdc_itf_t *itf, const cdc_line_coding_t *p_line_coding)
{
    (void)itf;
}

static void
cdc_console_rx_wanted_cb(cdc_itf_t *itf, char wanted_char)
{
    (void)itf;
}

int
usb_cdc_console_pkg_init(void)
{
    rx_receive_event.ev_cb = rx_ev_cb;
    tx_flush_event.ev_cb = tx_flush_ev_cb;

    cdc_itf_add(&console_cdc_itf);

    return 0;
}

int
usb_cdc_console_is_init(void)
{
    return (int)tud_cdc_n_connected(console_cdc_itf.cdc_num);
}

static const struct cdc_callbacks console_cdc_callback = {
    .cdc_rx_cb = cdc_console_rx_cb,
    .cdc_line_coding_cb = cdc_console_line_coding_cb,
    .cdc_line_state_cb = cdc_console_line_state_cb,
    .cdc_rx_wanted_cb = cdc_console_rx_wanted_cb,
    .cdc_send_break_cb = NULL,
    .cdc_tx_complete_cb = NULL,
};

