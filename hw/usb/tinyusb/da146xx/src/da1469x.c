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

static bool vbus_present;

static void
USBD_IRQHandler(void)
{
    tud_int_handler(0);
}

static void
tinyusb_vbus_changed(bool present)
{
    if (vbus_present == present) {
        return;
    }
    vbus_present = present;

    if (present) {
        da1469x_pd_acquire(MCU_PD_DOMAIN_SYS);
        tusb_vbus_changed(true);
    } else {
        tusb_vbus_changed(false);
        da1469x_pd_release(MCU_PD_DOMAIN_SYS);
    }
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
        tinyusb_vbus_changed(true);
    }
}
