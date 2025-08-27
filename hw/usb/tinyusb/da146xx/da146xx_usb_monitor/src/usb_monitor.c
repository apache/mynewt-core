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
#include "syscfg/syscfg.h"
#include <tusb.h>

#if MYNEWT_VAL(DA146XX_USB_MONITOR)
#include "da146xx_usb_monitor/da146xx_usb_monitor.h"

/* Keep-alive detection state structure */
struct da146xx_usb_monitor_state {
    struct os_callout monitor_callout;
    bool last_connected_state;
    da146xx_usb_monitor_cb_t callback;
};

/* USB monitor state */
static struct da146xx_usb_monitor_state g_ums;

/*
 * Monitor callout callback for periodic connection checking
 */
static void
da146xx_usb_monitor_cb(struct os_event *ev)
{
    bool is_connected;
    bool state_changed = false;

    /* Check if CDC is connected and mounted */
    is_connected = tud_connected();
    /* Check for state change */
    if (is_connected != g_ums.last_connected_state) {
        g_ums.last_connected_state = is_connected;
        state_changed = true;
    }

    /* Notify callback if state changed */
    if (state_changed && g_ums.callback) {
        g_ums.callback(is_connected);
    }

    /* Reset callout */
    os_callout_reset(&g_ums.monitor_callout,
                     os_time_ms_to_ticks32(MYNEWT_VAL(DA146XX_USB_MONITOR_RATE_MS)));
}

static void
da146xx_usb_monitor_evq_set(struct os_eventq *evq)
{
    os_callout_init(&g_ums.monitor_callout, evq, da146xx_usb_monitor_cb, NULL);
}

int
da146xx_usb_monitor_init(void)
{
    /* Initialize state */
    memset(&g_ums, 0, sizeof(g_ums));

#if MYNEWT_VAL_DA146XX_USB_MONITOR_EVQ
    /* Set event queue if specified */
    da146xx_usb_monitor_evq_set(MYNEWT_VAL(DA146XX_USB_MONITOR_EVQ));
#else
    /* Use default event queue */
    da146xx_usb_monitor_evq_set(os_eventq_dflt_get());
#endif

    /* Start monitoring */
    os_callout_reset(&g_ums.monitor_callout,
                     os_time_ms_to_ticks32(MYNEWT_VAL(DA146XX_USB_MONITOR_RATE_MS)));

    return 0;
}

void
da146xx_usb_monitor_register_cb(da146xx_usb_monitor_cb_t cb)
{
    g_ums.callback = cb;
}

bool
da146xx_usb_monitor_is_connected(void)
{
    return g_ums.last_connected_state;
}
#endif /* MYNEWT_VAL(DA146XX_USB_MONITOR) */
