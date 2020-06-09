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

#include <device/usbd.h>
#include <tusb.h>
#include <hal/hal_gpio.h>

static void
USBD_IRQHandler(void)
{
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

    mcu_gpio_set_pin_function(14, MCU_GPIO_MODE_INPUT, MCU_GPIO_FUNC_USB);
    mcu_gpio_set_pin_function(15, MCU_GPIO_MODE_INPUT, MCU_GPIO_FUNC_USB);
}
