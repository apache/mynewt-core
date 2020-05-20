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

#define CDC_IF_STR_IX (MYNEWT_VAL(USBD_CDC_DESCRIPTOR_STRING) == NULL ? 0 : 4)
#define MSC_IF_STR_IX (MYNEWT_VAL(USBD_MSC_DESCRIPTOR_STRING) == NULL ? 0 : 5)
#define HID_IF_STR_IX (MYNEWT_VAL(USBD_HID_DESCRIPTOR_STRING) == NULL ? 0 : 6)
#define BTH_IF_STR_IX (MYNEWT_VAL(USBD_BTH_DESCRIPTOR_STRING) == NULL ? 0 : 7)


#if CFG_TUD_HID

#if MYNEWT_VAL(USBD_HID_REPORT_ID_KEYBOARD) || MYNEWT_VAL(USBD_HID_REPORT_ID_MOUSE)
const uint8_t desc_hid_report[] = {
#if MYNEWT_VAL(USBD_HID_REPORT_ID_KEYBOARD)
    TUD_HID_REPORT_DESC_KEYBOARD(MYNEWT_VAL(USBD_HID_REPORT_ID_KEYBOARD),),
#endif
#if MYNEWT_VAL(USBD_HID_REPORT_ID_MOUSE)
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE),)
#endif
};
#else
#error Please specify keybaord and/or mouse report id
#endif

/*
 * Invoked when received GET HID REPORT DESCRIPTOR
 * Application return pointer to descriptor
 * Descriptor contents must exist long enough for transfer to complete
 */
const uint8_t *
tud_hid_descriptor_report_cb(void)
{
    return desc_hid_report;
}

/*
 * Invoked when received GET_REPORT control request
 * Application must fill buffer report's content and return its length.
 * Return zero will cause the stack to STALL request
 */
uint16_t
tud_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    /* TODO: not implemented yet */
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
tud_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type, const uint8_t *report, uint16_t report_size)
{
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

const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

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

    .idVendor           = MYNEWT_VAL(USBD_VID),
    .idProduct          = MYNEWT_VAL(USBD_PID),
    .bcdDevice          = USBD_PRODUCT_RELEASE_NUMBER,

    .iManufacturer      = 0x02,
    .iProduct           = 0x03,
    .iSerialNumber      = 0x01,

    .bNumConfigurations = 0x01
};

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

#if CFG_TUD_CDC
    ITF_NUM_CDC,
    ITF_NUM_CDC_DATA,
#endif

#if CFG_TUD_MSC
    ITF_NUM_MSC,
#endif

#if CFG_TUD_HID
    ITF_NUM_HID,
#endif

    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + \
                             CFG_TUD_CDC * TUD_CDC_DESC_LEN + \
                             CFG_TUD_MSC * TUD_MSC_DESC_LEN + \
                             CFG_TUD_HID * TUD_HID_DESC_LEN + \
                             CFG_TUD_BTH * TUD_BTH_DESC_LEN + \
                             0)

const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(CONFIG_NUM, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
                          MYNEWT_VAL(USBD_CONFIGURATION_MAX_POWER)),

#if CFG_TUD_BTH
    TUD_BTH_DESCRIPTOR(ITF_NUM_BTH, BTH_IF_STR_IX, USBD_BTH_EVENT_EP, USBD_BTH_EVENT_EP_SIZE,
                       USBD_BTH_EVENT_EP_INTERVAL, USBD_BTH_DATA_IN_EP, USBD_BTH_DATA_OUT_EP, USBD_BTH_DATA_EP_SIZE,
                       0, 9, 17, 25, 33, 49),
#endif

#if CFG_TUD_CDC
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, CDC_IF_STR_IX, USBD_CDC_NOTIFY_EP, USBD_CDC_NOTIFY_EP_SIZE,
                       USBD_CDC_DATA_OUT_EP, USBD_CDC_DATA_IN_EP, USBD_CDC_DATA_EP_SIZE),
#endif

#if CFG_TUD_MSC
    /* TODO: MSC not handled yet */
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, MSC_IF_STR_IX, EPNUM_MSC_OUT, EPNUM_MSC_IN,
                       (CFG_TUSB_RHPORT0_MODE & OPT_MODE_HIGH_SPEED) ? 512 : 64),
#endif

#if CFG_TUD_HID
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, HID_IF_STR_IX, HID_PROTOCOL_NONE, sizeof(desc_hid_report),
                       USBD_HID_REPORT_EP, USBD_HID_REPORT_EP_SIZE, USBD_HID_REPORT_EP_INTERVAL),
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
    MYNEWT_VAL(USBD_CDC_DESCRIPTOR_STRING),
    MYNEWT_VAL(USBD_MSC_DESCRIPTOR_STRING),
    MYNEWT_VAL(USBD_HID_DESCRIPTOR_STRING),
    MYNEWT_VAL(USBD_BTH_DESCRIPTOR_STRING),
};

static uint16_t desc_string[MYNEWT_VAL(USBD_STRING_DESCRIPTOR_MAX_LENGTH) + 1];

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

    if (index == 0) {
        desc_string[1] = MYNEWT_VAL(USBD_LANGID);
        char_num = 1;
    } else if (index == 1) {
        /* TODO: Add function call to get serial number */
        desc_string[1] = '1';
        char_num = 1;
    } else if (index - 2 < ARRAY_SIZE(string_desc_arr)) {
        str = string_desc_arr[index - 2];

        char_num = strlen(str);
        if (char_num >= ARRAY_SIZE(desc_string)) {
            char_num = ARRAY_SIZE(desc_string);
        }

        for (i = 0; i < char_num; ++i) {
            desc_string[1 + i] = str[i];
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
