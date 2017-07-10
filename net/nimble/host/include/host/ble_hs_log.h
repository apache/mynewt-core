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

#ifndef H_BLE_HS_LOG_
#define H_BLE_HS_LOG_

#include "log/log.h"
#include "host/ble_monitor.h"
#ifdef __cplusplus
extern "C" {
#endif

struct os_mbuf;

extern struct log ble_hs_log;

#define BLE_HS_LOG(lvl, ...) \
    LOG_ ## lvl(&ble_hs_log, LOG_MODULE_NIMBLE_HOST, __VA_ARGS__)

#define BLE_HS_LOG_ADDR(lvl, addr)                      \
    BLE_HS_LOG(lvl, "%02x:%02x:%02x:%02x:%02x:%02x",    \
               (addr)[5], (addr)[4], (addr)[3],         \
               (addr)[2], (addr)[1], (addr)[0])

void ble_hs_log_mbuf(const struct os_mbuf *om);
void ble_hs_log_flat_buf(const void *data, int len);


#if MYNEWT_VAL(BLE_MONITOR_DEBUG)

#define BLE_HS_ERROR(fmt, ...) \
    ble_monitor_log(LOG_LEVEL_ERROR, "%s: " fmt, __func__, ## __VA_ARGS__)

#define BLE_HS_WARN(fmt, ...) \
    ble_monitor_log(LOG_LEVEL_WARN, "%s: " fmt, __func__, ## __VA_ARGS__)

#define BLE_HS_INFO(fmt, ...) \
    ble_monitor_log(LOG_LEVEL_INFO, "%s: " fmt, __func__, ## __VA_ARGS__)

#define BLE_HS_DEBUG(fmt, ...) \
    ble_monitor_log(LOG_LEVEL_DEBUG, "%s: " fmt, __func__, ## __VA_ARGS__)

#else

#define BLE_HS_ERROR(fmt, ...) \
    BLE_HS_LOG(ERROR, "%s: " fmt, __func__, ## __VA_ARGS__)

#define BLE_HS_WARN(fmt, ...) \
    BLE_HS_LOG(WARN, "%s: " fmt, __func__, ## __VA_ARGS__)

#define BLE_HS_INFO(fmt, ...) \
    BLE_HS_LOG(INFO, "%s: " fmt, __func__, ## __VA_ARGS__)

#define BLE_HS_DEBUG(fmt, ...) \
    BLE_HS_LOG(DEBUG, "%s: " fmt, __func__, ## __VA_ARGS__)

#endif

const char *ble_addr_str(const ble_addr_t *addr);

const char *ble_hex(const void *buf, size_t len);

const char *ble_hex_mbuf(const struct os_mbuf *om);


#ifdef __cplusplus
}
#endif

#endif
