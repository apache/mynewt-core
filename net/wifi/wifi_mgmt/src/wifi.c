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

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "os/mynewt.h"
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <shell/shell.h>
#include <console/console.h>

#include "wifi_mgmt/wifi_mgmt.h"
#include "wifi_mgmt/wifi_mgmt_if.h"
#include "wifi_priv.h"

static struct os_task wifi_os_task;
struct os_eventq wifi_evq;

static struct wifi_ap *wifi_find_ap(struct wifi_if *wi, char *ssid);
static void wifi_events(struct os_event *);

static void wifi_event_state(struct os_event *ev);

static struct wifi_if *wifi_if;

/*
 * Looks up interface based on port number.
 */
struct wifi_if *
wifi_if_lookup(int port)
{
    assert(port == 0);
    return wifi_if;
}

/*
 * Called by wi-fi driver to register itself.
 */
int
wifi_if_register(struct wifi_if *wi, const struct wifi_if_ops *ops)
{
    if (wifi_if) {
        return -1;
    }
    wifi_if = wi;

    wi->wi_ops = ops;
    os_mutex_init(&wi->wi_mtx);
    os_callout_init(&wi->wi_timer, &wifi_evq, wifi_events, wi);
    wi->wi_event.ev_cb = wifi_event_state;
    wi->wi_event.ev_arg = wi;

    return 0;
}

/*
 * For Wi-fi mgmt state machine, set the target state, and queue an
 * event to do the state transition in wifi task context.
 */
static void
wifi_tgt_state(struct wifi_if *wi, int state)
{
    wi->wi_tgt = state;
    os_eventq_put(&wifi_evq, &wi->wi_event);
}

/*
 * Wi-fi driver reports a response to wifi scan request.
 * Driver reports networks one at a time.
 */
void
wifi_scan_result(struct wifi_if *wi, struct wifi_ap *ap)
{
    if (wi->wi_scan_cnt == WIFI_SCAN_CNT_MAX) {
        return;
    }
    wi->wi_scan[wi->wi_scan_cnt++] = *ap;
}

/*
 * Wifi driver reports that scan is finished.
 */
void
wifi_scan_done(struct wifi_if *wi, int status)
{
    struct wifi_ap *ap = NULL;

    console_printf("scan_results %d: %d\n", wi->wi_scan_cnt, status);
    if (status) {
        wifi_tgt_state(wi, STOPPED);
        return;
    }

    /*
     * XXX decide what to do with scan results here.
     */
    if (!WIFI_SSID_EMPTY(wi->wi_ssid)) {
        ap = wifi_find_ap(wi, wi->wi_ssid);
    }
    if (ap) {
        wifi_tgt_state(wi, CONNECTING);
    } else {
        wifi_tgt_state(wi, INIT);
    }
}

/*
 * Wi-fi driver reports whether it was successful in establishing connection
 * to an AP. XXX need status codes in wifi_mgmt.h
 */
void
wifi_connect_done(struct wifi_if *wi, int status)
{
    console_printf("connect_done : %d\n", status);
    if (status) {
        wifi_tgt_state(wi, INIT);
        return;
    }
    wifi_tgt_state(wi, DHCP_WAIT);
}

/*
 * Wi-fi driver reports that there's an IP address. We can start using
 * this interface.
 */
void
wifi_dhcp_done(struct wifi_if *wi, uint8_t *ip)
{
    console_printf("dhcp done %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    wifi_tgt_state(wi, CONNECTED);
}

/*
 * Wi-fi driver reports that we are disconnected from an AP.
 */
void
wifi_disconnected(struct wifi_if *wi, int status)
{
    console_printf("disconnect : %d\n", status);
    wifi_tgt_state(wi, INIT);
}

static struct wifi_ap *
wifi_find_ap(struct wifi_if *wi, char *ssid)
{
    int i;

    for (i = 0; i < wi->wi_scan_cnt; i++) {
        if (!strcmp(wi->wi_scan[i].wa_ssid, ssid)) {
            return &wi->wi_scan[i];
        }
    }
    return NULL;
}

static void
wifi_events(struct os_event *ev)
{
    /*
     * Expire connection attempts. Periodic scanning if tgt AP not visible.
     */
}

/*
 * Called by user to start bringing up Wi-fi interface online.
 */
int
wifi_start(struct wifi_if *wi)
{
    if (wi->wi_state != STOPPED) {
        return -1;
    }
    wifi_tgt_state(wi, INIT);
    return 0;
}

/*
 * Called by user to stop Wi-fi interface.
 */
int
wifi_stop(struct wifi_if *wi)
{
    wifi_tgt_state(wi, STOPPED);
    return 0;
}

/*
 * Called by user to connect Wi-fi interface to AP. Will fail if no
 * SSID name is set.
 */
int
wifi_connect(struct wifi_if *wi)
{
    switch (wi->wi_state) {
    case STOPPED:
        return -1;
    case INIT:
        if (WIFI_SSID_EMPTY(wi->wi_ssid)) {
            return -1;
        }
        wifi_tgt_state(wi, CONNECTING);
        return 0;
    default:
        return -1;
    }
    return 0;
}

/*
 * From user to initiate Wi-Fi scan.
 */
int
wifi_scan_start(struct wifi_if *wi)
{
    if (wi->wi_state == INIT) {
        wifi_tgt_state(wi, SCANNING);
        return 0;
    }
    return -1;
}

/*
 * Wi-fi mgmt state machine.
 */
static void
wifi_step(struct wifi_if *wi)
{
    int rc;
    struct wifi_ap *ap;

    switch (wi->wi_tgt) {
    case STOPPED:
        if (wi->wi_state != STOPPED) {
            if (wi->wi_state >= CONNECTING) {
                wi->wi_ops->wio_disconnect(wi);
            }
            wi->wi_ops->wio_deinit(wi);
            wi->wi_state = STOPPED;
        }
        break;
    case INIT:
        if (wi->wi_state == STOPPED) {
            rc = wi->wi_ops->wio_init(wi);
            console_printf("wifi_init : %d\n", rc);
            if (!rc) {
                wi->wi_state = INIT;
            }
        } else if (wi->wi_state == SCANNING) {
            wi->wi_state = wi->wi_tgt;
        } else if (wi->wi_state == CONNECTING) {
            wi->wi_state = wi->wi_tgt;
        }
        break;
    case SCANNING:
        if (wi->wi_state == INIT) {
            memset(wi->wi_scan, 0, sizeof(wi->wi_scan));
            rc = wi->wi_ops->wio_scan_start(wi);
            console_printf("wifi_request_scan : %d\n", rc);
            if (rc != 0) {
                break;
            }
            wi->wi_state = SCANNING;
        } else {
            wi->wi_tgt = wi->wi_state;
        }
        break;
    case CONNECTING:
        if (wi->wi_state == INIT || wi->wi_state == SCANNING) {
            ap = wifi_find_ap(wi, wi->wi_ssid);
            if (!ap) {
                wifi_tgt_state(wi, SCANNING);
                break;
            }
            rc = wi->wi_ops->wio_connect(wi, ap);
            console_printf("wifi_connect : %d\n", rc);
            if (rc == 0) {
                wi->wi_state = CONNECTING;
            } else {
                wi->wi_tgt = STOPPED;
            }
        }
        break;
    case DHCP_WAIT:
        wi->wi_state = wi->wi_tgt;
        break;
    case CONNECTED:
        wi->wi_state = wi->wi_tgt;
        break;
    default:
        console_printf("wifi_step() unknown tgt : %d\n", wi->wi_tgt);
        wi->wi_state = wi->wi_tgt;
        break;
    }
}

static void
wifi_event_state(struct os_event *ev)
{
    struct wifi_if *wi;

    wi = (struct wifi_if *)ev->ev_arg;
    while (wi->wi_state != wi->wi_tgt) {
        wifi_step(wi);
    }
}

static void
wifi_task(void *arg)
{
    while (1) {
        os_eventq_run(&wifi_evq);
    }
}

int
wifi_task_init(uint8_t prio, os_stack_t *stack, uint16_t stack_size)
{
#if MYNEWT_VAL(WIFI_MGMT_CLI)
    shell_cmd_register(&wifi_cli_cmd);
#endif
    os_eventq_init(&wifi_evq);

    return os_task_init(&wifi_os_task, "wifi", wifi_task, NULL,
      prio, OS_WAIT_FOREVER, stack, stack_size);
}
