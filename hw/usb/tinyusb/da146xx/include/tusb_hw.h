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

#ifndef __TUSB_HW_H__
#define __TUSB_HW_H__

#define CFG_TUSB_MCU OPT_MCU_DA1469X

#include <syscfg/syscfg.h>
#include <stdbool.h>

#if MYNEWT_VAL(DA1469X_USB_KEEPALIVE_DETECT)
/**
 * Set event queue for keep-alive monitoring
 */
void tinyusb_ka_evq_set(struct os_eventq *evq);

/**
 * Check if USB keep-alive detection considers USB to be active
 */
bool tinyusb_ka_is_active(void);

/**
 * Get number of SOFs received since last USB disconnect
 */
uint32_t tinyusb_ka_get_sof_cnt(void);

/**
 * Reset SOF count to zero
 */
void tinyusb_ka_reset_sof_cnt(void);

/**
 *
 * Register callback for keep-alive state changes
 */
void tinyusb_ka_register_cb(void (*cb)(bool connected));
#endif

/**
 * Check if USB is connected (based on keep-alive and VBUS if used)
 */
bool tinyusb_is_connected(void);

/* Function should be exported from TinyUSB */
void tusb_vbus_changed(bool present);

#define CFG_TUSB_RHPORT0_SPEED  OPT_MODE_FULL_SPEED

#if defined(MYNEWT_VAL_USBD_CDC_NOTIFY_EP)
#define USBD_CDC_NOTIFY_EP      MYNEWT_VAL(USBD_CDC_NOTIFY_EP)
#else
#define USBD_CDC_NOTIFY_EP      0x81
#endif

#if defined(MYNEWT_VAL_USBD_CDC_NOTIFY_EP_SIZE)
#define USBD_CDC_NOTIFY_EP_SIZE MYNEWT_VAL(USBD_CDC_NOTIFY_EP_SIZE)
#else
#define USBD_CDC_NOTIFY_EP_SIZE 0x08
#endif

#if defined(MYNEWT_VAL_USBD_CDC_DATA_OUT_EP)
#define USBD_CDC_DATA_OUT_EP    MYNEWT_VAL(USBD_CDC_DATA_OUT_EP)
#else
#define USBD_CDC_DATA_OUT_EP    0x02
#endif

#if defined(MYNEWT_VAL_USBD_CDC_DATA_IN_EP)
#define USBD_CDC_DATA_IN_EP     MYNEWT_VAL(USBD_CDC_DATA_IN_EP)
#else
#define USBD_CDC_DATA_IN_EP     0x82
#endif

#if defined(MYNEWT_VAL_USBD_CDC_DATA_EP_SIZE)
#define USBD_CDC_DATA_EP_SIZE   MYNEWT_VAL(USBD_CDC_DATA_EP_SIZE)
#else
#define USBD_CDC_DATA_EP_SIZE   0x40
#endif

#if defined(MYNEWT_VAL_USBD_HID_REPORT_EP)
#define USBD_HID_REPORT_EP      MYNEWT_VAL(USBD_HID_REPORT_EP)
#else
#define USBD_HID_REPORT_EP      0x83
#endif

#if defined(MYNEWT_VAL_USBD_HID_REPORT_EP_SIZE)
#define USBD_HID_REPORT_EP_SIZE MYNEWT_VAL(USBD_HID_REPORT_EP_SIZE)
#else
#define USBD_HID_REPORT_EP_SIZE 0x10
#endif

#if defined(MYNEWT_VAL_USBD_HID_REPORT_EP_INTERVAL)
#define USBD_HID_REPORT_EP_INTERVAL MYNEWT_VAL(USBD_HID_REPORT_EP_INTERVAL)
#else
#define USBD_HID_REPORT_EP_INTERVAL 10
#endif

#if defined(MYNEWT_VAL_USBD_MSC_DATA_IN_EP)
#define USBD_MSC_DATA_IN_EP     MYNEWT_VAL(USBD_MSC_DATA_IN_EP)
#else
#define USBD_MSC_DATA_IN_EP     0x83
#endif

#if defined(MYNEWT_VAL_USBD_MSC_DATA_OUT_EP)
#define USBD_MSC_DATA_OUT_EP     MYNEWT_VAL(USBD_MSC_DATA_OUT_EP)
#else
#define USBD_MSC_DATA_OUT_EP     0x03
#endif

#if defined(MYNEWT_VAL_USBD_BTH_EVENT_EP)
#define USBD_BTH_EVENT_EP       MYNEWT_VAL(USBD_BTH_EVENT_EP)
#else
#define USBD_BTH_EVENT_EP       0x81
#endif

#if defined(MYNEWT_VAL_USBD_BTH_EVENT_EP_SIZE)
#define BTH_EVENT_EP_SIZE       MYNEWT_VAL(BTH_EVENT_EP_SIZE)
#else
#define USBD_BTH_EVENT_EP_SIZE  0x10
#endif

#if defined(MYNEWT_VAL_USBD_BTH_EVENT_EP_INTERVAL)
#define USBD_BTH_EVENT_EP_INTERVAL  MYNEWT_VAL(USBD_BTH_EVENT_EP_INTERVAL)
#else
#define USBD_BTH_EVENT_EP_INTERVAL 10
#endif

#if defined(MYNEWT_VAL_USBD_BTH_DATA_OUT_EP)
#define USBD_BTH_DATA_OUT_EP    MYNEWT_VAL(USBD_BTH_DATA_OUT_EP)
#else
#define USBD_BTH_DATA_OUT_EP    0x02
#endif

#if defined(MYNEWT_VAL_USBD_BTH_DATA_IN_EP)
#define USBD_BTH_DATA_IN_EP     MYNEWT_VAL(USBD_BTH_DATA_IN_EP)
#else
#define USBD_BTH_DATA_IN_EP     0x82
#endif

#if defined(MYNEWT_VAL_USBD_BTH_DATA_EP_SIZE)
#define USBD_BTH_DATA_EP_SIZE   MYNEWT_VAL(USBD_BTH_DATA_EP_SIZE)
#else
#define USBD_BTH_DATA_EP_SIZE   0x40
#endif

#endif
