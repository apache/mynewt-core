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

#if MYNEWT_VAL_CHOICE(USBD_RHPORT, USB0)
#undef CFG_TUSB_RHPORT0_MODE
#if MYNEWT_VAL(USBD_HIGH_SPEED)
#define CFG_TUSB_RHPORT0_MODE  (OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED)
#else
#define CFG_TUSB_RHPORT0_MODE  (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif
#define USBD_RHPORT_MODE CFG_TUSB_RHPORT0_MODE

#elif MYNEWT_VAL_CHOICE(USBD_RHPORT, USB1)
#undef CFG_TUSB_RHPORT1_MODE
#if MYNEWT_VAL(USBD_HIGH_SPEED)
#define CFG_TUSB_RHPORT1_MODE  (OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED)
#else
#define CFG_TUSB_RHPORT1_MODE  (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif
#define USBD_RHPORT_MODE CFG_TUSB_RHPORT1_MODE
#endif

#if MYNEWT_VAL(OS_SCHEDULING)
#define CFG_TUSB_OS                 OPT_OS_MYNEWT
#else
#define CFG_TUSB_OS                 OPT_OS_NONE
#endif
#define CFG_TUSB_DEBUG              MYNEWT_VAL(CFG_TUSB_DEBUG)

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
/*
 * If CDC_CONSOLE does not have specific values for endpoint configuration,
 * use values for unspecified CDC
 */
#if defined(MYNEWT_VAL_USBD_CDC_CONSOLE_NOTIFY_EP_SIZE)
#define USBD_CDC_CONSOLE_NOTIFY_EP_SIZE MYNEWT_VAL(USBD_CDC_CONSOLE_NOTIFY_EP_SIZE)
#else
#define USBD_CDC_CONSOLE_NOTIFY_EP_SIZE USBD_CDC_NOTIFY_EP_SIZE
#endif

#if defined(MYNEWT_VAL_USBD_CDC_CONSOLE_NOTIFY_EP)
#define USBD_CDC_CONSOLE_NOTIFY_EP      MYNEWT_VAL(USBD_CDC_CONSOLE_NOTIFY_EP)
#else
#define USBD_CDC_CONSOLE_NOTIFY_EP      USBD_CDC_NOTIFY_EP
#endif

#if defined(MYNEWT_VAL_USBD_CDC_CONSOLE_DATA_OUT_EP)
#define USBD_CDC_CONSOLE_DATA_OUT_EP    MYNEWT_VAL(USBD_CDC_CONSOLE_DATA_OUT_EP)
#else
#define USBD_CDC_CONSOLE_DATA_OUT_EP    USBD_CDC_DATA_OUT_EP
#endif

#if defined(MYNEWT_VAL_USBD_CDC_CONSOLE_DATA_IN_EP)
#define USBD_CDC_CONSOLE_DATA_IN_EP     MYNEWT_VAL(USBD_CDC_CONSOLE_DATA_IN_EP)
#else
#define USBD_CDC_CONSOLE_DATA_IN_EP     USBD_CDC_DATA_IN_EP
#endif

#if defined(MYNEWT_VAL_USBD_CDC_CONSOLE_DATA_EP_SIZE)
#define USBD_CDC_CONSOLE_DATA_EP_SIZE   MYNEWT_VAL(USBD_CDC_CONSOLE_DATA_EP_SIZE)
#else
#define USBD_CDC_CONSOLE_DATA_EP_SIZE   USBD_CDC_DATA_EP_SIZE
#endif

/*
 * If CDC_HCI does not have specific values for endpoint configuration,
 * use values for unspecified CDC
 */
#if defined(MYNEWT_VAL_USBD_CDC_HCI_NOTIFY_EP)
#define USBD_CDC_HCI_NOTIFY_EP      MYNEWT_VAL(USBD_CDC_HCI_NOTIFY_EP)
#else
#define USBD_CDC_HCI_NOTIFY_EP      USBD_BTH_EVENT_EP
#endif

#if defined(MYNEWT_VAL_USBD_CDC_HCI_NOTIFY_EP_SIZE)
#define USBD_CDC_HCI_NOTIFY_EP_SIZE MYNEWT_VAL(USBD_CDC_HCI_NOTIFY_EP_SIZE)
#else
#define USBD_CDC_HCI_NOTIFY_EP_SIZE USBD_CDC_NOTIFY_EP_SIZE
#endif

#if defined(MYNEWT_VAL_USBD_CDC_HCI_DATA_OUT_EP)
#define USBD_CDC_HCI_DATA_OUT_EP    MYNEWT_VAL(USBD_CDC_HCI_DATA_OUT_EP)
#else
#define USBD_CDC_HCI_DATA_OUT_EP    USBD_BTH_DATA_OUT_EP
#endif

#if defined(MYNEWT_VAL_USBD_CDC_HCI_DATA_IN_EP)
#define USBD_CDC_HCI_DATA_IN_EP     MYNEWT_VAL(USBD_CDC_HCI_DATA_IN_EP)
#else
#define USBD_CDC_HCI_DATA_IN_EP     USBD_BTH_DATA_IN_EP
#endif

#if defined(MYNEWT_VAL_USBD_CDC_HCI_DATA_EP_SIZE)
#define USBD_CDC_HCI_DATA_EP_SIZE   MYNEWT_VAL(USBD_CDC_HCI_DATA_EP_SIZE)
#else
#define USBD_CDC_HCI_DATA_EP_SIZE   USBD_CDC_DATA_EP_SIZE
#endif


#if MYNEWT_VAL(USBD_CDC)
#define CFG_CDC                  MYNEWT_VAL(USBD_CDC)
#else
#define CFG_CDC                  0
#endif

#if MYNEWT_VAL(CONSOLE_USB)
#define CFG_CDC_CONSOLE          MYNEWT_VAL(CONSOLE_USB)
#else
#define CFG_CDC_CONSOLE          0
#endif

#if MYNEWT_VAL(USBD_CDC_HCI)
#define CFG_CDC_HCI              MYNEWT_VAL(USBD_CDC_HCI)
#else
#define CFG_CDC_HCI              0
#endif

#define CFG_TUD_CDC              ((CFG_CDC) + (CFG_CDC_CONSOLE) + (CFG_CDC_HCI))

#if MYNEWT_VAL(USBD_HID)
#define CFG_TUD_HID              MYNEWT_VAL(USBD_HID)
#else
#define CFG_TUD_HID              0
#endif
#if MYNEWT_VAL(USBD_MSC)
#define CFG_TUD_MSC              MYNEWT_VAL(USBD_MSC)
#else
#define CFG_TUD_MSC              0
#endif
#define CFG_TUD_MIDI             0
#define CFG_TUD_VENDOR           0
#define CFG_TUD_USBTMC           0
#if MYNEWT_VAL(USBD_DFU)
#define CFG_TUD_DFU              MYNEWT_VAL(USBD_DFU)
#else
#define CFG_TUD_DFU              0
#endif
#if MYNEWT_VAL(USBD_DFU_RUNTIME)
#define CFG_TUD_DFU_RUNTIME      MYNEWT_VAL(USBD_DFU_RUNTIME)
#else
#define CFG_TUD_DFU_RUNTIME      0
#endif
#if MYNEWT_VAL(USBD_BTH)
#define CFG_TUD_BTH              MYNEWT_VAL(USBD_BTH)
#else
#define CFG_TUD_BTH              0
#endif

/* Minimal number for alternative interfaces that is recognized by Windows as Bluetooth radio controller */
#define CFG_TUD_BTH_ISO_ALT_COUNT 2

#if MYNEWT_VAL(USBD_CDC_RX_BUFSIZE)
#define CFG_TUD_CDC_RX_BUFSIZE   MYNEWT_VAL(USBD_CDC_RX_BUFSIZE)
#else
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#endif

#if MYNEWT_VAL(USBD_CDC_TX_BUFSIZE)
#define CFG_TUD_CDC_TX_BUFSIZE   MYNEWT_VAL(USBD_CDC_TX_BUFSIZE)
#else
#define CFG_TUD_CDC_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#endif

/* HID buffer size Should be sufficient to hold ID (if any) + Data */
#define CFG_TUD_HID_BUFSIZE      16

/* MSC Buffer size of Device Mass storage */
#if MYNEWT_VAL(USBD_MSC_EP_BUFSIZE)
#define CFG_TUD_MSC_EP_BUFSIZE   MYNEWT_VAL(USBD_MSC_EP_BUFSIZE)
#else
#define CFG_TUD_MSC_EP_BUFSIZE   512
#endif

#ifndef CFG_TUD_DFU_XFER_BUFSIZE
#define CFG_TUD_DFU_XFER_BUFSIZE    MYNEWT_VAL(USBD_DFU_BLOCK_SIZE)
#endif

#if MYNEWT_VAL(USBD_DFU_DETACH_TIMEOUT)
#define CFG_TUD_DFU_DETACH_TIMEOUT  MYNEWT_VAL(USBD_DFU_DETACH_TIMEOUT)
#else
#define CFG_TUD_DFU_DETACH_TIMEOUT  1000
#endif

/* ------------- Audio (USB Audio Class 2.0) ------------- */
#if MYNEWT_VAL_CHOICE(USBD_AUDIO, IN)
#define CFG_TUD_AUDIO_ENABLE_EP_IN      1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT     0
#elif MYNEWT_VAL_CHOICE(USBD_AUDIO, OUT)
#define CFG_TUD_AUDIO_ENABLE_EP_IN      0
#define CFG_TUD_AUDIO_ENABLE_EP_OUT     1
#elif MYNEWT_VAL_CHOICE(USBD_AUDIO, IN_OUT)
#define CFG_TUD_AUDIO_ENABLE_EP_IN      1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT     1
#else
#define CFG_TUD_AUDIO_ENABLE_EP_IN      0
#define CFG_TUD_AUDIO_ENABLE_EP_OUT     0
#endif
#define CFG_TUD_AUDIO \
    (CFG_TUD_AUDIO_ENABLE_EP_IN || CFG_TUD_AUDIO_ENABLE_EP_OUT)

#if CFG_TUD_AUDIO
#define USBD_AUDIO_SAMPLE_RATE          MYNEWT_VAL(USBD_AUDIO_SAMPLE_RATE)
/* Valid bits in sample (bit resolution) */
#define USBD_AUDIO_SAMPLE_SIZE          MYNEWT_VAL(USBD_AUDIO_SAMPLE_SIZE)
/* Bytes used by sample in USB packet (subslot size) */
#if MYNEWT_VAL(USBD_AUDIO_BYTES_PER_SAMPLE)
#define USBD_AUDIO_BYTES_PER_SAMPLE     MYNEWT_VAL(USBD_AUDIO_BYTES_PER_SAMPLE)
#else
#define USBD_AUDIO_BYTES_PER_SAMPLE     ((USBD_AUDIO_SAMPLE_SIZE + 7) / 8)
#endif

#define USBD_AUDIO_IN_CHANNELS          MYNEWT_VAL(USBD_AUDIO_IN_CHANNELS)
#define USBD_AUDIO_OUT_CHANNELS         MYNEWT_VAL(USBD_AUDIO_OUT_CHANNELS)

/* Endpoint addresses, isochronous endpoints on nRF5x are fixed to 8 */
#if MYNEWT_VAL(USBD_AUDIO_OUT_EP)
#define USBD_AUDIO_OUT_EP               MYNEWT_VAL(USBD_AUDIO_OUT_EP)
#elif CFG_TUSB_MCU == OPT_MCU_NRF5X
#define USBD_AUDIO_OUT_EP               8
#else
#define USBD_AUDIO_OUT_EP               1
#endif

#if MYNEWT_VAL(USBD_AUDIO_IN_EP)
#define USBD_AUDIO_IN_EP                (0x80 | MYNEWT_VAL(USBD_AUDIO_IN_EP))
#elif CFG_TUSB_MCU == OPT_MCU_NRF5X
#define USBD_AUDIO_IN_EP                (0x80 | 8)
#else
#define USBD_AUDIO_IN_EP                (0x80 | 1)
#endif

#if MYNEWT_VAL(USBD_AUDIO_FEEDBACK_EP)
#define USBD_AUDIO_FEEDBACK_EP          \
    (0x80 | MYNEWT_VAL(USBD_AUDIO_FEEDBACK_EP))
#elif CFG_TUSB_MCU == OPT_MCU_NRF5X
#define USBD_AUDIO_FEEDBACK_EP          (0x80 | 8)
#else
#define USBD_AUDIO_FEEDBACK_EP          (0x80 | 2)
#endif

#if MYNEWT_VAL(USBD_AUDIO_FEEDBACK) && CFG_TUD_AUDIO_ENABLE_EP_OUT
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP    1
#else
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP    0
#endif

#if MYNEWT_VAL(USBD_AUDIO_INT_EP)
#define CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP   1
#define USBD_AUDIO_INT_EP               (0x80 | MYNEWT_VAL(USBD_AUDIO_INT_EP))
#else
#define CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP   0
#endif

/*
 * Endpoint sizes for 1 ms packets (one extra sample for rates that are not
 * multiple of 1 kHz and for asynchronous streams).  Same packet size is used
 * for full and high speed, high speed endpoints use interval of 8
 * microframes.
 * Same formula as TUD_AUDIO_EP_SIZE(false, ...), written out here because
 * TinyUSB evaluates these values in #if before usbd.h is included.
 */
#define USBD_AUDIO_EP_SIZE(_rate, _bytes, _nch) \
    (((((_rate) + 999) / 1000) + 1) * (_bytes) * (_nch))
#define USBD_AUDIO_EP_IN_SZ \
    USBD_AUDIO_EP_SIZE(USBD_AUDIO_SAMPLE_RATE, USBD_AUDIO_BYTES_PER_SAMPLE, \
                       USBD_AUDIO_IN_CHANNELS)
#define USBD_AUDIO_EP_OUT_SZ \
    USBD_AUDIO_EP_SIZE(USBD_AUDIO_SAMPLE_RATE, USBD_AUDIO_BYTES_PER_SAMPLE, \
                       USBD_AUDIO_OUT_CHANNELS)

#if CFG_TUD_AUDIO_ENABLE_EP_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX       USBD_AUDIO_EP_IN_SZ
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ \
    (MYNEWT_VAL(USBD_AUDIO_IN_BUFFER_PACKETS) * USBD_AUDIO_EP_IN_SZ)
#endif
#if CFG_TUD_AUDIO_ENABLE_EP_OUT
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX      USBD_AUDIO_EP_OUT_SZ
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ \
    (MYNEWT_VAL(USBD_AUDIO_OUT_BUFFER_PACKETS) * USBD_AUDIO_EP_OUT_SZ)
#endif
#endif /* CFG_TUD_AUDIO */

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
