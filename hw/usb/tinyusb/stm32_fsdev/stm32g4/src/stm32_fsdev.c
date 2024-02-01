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

#include <mcu/stm32g4_bsp.h>
#include <mcu/stm32g4xx_mynewt_hal.h>
#include <bsp/stm32g4xx_hal_conf.h>

static void
USB_IRQHandler(void)
{
    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    /* TinyUSB provides interrupt handler, here it is setup the mynewt way */
    NVIC_SetVector(USB_HP_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_HP_IRQn, 2);
    NVIC_SetVector(USB_LP_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_LP_IRQn, 2);
    NVIC_SetVector(USBWakeUp_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USBWakeUp_IRQn, 2);
    NVIC_SetVector(UCPD1_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(UCPD1_IRQn, 2);

    if (MYNEWT_VAL(STM32_CLOCK_HSI48)) {
        RCC->CCIPR = RCC->CCIPR & ~RCC_CCIPR_CLK48SEL;
    }

    /*
     * Enable USB clock
     */
    __HAL_RCC_USB_CLK_ENABLE();

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_CRC_CLK_ENABLE();
    __HAL_RCC_UCPD1_CLK_ENABLE();

    /* Enable DMA for USB PD */
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
}
