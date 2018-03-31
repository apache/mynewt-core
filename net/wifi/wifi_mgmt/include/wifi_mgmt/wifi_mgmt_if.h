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
#ifndef __WIFI_MGMT_IF_H__
#define __WIFI_MGMT_IF_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Interface between Wi-fi management and the driver.
 */
struct wifi_if_ops {
    int (*wio_init)(struct wifi_if *);
    void (*wio_deinit)(struct wifi_if *);
    int (*wio_scan_start)(struct wifi_if *);
    int (*wio_connect)(struct wifi_if *, struct wifi_ap *);
    void (*wio_disconnect)(struct wifi_if *);
};

/*
 * Exported so driver can use this for it's timers.
 */
extern struct os_eventq wifi_evq;

/*
 * Called by the Wi-fi driver.
 */
void wifi_scan_result(struct wifi_if *, struct wifi_ap *);
void wifi_scan_done(struct wifi_if *, int status);
void wifi_connect_done(struct wifi_if *wi, int status);
void wifi_disconnected(struct wifi_if *wi, int status);
void wifi_dhcp_done(struct wifi_if *wi, uint8_t *ip); /* XXX more IP info */

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_MGMT_IF_H__ */
