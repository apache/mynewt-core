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

#if MYNEWT_VAL(DA146XX_USB_MONITOR)

#ifdef MYNEWT_VAL_DA146XX_USB_MONITOR_EVQ
struct os_eventq MYNEWT_VAL(DA146XX_USB_MONITOR_EVQ);
#endif

#include <da146xx_usb_monitor/da146xx_usb_monitor.h>
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

#if MYNEWT_VAL(DA146XX_USB_MONITOR)
static void
da1469x_da146xx_usb_monitor_cb(bool connected)
{
    /* Trigger vbus change if not using VBUS detection
     * or if using custom VBUS handling
     */
    if (MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, ignore) ||
        MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, custom)) {
        tinyusb_vbus_changed(connected);
    }
}
#endif /* MYNEWT_VAL(DA146XX_USB_MONITOR) */

static void
USBD_IRQHandler(void)
{
    /* Let TinyUSB handle its interrupts first */
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
    } else if (MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, ignore) ||
               MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, custom)) {
        /* If VBUS handling is ignored notify TinyUSB that VBUS is on */
#if MYNEWT_VAL(DA146XX_USB_MONITOR)
        /* With USB Monitor enabled, don't immediately notify VBUS on
         * Wait for actual USB activity (Setup packet) to be detected
         * by USB Monitor before notifying TinyUSB of VBUS presence.
         */
        tinyusb_vbus_changed(tinyusb_da146xx_is_connected());
#else
        /* Without USB Monitor, just assume VBUS is present */
        if (MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, ignore)) {
            tinyusb_vbus_changed(true);
        }
#endif
    }

#if MYNEWT_VAL(DA146XX_USB_MONITOR)
    /* Register callback for connection events */
    da146xx_usb_monitor_register_cb(da1469x_da146xx_usb_monitor_cb);

    /* Initialize USB monitor for USB state monitoring */
    da146xx_usb_monitor_init();
#endif
}

/*
 * Public API functions - map to da146xx_usb_monitor functions
 */
#if MYNEWT_VAL(DA146XX_USB_MONITOR)

void
tinyusb_da146xx_usb_monitor_register_cb(void (*cb)(bool connected))
{
    da146xx_usb_monitor_register_cb(cb);
}

bool
tinyusb_da146xx_is_connected(void)
{
    bool usb_active;

    /* Check USB is active/connected */
    usb_active = da146xx_usb_monitor_is_connected();

    /* If using auto or custom VBUS handling, also check VBUS */
    if (MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, auto) ||
        MYNEWT_VAL_CHOICE(DA1469X_USB_VBUS_HANDLING, custom)) {
        return g_vbus_present || usb_active;
    }

    /* If ignoring VBUS, rely only on USB state */
    return usb_active;
}

#endif /* MYNEWT_VAL(DA146XX_USB_MONITOR) */
