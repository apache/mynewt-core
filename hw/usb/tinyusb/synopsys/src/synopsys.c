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

#if MYNEWT_VAL(USB_USE_OTG_HS)
#define USB_OTG            USB_OTG_HS
#define USB_OTG_IRQHandler OTG_HS_IRQHandler
#define USB_OTG_IRQn       OTG_HS_IRQn
#define USB_OTG_CLK_ENABLE __HAL_RCC_USB_OTG_HS_CLK_ENABLE
#else
#define USB_OTG            USB_OTG_FS
#define USB_OTG_IRQHandler OTG_FS_IRQHandler
#define USB_OTG_IRQn       OTG_FS_IRQn
#define USB_OTG_CLK_ENABLE __HAL_RCC_USB_OTG_FS_CLK_ENABLE
#endif
#define GPIO_AF_USB        MYNEWT_VAL(USB_AF_USB)

void
OTG_HS_IRQHandler(void)
{
    /* TinyUSB provides interrupt handler code */
    tud_int_handler(1);
}

void
OTG_FS_IRQHandler(void)
{
    /* TinyUSB provides interrupt handler code */
    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    NVIC_SetVector(USB_OTG_IRQn, (uint32_t)USB_OTG_IRQHandler);
    NVIC_SetPriority(USB_OTG_IRQn, 2);
#if MYNEWT_VAL(USB_USE_OTG_HS)
#ifdef __HAL_RCC_PWR_CLK_ENABLE
    __HAL_RCC_PWR_CLK_ENABLE();
#endif
    USB_OTG_CLK_ENABLE();
#if MYNEWT_VAL_USB_USE_OTG_HS_ULPI
    hal_gpio_init_af(MCU_GPIO_PORTA(3), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTA(5), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTB(0), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTB(1), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTB(5), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTB(10), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTB(11), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTB(12), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTB(13), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTC(0), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTC(2), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTC(3), 10, GPIO_NOPULL, GPIO_MODE_AF_PP);

    __HAL_RCC_USB1_OTG_HS_ULPI_CLK_ENABLE();
#else
    /*
     * USB Pin Init
     * PB14- DM, PB15- DP
     */
    hal_gpio_init_af(MCU_GPIO_PORTB(14), 12, GPIO_NOPULL, GPIO_MODE_AF_PP);
    hal_gpio_init_af(MCU_GPIO_PORTB(15), 12, GPIO_NOPULL, GPIO_MODE_AF_PP);
#endif
    // No VBUS sense
    USB_OTG->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

    /* B-peripheral session valid override enable */
    USB_OTG->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USB_OTG->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;

    // Force device mode
    USB_OTG->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
    USB_OTG->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;
#else
    /*
     * USB Pin Init
     * PA11- DM, PA12- DP
     */
    hal_gpio_init_af(MCU_GPIO_PORTA(11), GPIO_AF_USB, GPIO_NOPULL, GPIO_MODE_AF_PP);
#if MYNEWT_VAL(USB_DP_HAS_EXTERNAL_PULL_UP)
    hal_gpio_init_out(MCU_GPIO_PORTA(12), 0);
#if MYNEWT_VAL(OS_SCHEDULING)
    os_cputime_delay_usecs(1000);
#else
    os_time_delay(1);
#endif
#endif
    hal_gpio_init_af(MCU_GPIO_PORTA(12), GPIO_AF_USB, GPIO_NOPULL, GPIO_MODE_AF_PP);

    /*
     * Enable USB OTG clock, force device mode
     */
    USB_OTG_CLK_ENABLE();
#ifdef __HAL_RCC_PWR_CLK_ENABLE
    __HAL_RCC_PWR_CLK_ENABLE();
#endif

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
    USB_OTG->GCCFG |= USB_OTG_GCCFG_VBDEN;
#else
    /* PA9- VUSB not used for USB */
    USB_OTG->GCCFG &= ~USB_OTG_GCCFG_VBDEN;
    /* B-peripheral session valid override enable */
    USB_OTG->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USB_OTG->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#endif
#endif
#endif

#if MYNEWT_VAL(MCU_STM32U5) || MYNEWT_VAL(MCU_STM32L4) || MYNEWT_VAL(MCU_STM32L5)
    __HAL_RCC_PWR_CLK_ENABLE();
    /* Enable USB power */
    HAL_PWREx_EnableVddUSB();
#elif MYNEWT_VAL(MCU_STM32H7)
    HAL_PWREx_EnableUSBVoltageDetector();
#endif
}
