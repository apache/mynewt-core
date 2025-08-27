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

#include <os/mynewt.h>
#include <mcu/mcu.h>
#include <mcu/da1469x_vbus.h>
#include <mcu/da1469x_pd.h>

#include <device/usbd.h>

#if MYNEWT_VAL(USB_KEEPALIVE_DETECT)

#ifdef MYNEWT_VAL_USB_KEEPALIVE_EVQ
struct os_eventq MYNEWT_VAL(USB_KEEPALIVE_EVQ);
#endif

#include <keepalive_detect/keepalive_detect.h>

/* USB register bit definitions moved here when keepalive is enabled,
 * this combines position and mask, so an absolute mask can be used
 */
#define USB_MAMSK_REG_USB_M_FRAME_Msk          (0x0001U)
#define USB_ALTMSK_REG_USB_M_SD3_Msk           (0x0008U)
#define USB_ALTMSK_REG_USB_M_RESUME_Msk        (0x0004U)
#define USB_MAEV_REG_USB_FRAME_Msk             (0x0001U)
#define USB_ALTEV_REG_USB_SD3_Msk              (0x0008U)
#define USB_ALTEV_REG_USB_RESUME_Msk           (0x0004U)
#endif

static bool g_vbus_present;

static void
tinyusb_vbus_changed(bool present)
{
    if (g_vbus_present == present) {
        return;
    }
    g_vbus_present = present;

    if (present) {
        da1469x_pd_acquire(MCU_PD_DOMAIN_SYS);
        tusb_vbus_changed(true);
    } else {
        tusb_vbus_changed(false);
        da1469x_pd_release(MCU_PD_DOMAIN_SYS);
    }
}

#if MYNEWT_VAL(USB_KEEPALIVE_DETECT)
/*
 * DA1469x-specific keep-alive callbacks implementation
 */
static uint16_t
da1469x_get_frame_number(void)
{
    uint16_t frame_num;

    frame_num = USB->USB_FNL_REG & 0xFF;
    frame_num |= (USB->USB_FNH_REG & 0x07) << 8;

    return frame_num;
}

static void
da1469x_handle_sof_interrupt(void)
{
    uint32_t maev;

    maev = USB->USB_MAEV_REG;

    if (maev & USB_MAEV_REG_USB_FRAME_Msk) {
        /* Clear the interrupt flag */
        USB->USB_MAEV_REG = USB_MAEV_REG_USB_FRAME_Msk;

        /* Notify keep-alive module */
        usb_keepalive_handle_sof();
    }
}

static void
da1469x_check_suspend(void)
{
    uint32_t altev;

    altev = USB->USB_ALTEV_REG;

    if (altev & USB_ALTEV_REG_USB_SD3_Msk) {
        /* No SOF for 3ms - USB suspended */
        USB->USB_ALTEV_REG = USB_ALTEV_REG_USB_SD3_Msk;
        usb_keepalive_handle_suspend();
    }

    if (altev & USB_ALTEV_REG_USB_RESUME_Msk) {
        /* Resume detected */
        USB->USB_ALTEV_REG = USB_ALTEV_REG_USB_RESUME_Msk;
        usb_keepalive_handle_resume();
    }
}

static void
da1469x_enable_ka_interrupts(void)
{
    /* Enable frame/SOF interrupt */
    USB->USB_MAMSK_REG |= USB_MAMSK_REG_USB_M_FRAME_Msk;

    /* Enable suspend detection */
    USB->USB_ALTMSK_REG |= USB_ALTMSK_REG_USB_M_SD3_Msk |
                           USB_ALTMSK_REG_USB_M_RESUME_Msk;
}

static const struct usb_keepalive_cbs da1469x_keepalive_cbs = {
    .handle_sof_interrupt = da1469x_handle_sof_interrupt,
    .check_suspend = da1469x_check_suspend,
    .enable_interrupts = da1469x_enable_ka_interrupts,
    .get_frame_number = da1469x_get_frame_number,
};

static void
da1469x_keepalive_cb(bool connected)
{
    /* Trigger vbus change if not using VBUS detection */
    if (MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, ignore)) {
        tinyusb_vbus_changed(connected);
    }
}
#endif /* MYNEWT_VAL(USB_KEEPALIVE_DETECT) */

static void
USBD_IRQHandler(void)
{
#if MYNEWT_VAL(USB_KEEPALIVE_DETECT)
    /* Handle SOF/keep-alive interrupts */
    da1469x_handle_sof_interrupt();

    /* Check for suspend (no keep-alives) */
    da1469x_check_suspend();
#endif

    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    /* Setup USB IRQ */
    NVIC_SetVector(USB_IRQn, (uint32_t)USBD_IRQHandler);
    NVIC_SetPriority(USB_IRQn, 2);
    NVIC_EnableIRQ(USB_IRQn);

    /* Use PLL96 / 2 clock not HCLK */
    CRG_TOP->CLK_CTRL_REG &= ~CRG_TOP_CLK_CTRL_REG_USB_CLK_SRC_Msk;

    mcu_gpio_set_pin_function(MCU_PIN_USB_DP, MCU_GPIO_MODE_INPUT, MCU_GPIO_FUNC_USB);
    mcu_gpio_set_pin_function(MCU_PIN_USB_DM, MCU_GPIO_MODE_INPUT, MCU_GPIO_FUNC_USB);

    if (MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, auto)) {
        /* If VBUS handling is set to 'auto' let da1469x_vbus module handle notification */
        da1469x_vbus_add_handler(tinyusb_vbus_changed);
    }
    else if (MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, ignore)) {
        /* If VBUS handling is ignored notify TinyUSB that VBUS is on */
#if MYNEWT_VAL(USB_KEEPALIVE_DETECT)
        /* With keep-alive detection, don't immediately notify VBUS on */
        /* Wait for actual USB activity (SOF) to be detected */
#else
        tinyusb_vbus_changed(true);
#endif
    }

#if MYNEWT_VAL(USB_KEEPALIVE_DETECT)
    /* Register DA1469x-specific callbacks */
    usb_keepalive_register_cbs(&da1469x_keepalive_cbs);

    /* Register callback for connection events */
    usb_keepalive_register_cb(da1469x_keepalive_cb);

    /* Set event queue if specified */
#if MYNEWT_VAL(USB_KEEPALIVE_EVQ)
    usb_keepalive_evq_set(MYNEWT_VAL(USB_KEEPALIVE_EVQ));
#endif

    /* Initialize keep-alive detection */
    usb_keepalive_init();
#endif
}

/*
 * Public API functions - map to keepalive_detect functions
 */
#if MYNEWT_VAL(USB_KEEPALIVE_DETECT)

bool
tinyusb_ka_is_active(void)
{
    return usb_keepalive_is_active();
}

uint32_t
tinyusb_ka_get_sof_cnt(void)
{
    return usb_keepalive_get_sof_cnt();
}

void
tinyusb_ka_register_cb(void (*cb)(bool connected))
{
    usb_keepalive_register_cb(cb);
}

bool
tinyusb_is_connected(void)
{
    bool ka_active;

    /* Check keep-alive detection status */
    ka_active = usb_keepalive_is_active();

    /* If using auto VBUS handling, also check VBUS */
    if (MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, auto)) {
        return g_vbus_present || ka_active;
    }

    /* If ignoring VBUS, rely only on keep-alive */
    return ka_active;
}

#endif /* MYNEWT_VAL(USB_KEEPALIVE_DETECT) */
