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

#include <mcu/stm32f1_bsp.h>
#include <mcu/stm32f1xx_mynewt_hal.h>
#include <bsp/stm32f1xx_hal_conf.h>

static void
USB_IRQHandler(void)
{
    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    /* TinyUSB provides interrupt handler, here it is setup the mynewt way */
    NVIC_SetVector(USB_HP_CAN1_TX_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_HP_CAN1_TX_IRQn, 2);
    NVIC_SetVector(USB_LP_CAN1_RX0_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 2);
    NVIC_SetVector(USBWakeUp_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USBWakeUp_IRQn, 2);

    /*
     * USB Pin Init
     * PA11- DM, PA12- DP
     */
    hal_gpio_init_af(MCU_GPIO_PORTA(11), 0, GPIO_NOPULL, GPIO_MODE_AF_PP);

    /*
     * Device needs external PULLUP, following two lines change DP to 0 for a while
     * to indicated device connection.
     */
#if MYNEWT_VAL(USB_DP_HAS_EXTERNAL_PULL_UP)
    hal_gpio_init_out(MCU_GPIO_PORTA(12), 0);
    os_time_delay(1);
#endif
    hal_gpio_init_af(MCU_GPIO_PORTA(12), 0, GPIO_NOPULL, GPIO_MODE_AF_PP);

    /*
     * Enable USB clock
     */
    __HAL_RCC_USB_CLK_ENABLE();
}
