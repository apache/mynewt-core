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

#if defined(USB_OTG_FS)
#if defined(STM32U5)
#define GPIO_AF_USB GPIO_AF10_USB
#else
#define GPIO_AF_USB GPIO_AF10_OTG_FS
#endif

static void
OTG_FS_IRQHandler(void)
{
    /* TinyUSB provides interrupt handler code */
    tud_int_handler(0);
}
#define USB_OTG                 USB_OTG_FS
#define USB_OTG_CLK_ENABLE      __HAL_RCC_USB_OTG_FS_CLK_ENABLE
#elif defined(USB_OTG_HS)
static void
OTG_HS_IRQHandler(void)
{
    /* TinyUSB provides interrupt handler code */
    tud_int_handler(0);
}
#define USB_OTG                 USB_OTG_HS
#define USB_OTG_CLK_ENABLE      __HAL_RCC_USB_OTG_HS_CLK_ENABLE
#define GPIO_AF10_OTG_FS        GPIO_AF10_OTG1_FS
#define GPIO_AF_USB             GPIO_AF10_OTG_FS
#endif

void
tinyusb_hardware_init(void)
{
#if defined(USB_OTG_FS)
    NVIC_SetVector(OTG_FS_IRQn, (uint32_t)OTG_FS_IRQHandler);
    NVIC_SetPriority(OTG_FS_IRQn, 2);
#elif defined(USB_OTG_HS)
    NVIC_SetVector(OTG_HS_IRQn, (uint32_t)OTG_HS_IRQHandler);
    NVIC_SetPriority(OTG_HS_IRQn, 2);
#endif
    /*
     * USB Pin Init
     * PA11- DM, PA12- DP
     */
    hal_gpio_init_af(MCU_GPIO_PORTA(11), GPIO_AF_USB, GPIO_NOPULL, GPIO_MODE_AF_PP);
#if MYNEWT_VAL(USB_DP_HAS_EXTERNAL_PULL_UP)
    hal_gpio_init_out(MCU_GPIO_PORTA(12), 0);
#if MYNEWT_VAL(BOOT_LOADER)
    os_cputime_delay_usecs(1000);
#else
    os_time_delay(1);
#endif
#endif
    hal_gpio_init_af(MCU_GPIO_PORTA(12), GPIO_AF_USB, GPIO_NOPULL, GPIO_MODE_AF_PP);

#if MYNEWT_VAL(MCU_STM32H7)
    HAL_PWREx_EnableUSBVoltageDetector();
#endif
    /*
     * Enable USB OTG clock, force device mode
     */
    USB_OTG_CLK_ENABLE();

#if MYNEWT_VAL(USB_ID_PIN_ENABLE)
    hal_gpio_init_af(MCU_GPIO_PORTA(10), GPIO_AF_USB, GPIO_PULLUP, GPIO_MODE_AF_OD);
#else
    USB_OTG->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
    USB_OTG->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;
#endif

#ifdef USB_OTG_GCCFG_NOVBUSSENS
#if !MYNEWT_VAL(USB_VBUS_DETECTION_ENABLE)
    /* PA9- VUSB not used for USB */
    USB_OTG->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
    USB_OTG->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
    USB_OTG->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;
#else
    USB_OTG->GCCFG &= ~USB_OTG_GCCFG_NOVBUSSENS;
    USB_OTG->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
    USB_OTG->GCCFG |= USB_OTG_GCCFG_VBUSASEN;
    hal_gpio_init_in(MCU_GPIO_PORTA(9), HAL_GPIO_PULL_NONE);
#endif
#elif USB_OTG_GCCFG_VBDEN
#if MYNEWT_VAL(USB_VBUS_DETECTION_ENABLE)
    hal_gpio_init_in(MCU_GPIO_PORTA(9), HAL_GPIO_PULL_NONE);
    USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBDEN;
#else
    /* PA9- VUSB not used for USB */
    USB_OTG->GCCFG &= ~USB_OTG_GCCFG_VBDEN;
    /* B-peripheral session valid override enable */
    USB_OTG->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USB_OTG->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#endif
#endif

#if MYNEWT_VAL(MCU_STM32U5)
    /* Enable USB power */
    HAL_PWREx_EnableVddUSB();
#endif
}
