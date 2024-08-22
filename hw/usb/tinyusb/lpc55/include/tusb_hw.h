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

#include <syscfg/syscfg.h>

#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(64)))
#define CFG_TUSB_MEM_SECTION __attribute__((section(".usbram")))

#if MYNEWT_VAL(MCU_LPC55XX)
#define CFG_TUSB_MCU OPT_MCU_LPC55XX
#endif

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
#define USBD_CDC_DATA_OUT_EP    0x01
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
#define USBD_MSC_DATA_IN_EP     0x84
#endif

#if defined(MYNEWT_VAL_USBD_MSC_DATA_OUT_EP)
#define USBD_MSC_DATA_OUT_EP     MYNEWT_VAL(USBD_MSC_DATA_OUT_EP)
#else
#define USBD_MSC_DATA_OUT_EP     0x04
#endif

#endif
