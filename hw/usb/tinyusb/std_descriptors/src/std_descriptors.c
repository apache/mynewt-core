/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

#include <assert.h>
#include <syscfg/syscfg.h>
#include <bsp/bsp.h>
#include <string.h>
#include <tusb.h>
#include <device/usbd.h>
#include <os/util.h>
#include <console/console.h>
#include <hal/hal_gpio.h>

#define USBD_PRODUCT_RELEASE_NUMBER MYNEWT_VAL(USBD_PRODUCT_RELEASE_NUMBER)

#ifndef CONFIG_NUM
#define CONFIG_NUM 1
#endif

enum usb_desc_ix {
    USB_DESC_IX_SERIAL_NUMBER   = 1,
    USB_DESC_IX_VENDOR          = 2,
    USB_DESC_IX_PRODUCT         = 3,
#if defined MYNEWT_VAL_USBD_CDC_DESCRIPTOR_STRING
    CDC_IF_STR_IX,
#else
#define CDC_IF_STR_IX 0
#endif
#if defined MYNEWT_VAL_USBD_CDC_CONSOLE_DESCRIPTOR_STRING
    CDC_CONSOLE_IF_STR_IX,
#else
#define CDC_CONSOLE_IF_STR_IX   0
#endif
#if defined MYNEWT_VAL_USBD_CDC_HCI_DESCRIPTOR_STRING
    CDC_HCI_IF_STR_IX,
#else
#define CDC_HCI_IF_STR_IX   0
#endif
#if defined MYNEWT_VAL_USBD_MSC_DESCRIPTOR_STRING
    MSC_IF_STR_IX,
#else
#define MSC_IF_STR_IX   0
#endif
#if defined MYNEWT_VAL_USBD_HID_DESCRIPTOR_STRING
    HID_IF_STR_IX,
#else
#define HID_IF_STR_IX   0
#endif
#if defined MYNEWT_VAL_USBD_BTH_DESCRIPTOR_STRING
    BTH_IF_STR_IX,
#else
#define BTH_IF_STR_IX   0
#endif
#if defined MYNEWT_VAL_USBD_DFU_SLOT_NAME
    DFU_SLOT_NAME_IF_STR_IX,
#endif
};

#if MYNEWT_VAL(USBD_CONFIGURATION_SELF_POWERED)
#define SELF_POWERED_OPT    TUSB_DESC_CONFIG_ATT_SELF_POWERED
#else
#define SELF_POWERED_OPT    0
#endif

#if MYNEWT_VAL(USBD_CONFIGURATION_REMOTE_WAKEUP)
#define REMOTE_WAKEUP_OPT   TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP
#else
#define REMOTE_WAKEUP_OPT   0
#endif

#define CONFIG_ATT          (SELF_POWERED_OPT | REMOTE_WAKEUP_OPT)

#if CFG_TUD_HID

#if MYNEWT_VAL(USBD_HID_REPORT_ID_KEYBOARD) || MYNEWT_VAL(USBD_HID_REPORT_ID_MOUSE)
const uint8_t desc_hid_report[] = {
#if MYNEWT_VAL(USBD_HID_REPORT_ID_KEYBOARD)
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(MYNEWT_VAL(USBD_HID_REPORT_ID_KEYBOARD))),
#endif
#if MYNEWT_VAL(USBD_HID_REPORT_ID_MOUSE)
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(MYNEWT_VAL(USBD_HID_REPORT_ID_MOUSE)))
#endif
};
#else
#error Please specify keybaord and/or mouse report id with a non-zero value
#endif

/*
 * Invoked when received GET HID REPORT DESCRIPTOR
 * Application return pointer to descriptor
 * Descriptor contents must exist long enough for transfer to complete
 */
const uint8_t *
tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return desc_hid_report;
}

/*
 * Invoked when received GET_REPORT control request
 * Application must fill buffer report's content and return its length.
 * Return zero will cause the stack to STALL request
 */
uint16_t
tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    /* TODO: not implemented yet */
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}

/*
 * Invoked when received SET_REPORT control request or
 * received data on OUT endpoint ( Report ID = 0, Type = 0 )
 */
void
tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, const uint8_t *report,
                      uint16_t report_size)
{
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)report_size;
    if (MYNEWT_VAL(USBD_HID_CAPS_LOCK_LED_PIN) >= 0) {
        hal_gpio_write(MYNEWT_VAL(USBD_HID_CAPS_LOCK_LED_PIN),
                       1 & ((report[0] >> 1) ^ 1u ^ MYNEWT_VAL(USBD_HID_CAPS_LOCK_LED_ON_VALUE)));
    }
    if (MYNEWT_VAL(USBD_HID_NUM_LOCK_LED_PIN) >= 0) {
        hal_gpio_write(MYNEWT_VAL(USBD_HID_NUM_LOCK_LED_PIN),
                       1 & (report[0] ^ 1u ^ MYNEWT_VAL(USBD_HID_CAPS_LOCK_LED_ON_VALUE)));
    }
}
#endif /* CFG_TUD_HID */

#define USB_BCD     tu_htole16(0x200)

const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

    .bDeviceClass       = MYNEWT_VAL(USBD_DEVICE_CLASS),
    .bDeviceSubClass    = MYNEWT_VAL(USBD_DEVICE_SUBCLASS),
    .bDeviceProtocol    = MYNEWT_VAL(USBD_DEVICE_PROTOCOL),

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = MYNEWT_VAL(USBD_VID),
    .idProduct          = MYNEWT_VAL(USBD_PID),
    .bcdDevice          = USBD_PRODUCT_RELEASE_NUMBER,

    .iManufacturer      = 0x02,
    .iProduct           = 0x03,
    .iSerialNumber      = 0x01,

    .bNumConfigurations = 0x01
};

#if TUD_OPT_HIGH_SPEED

/*
 * device qualifier is mostly similar to device descriptor since we don't change configuration based on speed.
 */
tusb_desc_device_qualifier_t const desc_device_qualifier = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

#if CFG_TUD_BTH
    .bDeviceClass       = TUD_BT_APP_CLASS,
    .bDeviceSubClass    = TUD_BT_APP_SUBCLASS,
    .bDeviceProtocol    = TUD_BT_PROTOCOL_PRIMARY_CONTROLLER,
#elif CFG_TUD_CDC
    /*
     * Use Interface Association Descriptor (IAD) for CDC
     * As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
     */
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
#else
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
#endif

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved          = 0x00,
};

/*
 * Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
 * Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
 * device_qualifier descriptor describes information about a high-speed capable device that would
 * change if the device were operating at the other speed. If not highspeed capable stall this request.
 */
const uint8_t *
tud_descriptor_device_qualifier_cb(void)
{
    return (const uint8_t *)&desc_device_qualifier;
}

#endif

/*
 * Invoked when received GET DEVICE DESCRIPTOR
 * Application return pointer to descriptor
 */
const uint8_t *
tud_descriptor_device_cb(void)
{
    return (const uint8_t *)&desc_device;
}

/*
 * Configuration Descriptor
 */

enum {
#if CFG_TUD_BTH
    ITF_NUM_BTH,
#if CFG_TUD_BTH_ISO_ALT_COUNT > 0
    ITF_NUM_BTH_VOICE,
#endif
#endif

#if CFG_CDC
    ITF_NUM_CDC,
    ITF_NUM_CDC_DATA,
#endif

#if CFG_CDC_CONSOLE
    ITF_NUM_CDC_CONSOLE,
    ITF_NUM_CDC_CONSOLE_DATA,
#endif

#if CFG_CDC_HCI
    ITF_NUM_CDC_HCI,
    ITF_NUM_CDC_HCI_DATA,
#endif

#if CFG_TUD_MSC
    ITF_NUM_MSC,
#endif

#if CFG_TUD_HID
    ITF_NUM_HID,
#endif

#if CFG_TUD_DFU
    ITF_NUM_DFU,
#endif

    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + \
                             CFG_CDC * TUD_CDC_DESC_LEN + \
                             CFG_CDC_CONSOLE * TUD_CDC_DESC_LEN + \
                             CFG_CDC_HCI * TUD_CDC_DESC_LEN + \
                             CFG_TUD_MSC * TUD_MSC_DESC_LEN + \
                             CFG_TUD_HID * TUD_HID_DESC_LEN + \
                             CFG_TUD_BTH * TUD_BTH_DESC_LEN + \
                             CFG_TUD_DFU * TUD_DFU_DESC_LEN(1) + \
                             0)

const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(CONFIG_NUM, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, CONFIG_ATT,
                          MYNEWT_VAL(USBD_CONFIGURATION_MAX_POWER)),

#if CFG_TUD_BTH
    TUD_BTH_DESCRIPTOR(ITF_NUM_BTH, BTH_IF_STR_IX, USBD_BTH_EVENT_EP, USBD_BTH_EVENT_EP_SIZE,
                       USBD_BTH_EVENT_EP_INTERVAL, USBD_BTH_DATA_IN_EP, USBD_BTH_DATA_OUT_EP,
                       (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? 512 : USBD_BTH_DATA_EP_SIZE,
                       0, 9, 17, 25, 33, 49),
#endif

#if CFG_CDC_CONSOLE
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_CONSOLE, CDC_CONSOLE_IF_STR_IX, USBD_CDC_CONSOLE_NOTIFY_EP,
                       USBD_CDC_CONSOLE_NOTIFY_EP_SIZE, USBD_CDC_CONSOLE_DATA_OUT_EP, USBD_CDC_CONSOLE_DATA_IN_EP,
                       (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? 512 : USBD_CDC_CONSOLE_DATA_EP_SIZE),
#endif

#if CFG_CDC_HCI
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_HCI, CDC_HCI_IF_STR_IX, USBD_CDC_HCI_NOTIFY_EP, USBD_CDC_HCI_NOTIFY_EP_SIZE,
                       USBD_CDC_HCI_DATA_OUT_EP, USBD_CDC_HCI_DATA_IN_EP,
                       (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? 512 : USBD_CDC_HCI_DATA_EP_SIZE),
#endif

#if CFG_CDC
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, CDC_IF_STR_IX, USBD_CDC_NOTIFY_EP, USBD_CDC_NOTIFY_EP_SIZE,
                       USBD_CDC_DATA_OUT_EP, USBD_CDC_DATA_IN_EP,
                       (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? 512 : USBD_CDC_DATA_EP_SIZE),
#endif

#if CFG_TUD_MSC
    /* TODO: MSC not handled yet */
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, MSC_IF_STR_IX, USBD_MSC_DATA_OUT_EP, USBD_MSC_DATA_IN_EP,
                       (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? 512 : 64),
#endif

#if CFG_TUD_HID
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, HID_IF_STR_IX, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report),
                       USBD_HID_REPORT_EP, USBD_HID_REPORT_EP_SIZE, USBD_HID_REPORT_EP_INTERVAL),
#endif

#if CFG_TUD_DFU
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, 1, DFU_SLOT_NAME_IF_STR_IX, DFU_ATTR_CAN_DOWNLOAD,
                       CFG_TUD_DFU_DETACH_TIMEOUT, CFG_TUD_DFU_XFER_BUFSIZE),
#endif
};

/**
 * Invoked when received GET CONFIGURATION DESCRIPTOR
 * Application return pointer to descriptor
 * Descriptor contents must exist long enough for transfer to complete
 */
const uint8_t *
tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;

    return desc_configuration;
}

const char *string_desc_arr[] = {
    MYNEWT_VAL(USBD_VENDOR_STRING),
    MYNEWT_VAL(USBD_PRODUCT_STRING),
#if defined MYNEWT_VAL_USBD_CDC_DESCRIPTOR_STRING
    MYNEWT_VAL(USBD_CDC_DESCRIPTOR_STRING),
#endif
#if defined MYNEWT_VAL_USBD_CDC_CONSOLE_DESCRIPTOR_STRING
    MYNEWT_VAL(USBD_CDC_CONSOLE_DESCRIPTOR_STRING),
#endif
#if defined MYNEWT_VAL_USBD_CDC_HCI_DESCRIPTOR_STRING
    MYNEWT_VAL(USBD_CDC_HCI_DESCRIPTOR_STRING),
#endif
#if defined MYNEWT_VAL_USBD_MSC_DESCRIPTOR_STRING
    MYNEWT_VAL(USBD_MSC_DESCRIPTOR_STRING),
#endif
#if defined MYNEWT_VAL_USBD_HID_DESCRIPTOR_STRING
    MYNEWT_VAL(USBD_HID_DESCRIPTOR_STRING),
#endif
#if defined MYNEWT_VAL_USBD_BTH_DESCRIPTOR_STRING
    MYNEWT_VAL(USBD_BTH_DESCRIPTOR_STRING),
#endif
#if defined MYNEWT_VAL_USBD_DFU_SLOT_NAME
    MYNEWT_VAL(USBD_DFU_SLOT_NAME),
#endif
};

static uint16_t desc_string[MYNEWT_VAL(USBD_STRING_DESCRIPTOR_MAX_LENGTH) + 1];

#if MYNEWT_VAL(USBD_WINDOWS_COMP_ID)

#define MICROSOFT_OS_STRING_DESCRIPTOR  0xEE
#define COMPATIBILITY_FEATURE_REQUEST   0xFE

static const uint8_t microsoft_os_string_descriptor[] = {
    0x12,                           /* BYTE     Descriptor length (18 bytes) */
    0x03,                           /* BYTE     Descriptor type (3 = String) */
    0x4D, 0x00, 0x53, 0x00,         /* 7 WORDS Unicode String (LE)  Signature: "MSFT100" */
    0x46, 0x00, 0x54, 0x00,
    0x31, 0x00, 0x30, 0x00,
    0x30, 0x00,
    COMPATIBILITY_FEATURE_REQUEST,  /* BYTE Vendor Code */
    0x00                            /* BYTE Padding */
};

struct {
    uint32_t len;
    uint16_t version;
    uint16_t four;
    uint8_t number_of_sections;
    uint8_t reserved1[7];
    uint8_t itf;
    uint8_t reserved2;
    uint8_t compatible_id[8];
    uint8_t sub_compatible_id[8];
    uint8_t reserved3[6];
} static windows_compat_id = {
    .len = tu_htole32(40),
    .version = tu_htole16(0x100),
    .four = tu_htole16(4),
    .number_of_sections = 1,
    .reserved2 = 1,
    .itf = MYNEWT_VAL(USBD_WINDOWS_COMP_INTERFACE),
    .compatible_id = MYNEWT_VAL(USBD_WINDOWS_COMP_ID_STRING),
};

bool
tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request)
{
    if (request->wIndex == 0x04 && request->bRequest == COMPATIBILITY_FEATURE_REQUEST) {
        if (stage == CONTROL_STAGE_SETUP) {
            return tud_control_xfer(rhport, request, (void *)&windows_compat_id, 40);
        } else {
            return true;
        }
    }
    return false;
}
#endif

/*
 * Invoked when received GET STRING DESCRIPTOR request
 * Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
 */
const uint16_t *
tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    int char_num = 0;
    int i;
    const char *str;

#if MYNEWT_VAL(USBD_WINDOWS_COMP_ID)
    if (index == MICROSOFT_OS_STRING_DESCRIPTOR) {
        return (const uint16_t *)microsoft_os_string_descriptor;
    }
#endif
    if (index == 0) {
        desc_string[1] = MYNEWT_VAL(USBD_LANGID);
        char_num = 1;
    } else if (index == 1) {
        /* TODO: Add function call to get serial number */
        desc_string[1] = '1';
        char_num = 1;
    } else if (index - 2 < ARRAY_SIZE(string_desc_arr)) {
        str = string_desc_arr[index - 2];

        if (str) {
            char_num = strlen(str);
            assert(char_num <= ARRAY_SIZE(desc_string) - 1);
            if (char_num > ARRAY_SIZE(desc_string) - 1) {
                char_num = ARRAY_SIZE(desc_string) - 1;
            }

            for (i = 0; i < char_num; ++i) {
                desc_string[1 + i] = str[i];
            }
        }
    }

    if (char_num) {
        /* Encode length in first byte */
        desc_string[0] = (TUSB_DESC_STRING << 8) | (2 * char_num + 2);
        return desc_string;
    } else {
        return NULL;
    }
}
