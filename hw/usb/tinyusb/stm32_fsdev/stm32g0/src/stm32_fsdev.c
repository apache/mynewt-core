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

#include <mcu/stm32g0_bsp.h>
#include <mcu/stm32g0xx_mynewt_hal.h>
#include <bsp/stm32g0xx_hal_conf.h>

static void
USB_IRQHandler(void)
{
    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    /* TinyUSB provides interrupt handler, here it is setup the mynewt way */
    NVIC_SetVector(USB_UCPD1_2_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_UCPD1_2_IRQn, 2);

    if (MYNEWT_VAL(STM32_CLOCK_HSI48)) {
        RCC->CCIPR2 = RCC->CCIPR2 & ~RCC_CCIPR2_USBSEL;
    }

    /*
     * Enable USB clock
     */
    __HAL_RCC_USB_CLK_ENABLE();

    /* Enable VDDUSB */
    HAL_PWREx_EnableVddUSB();
}
