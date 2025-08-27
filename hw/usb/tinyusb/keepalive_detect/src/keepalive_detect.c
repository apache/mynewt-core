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
#include <string.h>
#include "keepalive_detect/keepalive_detect.h"
#include "syscfg/syscfg.h"

#if MYNEWT_VAL(USB_KEEPALIVE_DETECT)

/* Keep-alive detection state structure */
struct usb_keepalive_state {
    uint32_t last_frame_num;
    uint32_t sof_cnt;
    uint32_t prev_sof_time;
    bool usb_active;
    uint32_t suspend_cnt;
    struct os_callout monitor_callout;
    uint32_t ticks;
    uint32_t prev_sof_cnt;
};

/* Keep-alive detection variables */
static struct usb_keepalive_state g_uks;
static struct os_mutex g_uks_mutex;
static usb_keepalive_cb_t g_usb_ka_cb;
static const struct usb_keepalive_cbs *g_keepalive_cbs;

/*
 * Monitor callout callback for periodic SOF checking
 */
static void
keepalive_monitor_cb(struct os_event *ev)
{
    uint32_t curr_sof_cnt;
    uint32_t sof_diff;

    os_mutex_pend(&g_uks_mutex, OS_TIMEOUT_NEVER);
    curr_sof_cnt = g_uks.sof_cnt;
    os_mutex_release(&g_uks_mutex);

    /* Calculate SOF rate */
    sof_diff = curr_sof_cnt - g_uks.prev_sof_cnt;
    g_uks.prev_sof_cnt = curr_sof_cnt;

    /* Check if we're receiving SOFs */
    if (sof_diff > 0) {
        g_uks.ticks = 0;

        if (!usb_keepalive_is_active()) {
            os_mutex_pend(&g_uks_mutex, OS_TIMEOUT_NEVER);
            g_uks.usb_active = true;
            os_mutex_release(&g_uks_mutex);

            if (g_usb_ka_cb) {
                g_usb_ka_cb(true);
            }
        }
    } else {
        g_uks.ticks += MYNEWT_VAL(USB_KEEPALIVE_MONITOR_RATE_MS);

        if (g_uks.ticks >= MYNEWT_VAL(USB_KEEPALIVE_TIMEOUT_MS)) {
            if (usb_keepalive_is_active()) {
                os_mutex_pend(&g_uks_mutex, OS_TIMEOUT_NEVER);
                g_uks.usb_active = false;
                os_mutex_release(&g_uks_mutex);

                if (g_usb_ka_cb) {
                    g_usb_ka_cb(false);
                }
            }
            g_uks.ticks = 0;
        }
    }

    /* Reset callout */
    os_callout_reset(&g_uks.monitor_callout,
                     os_time_ms_to_ticks32(MYNEWT_VAL(USB_KEEPALIVE_MONITOR_RATE_MS)));
}

int
usb_keepalive_init(void)
{
    int rc;

    /* Initialize mutex */
    rc = os_mutex_init(&g_uks_mutex);
    if (rc != 0) {
        return rc;
    }

    /* Initialize state */
    memset(&g_uks, 0, sizeof(g_uks));

    /* Initialize callout */
    os_callout_init(&g_uks.monitor_callout, os_eventq_dflt_get(),
                    keepalive_monitor_cb, NULL);

    /* Enable platform-specific interrupts if callbacks are registered */
    if (g_keepalive_cbs && g_keepalive_cbs->enable_interrupts) {
        g_keepalive_cbs->enable_interrupts();
    }

    /* Start monitoring */
    os_callout_reset(&g_uks.monitor_callout,
                     os_time_ms_to_ticks32(MYNEWT_VAL(USB_KEEPALIVE_MONITOR_RATE_MS)));

    return 0;
}

void
usb_keepalive_evq_set(struct os_eventq *evq)
{
    if (evq == NULL) {
        evq = os_eventq_dflt_get();
    }

    os_callout_init(&g_uks.monitor_callout, evq,
                    keepalive_monitor_cb, NULL);
}

void
usb_keepalive_register_cbs(const struct usb_keepalive_cbs *cbs)
{
    g_keepalive_cbs = cbs;
}

void
usb_keepalive_register_cb(usb_keepalive_cb_t cb)
{
    g_usb_ka_cb = cb;
}

bool
usb_keepalive_is_active(void)
{
    bool active;

    os_mutex_pend(&g_uks_mutex, OS_TIMEOUT_NEVER);
    active = g_uks.usb_active;
    os_mutex_release(&g_uks_mutex);

    return active;
}

uint32_t
usb_keepalive_get_sof_cnt(void)
{
    uint32_t cnt;

    os_mutex_pend(&g_uks_mutex, OS_TIMEOUT_NEVER);
    cnt = g_uks.sof_cnt;
    os_mutex_release(&g_uks_mutex);

    return cnt;
}

void
usb_keepalive_handle_sof(void)
{
    uint16_t current_frame;

    if (g_keepalive_cbs && g_keepalive_cbs->get_frame_number) {
        current_frame = g_keepalive_cbs->get_frame_number();

        os_mutex_pend(&g_uks_mutex, OS_TIMEOUT_NEVER);

        /* Check if frame number changed (valid SOF) */
        if (current_frame != g_uks.last_frame_num) {
            g_uks.last_frame_num = current_frame;
            g_uks.sof_cnt++;
            g_uks.prev_sof_time = os_cputime_get32();

            /* USB is active if we're receiving SOFs */
            if (!g_uks.usb_active) {
                g_uks.usb_active = true;
                if (g_usb_ka_cb) {
                    g_usb_ka_cb(true);
                }
            }

            /* Reset suspend cnter */
            g_uks.suspend_cnt = 0;
        }

        os_mutex_release(&g_uks_mutex);
    }
}

void
usb_keepalive_handle_suspend(void)
{
    os_mutex_pend(&g_uks_mutex, OS_TIMEOUT_NEVER);

    g_uks.suspend_cnt++;

    if (g_uks.usb_active &&
        g_uks.suspend_cnt > MYNEWT_VAL(USB_KEEPALIVE_SUSPEND_COUNT)) {
        /* USB disconnected (no keep-alives) */
        g_uks.usb_active = false;
        g_uks.sof_cnt = 0;
        if (g_usb_ka_cb) {
            g_usb_ka_cb(false);
        }
    }

    os_mutex_release(&g_uks_mutex);
}

void
usb_keepalive_handle_resume(void)
{
    os_mutex_pend(&g_uks_mutex, OS_TIMEOUT_NEVER);
    g_uks.suspend_cnt = 0;
    os_mutex_release(&g_uks_mutex);
}

#endif /* MYNEWT_VAL(USB_KEEPALIVE_DETECT) */
