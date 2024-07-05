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

#include <tusb.h>

#include <mcu/stm32f3_bsp.h>
#include <mcu/stm32f3xx_mynewt_hal.h>
#include <bsp/stm32f3xx_hal_conf.h>
#include <stm32f3xx_hal.h>

static void
USB_IRQHandler(void)
{
    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    /*
     * Use remapped interrupt (not shared with CAN).
     * Enable SYSCFG in RCC to remap
     */
    bool syscfg_enabled = __HAL_RCC_GPIOA_IS_CLK_ENABLED();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_REMAPINTERRUPT_USB_ENABLE();
    if (!syscfg_enabled) {
        __HAL_RCC_GPIOA_CLK_DISABLE();
    }

    /* TinyUSB provides interrupt handler, here it is setup the mynewt way */
    NVIC_SetVector(USB_HP_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_HP_IRQn, 2);
    NVIC_SetVector(USB_LP_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_LP_IRQn, 2);
    NVIC_SetVector(USBWakeUp_RMP_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USBWakeUp_RMP_IRQn, 2);

    /*
     * USB Pin Init
     * PA11- DM, PA12- DP
     */
    hal_gpio_init_af(MCU_GPIO_PORTA(11), GPIO_AF14_USB, GPIO_NOPULL, GPIO_MODE_AF_PP);
#if defined(MYNEWT_VAL_USB_DP_PULLUP_CONTROL_PIN) && MYNEWT_VAL(USB_DP_PULLUP_CONTROL_PIN) >= 0
    if (MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_1_disable_0) ||
        MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_input_disable_0)) {
        hal_gpio_init_out(MYNEWT_VAL(USB_DP_PULLUP_CONTROL_PIN), 0);
    } else if (MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_0_disable_1) ||
               MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_input_disable_1)) {
        hal_gpio_init_out(MYNEWT_VAL(USB_DP_PULLUP_CONTROL_PIN), 1);
    } else if (MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_0_disable_input) ||
               MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_1_disable_input)) {
        hal_gpio_deinit(MYNEWT_VAL(USB_DP_PULLUP_CONTROL_PIN));
    }
#else
    hal_gpio_init_out(MCU_GPIO_PORTA(12), 0);
#endif
#if MYNEWT_VAL(OS_SCHEDULING)
    os_time_delay(2);
#else
    os_cputime_delay_usecs(1000);
#endif
#if defined(MYNEWT_VAL_USB_DP_PULLUP_CONTROL_PIN) && MYNEWT_VAL(USB_DP_PULLUP_CONTROL_PIN) >= 0
    if (MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_1_disable_0) ||
        MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_1_disable_input)) {
        hal_gpio_init_out(MYNEWT_VAL(USB_DP_PULLUP_CONTROL_PIN), 1);
    } else if (MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_0_disable_1) ||
               MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_0_disable_input)) {
        hal_gpio_init_out(MYNEWT_VAL(USB_DP_PULLUP_CONTROL_PIN), 0);
    } else if (MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_input_disable_0) ||
               MYNEWT_VAL_CHOICE(USB_DP_PULLUP_CONTROL_PIN_MODE, enable_input_disable_1)) {
        hal_gpio_deinit(MYNEWT_VAL(USB_DP_PULLUP_CONTROL_PIN));
    }
#else
    hal_gpio_init_out(MCU_GPIO_PORTA(12), 0);
#endif
    hal_gpio_init_af(MCU_GPIO_PORTA(12), GPIO_AF14_USB, GPIO_NOPULL, GPIO_MODE_AF_PP);

    /*
     * Enable USB clock
     */
    __HAL_RCC_USB_CLK_ENABLE();
}
