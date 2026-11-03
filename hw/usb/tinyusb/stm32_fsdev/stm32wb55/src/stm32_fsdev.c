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

#include <mcu/stm32wb_bsp.h>
#include <mcu/stm32wbxx_mynewt_hal.h>
#include <bsp/stm32wbxx_hal_conf.h>
#include <stm32wbxx_ll_hsem.h>
#include <stm32wbxx_ll_rcc.h>

/* HSEM semaphore that CPU1 holds while it uses CLK48 (ST hw_conf.h) */
#define STM32WB_HSEM_CLK48_CONFIG 5

static void
USB_IRQHandler(void)
{
    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    HAL_PWREx_EnableVddUSB();

    /* TinyUSB provides interrupt handler, here it is setup the mynewt way */
    NVIC_SetVector(USB_HP_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_HP_IRQn, 2);
    NVIC_SetVector(USB_LP_IRQn, (uint32_t)USB_IRQHandler);
    NVIC_SetPriority(USB_LP_IRQn, 2);

    /*
     * USB Pin Init
     * PA11- DM, PA12- DP
     */
    hal_gpio_init_af(MCU_GPIO_PORTA(11), GPIO_AF10_USB, GPIO_NOPULL, GPIO_MODE_AF_PP);

    /*
     * Device needs external PULLUP, following two lines change DP to 0 for a while
     * to indicated device connection.
     */
#if MYNEWT_VAL(USB_DP_HAS_EXTERNAL_PULL_UP)
    hal_gpio_init_out(MCU_GPIO_PORTA(12), 0);
#if MYNEWT_VAL(BOOT_LOADER)
    os_cputime_delay_usecs(1000);
#else
    os_time_delay(1);
#endif
#endif
    hal_gpio_init_af(MCU_GPIO_PORTA(12), GPIO_AF10_USB, GPIO_NOPULL, GPIO_MODE_AF_PP);

    if (MYNEWT_VAL(STM32_CLOCK_HSI48)) {
        /*
         * CPU2 (wireless stack) may reconfigure or stop CLK48 that it uses
         * for RNG unless CPU1 holds CLK48 config semaphore (AN5289).
         */
        __HAL_RCC_HSEM_CLK_ENABLE();
        LL_HSEM_1StepLock(HSEM, STM32WB_HSEM_CLK48_CONFIG);
        LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_HSI48);
        /*
         * HSI48 is accurate enough for USB only when trimmed by CRS,
         * CRS reset configuration already uses USB SOF for synchronization.
         */
        __HAL_RCC_CRS_CLK_ENABLE();
        CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
    }

    /*
     * Enable USB clock
     */
    __HAL_RCC_USB_CLK_ENABLE();
}
