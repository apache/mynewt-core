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
#ifndef __WIFI_MGMT_H__
#define __WIFI_MGMT_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wi-Fi interface abstraction.
 */
#define WIFI_SSID_MAX           32	/* max SSID name length */
#define WIFI_BSSID_LEN           6      /* MAC address len */
#define WIFI_SCAN_CNT_MAX       20
#define WIFI_KEY_MAX            64      /* max key length */
#define WIFI_SSID_EMPTY(ssid)   (ssid)[0] == '\0'

/*
 * Info about an access point.
 */
struct wifi_ap {
    char wa_ssid[WIFI_SSID_MAX + 1];
    char wa_bssid[WIFI_BSSID_LEN + 1];
    int8_t wa_rssi;
    uint8_t wa_key_type;
    uint8_t wa_channel;
};

struct wifi_if_ops;

/*
 * Wifi interface
 */
struct wifi_if {
    enum  {
        STOPPED = 0,
        INIT,
        CONNECTING,
        DHCP_WAIT,
        CONNECTED,
        SCANNING
    } wi_state, wi_tgt;
    struct os_mutex wi_mtx;
    struct os_event wi_event;
    struct os_callout wi_timer;
    const struct wifi_if_ops *wi_ops;

    uint8_t wi_scan_cnt;
    struct wifi_ap wi_scan[WIFI_SCAN_CNT_MAX];
    char wi_ssid[WIFI_SSID_MAX + 1];
    char wi_key[WIFI_KEY_MAX + 1];
    uint8_t wi_myip[4];
};

/*
 * XXX. is wifi_if_lookup() needed? It is unlikely that there's going to be
 * multiple wifi interfaces on a system.
 */
struct wifi_if *wifi_if_lookup(int port);
int wifi_if_register(struct wifi_if *, const struct wifi_if_ops *);

int wifi_start(struct wifi_if *);
int wifi_connect(struct wifi_if *);
int wifi_stop(struct wifi_if *w);
int wifi_scan_start(struct wifi_if *w);

int wifi_task_init(uint8_t prio, os_stack_t *stack, uint16_t stack_size);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_MGMT_H__ */
