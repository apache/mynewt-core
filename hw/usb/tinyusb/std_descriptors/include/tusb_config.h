/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * COMMON CONFIGURATION
 */

#include <tusb_hw.h>

/* defined by compiler flags for flexibility */
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE

#define CFG_TUSB_OS                 OPT_OS_MYNEWT
#define CFG_TUSB_DEBUG              0

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

/**
 * DEVICE CONFIGURATION
 */
#define CFG_TUD_ENDPOINT0_SIZE   MYNEWT_VAL(USBD_EP0_SIZE)

/* ------------- CLASS ------------- */
#define CFG_TUD_CDC              MYNEWT_VAL(USBD_CDC)
#define CFG_TUD_HID              MYNEWT_VAL(USBD_HID)
#define CFG_TUD_MSC              0
#define CFG_TUD_MIDI             0
#define CFG_TUD_VENDOR           0
#define CFG_TUD_USBTMC           0
#define CFG_TUD_DFU_RT           0
#define CFG_TUD_NET              0
#define CFG_TUD_BTH              MYNEWT_VAL(USBD_BTH)

/* Minimal number for alternative interfaces that is recognized by Windows as Bluetooth radio controller */
#define CFG_TUD_BTH_ISO_ALT_COUNT 2

/*  CDC FIFO size of TX and RX */
#define CFG_TUD_CDC_RX_BUFSIZE   64
#define CFG_TUD_CDC_TX_BUFSIZE   64

/* HID buffer size Should be sufficient to hold ID (if any) + Data */
#define CFG_TUD_HID_BUFSIZE      16

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
