/**
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
#include <hal/flash_map.h>
#include <hal/hal_bsp.h>
#include "bsp/cmsis_nvic.h"
#include "mcu/nrf51.h"
#include "mcu/nrf51_bitfields.h"
#include "mcu/nrf51_hal.h"

#define BSP_LOWEST_PRIO     ((1 << __NVIC_PRIO_BITS) - 1)

static struct flash_area bsp_flash_areas[] = {
    [FLASH_AREA_BOOTLOADER] = {
        .fa_flash_id = 0,       /* internal flash */
        .fa_off = 0x00000000,   /* beginning */
        .fa_size = (32 * 1024)
    },
    [FLASH_AREA_IMAGE_0] = {
        .fa_flash_id = 0,
        .fa_off = 0x00008000,
        .fa_size = (110 * 1024)
    },
    [FLASH_AREA_IMAGE_1] = {
        .fa_flash_id = 0,
        .fa_off = 0x00023800,
        .fa_size = (110 * 1024)
    },
    [FLASH_AREA_IMAGE_SCRATCH] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003f000,
        .fa_size = (2 * 1024)
    },
    [FLASH_AREA_NFFS] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003f800,
        .fa_size = (2 * 1024)
    }
};

void _close(int fd);

/*
 * Returns the flash map slot where the currently active image is located.
 * If executing from internal flash from fixed location, that slot would
 * be easy to find.
 * If images are in external flash, and copied to RAM for execution, then
 * this routine would have to figure out which one of those slots is being
 * used.
 */
int
bsp_imgr_current_slot(void)
{
    return FLASH_AREA_IMAGE_0;
}

void
bsp_init(void)
{
    /*
     * XXX this reference is here to keep this function in.
     */
    _sbrk(0);
    _close(0);

    flash_area_init(bsp_flash_areas,
      sizeof(bsp_flash_areas) / sizeof(bsp_flash_areas[0]));

}

extern void timer_handler(void);
static void
rtc0_timer_handler(void)
{
    if (NRF_RTC0->EVENTS_TICK) {
        NRF_RTC0->EVENTS_TICK = 0;
        timer_handler();
    }
}

void
os_bsp_systick_init(uint32_t os_ticks_per_sec)
{
    uint32_t ctx;
    uint32_t mask;
    uint32_t pre_scaler;

    /* Turn on the LFCLK */
    NRF_CLOCK->XTALFREQ = CLOCK_XTALFREQ_XTALFREQ_16MHz;
    NRF_CLOCK->TASKS_LFCLKSTOP = 1;
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal;
    NRF_CLOCK->TASKS_LFCLKSTART = 1;

    /* Wait here till started! */
    mask = CLOCK_LFCLKSTAT_STATE_Msk | CLOCK_LFCLKSTAT_SRC_Xtal;
    while (1) {
        if (NRF_CLOCK->EVENTS_LFCLKSTARTED) {
            if ((NRF_CLOCK->LFCLKSTAT & mask) == mask) {
                break;
            }
        }
    }

    /* Is this exact frequency obtainable? */
    pre_scaler = (32768 / os_ticks_per_sec) - 1;

    /* disable interrupts */
    __HAL_DISABLE_INTERRUPTS(ctx);

    NRF_RTC0->TASKS_STOP = 1;
    NRF_RTC0->EVENTS_TICK = 0;
    NRF_RTC0->PRESCALER = pre_scaler;
    NRF_RTC0->INTENCLR = 0xffffffff;
    NRF_RTC0->TASKS_CLEAR = 1;

    /* Set isr in vector table and enable interrupt */
    NVIC_SetPriority(RTC0_IRQn, BSP_LOWEST_PRIO - 1);
    NVIC_SetVector(RTC0_IRQn, (uint32_t)rtc0_timer_handler);
    NVIC_EnableIRQ(RTC0_IRQn);

    NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Msk;
    NRF_RTC0->TASKS_START = 1;

    __HAL_ENABLE_INTERRUPTS(ctx);
}
