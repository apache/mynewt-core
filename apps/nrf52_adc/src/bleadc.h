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

#ifndef H_BLEADC_
#define H_BLEADC_

#include "log/log.h"
#ifdef __cplusplus
extern "C" {
#endif

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

extern struct log bleadc_log;

/* bleadc uses the first "peruser" log module. */
#define BLEADC_LOG_MODULE  (LOG_MODULE_PERUSER + 0)

/* Convenience macro for logging to the bleadc module. */
#define BLEADC_LOG(lvl, ...) \
    LOG_ ## lvl(&bleadc_log, BLEADC_LOG_MODULE, __VA_ARGS__)

/** GATT server. */
#define GATT_SVR_SVC_ALERT_UUID               0x1811
#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID   0x2A47
#define GATT_SVR_CHR_NEW_ALERT                0x2A46
#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID   0x2A48
#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID      0x2A45
#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT        0x2A44

/* Sensor Data */
/* e761d2af-1c15-4fa7-af80-b5729002b340 */
static const uint8_t gatt_svr_svc_sns_uuid[16] = {
    0x40, 0xb3, 0x20, 0x90, 0x72, 0xb5, 0x80, 0xaf,
    0xa7, 0x4f, 0x15, 0x1c, 0xaf, 0xd2, 0x61, 0xe7 };
#define ADC_SNS_TYPE          0xDEAD
#define ADC_SNS_STRING "eTape Water Level Sensor"
#define ADC_SNS_VAL               0xBEEF
#define SPI_SNS_TYPE          0xDE48
#define SPI_SNS_STRING "SPI Sensor"
#define SPI_SNS_VAL               0xBE48

uint16_t gatt_adc_val; 
uint8_t gatt_spi_val;

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int gatt_svr_init(void);

/** Misc. */
void print_bytes(const uint8_t *bytes, int len);
void print_addr(const void *addr);

#ifdef __cplusplus
}
#endif

#endif
