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

#include <mcu/stm32_hal.h>

static void
OTG_FS_IRQHandler(void)
{
    /* TinyUSB provides interrupt handler code */
    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    NVIC_SetVector(OTG_FS_IRQn, (uint32_t)OTG_FS_IRQHandler);
    NVIC_SetPriority(OTG_FS_IRQn, 2);

    /*
     * USB Pin Init
     * PA11- DM, PA12- DP
     */
    hal_gpio_init_af(MCU_GPIO_PORTA(11), GPIO_AF10_OTG_FS, GPIO_NOPULL, GPIO_MODE_AF_PP);
#if MYNEWT_VAL(USB_DP_HAS_EXTERNAL_PULL_UP)
    hal_gpio_init_out(MCU_GPIO_PORTA(12), 0);
    os_time_delay(1);
#endif
    hal_gpio_init_af(MCU_GPIO_PORTA(12), GPIO_AF10_OTG_FS, GPIO_NOPULL, GPIO_MODE_AF_PP);

    /*
     * Enable USB OTG clock, force device mode
     */
    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
    USB_OTG_FS->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
    USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

#ifdef USB_OTG_GCCFG_NOVBUSSENS
#if !MYNEWT_VAL(USB_VBUS_DETECTION_ENABLE)
    /* PA9- VUSB not used for USB */
    USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
    USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
    USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;
#else
    USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_NOVBUSSENS;
    USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
    USB_OTG_FS->GCCFG |= ~USB_OTG_GCCFG_VBUSASEN;
    hal_gpio_init_af(MCU_GPIO_PORTA(9), GPIO_AF10_OTG_FS, GPIO_NOPULL, GPIO_MODE_AF_PP);
#endif
#endif
}
