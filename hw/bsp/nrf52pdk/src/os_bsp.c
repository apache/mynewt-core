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
#include <assert.h>

#include <os/os.h>
#include <bsp/cmsis_nvic.h>
#include <mcu/nrf52.h>
#include <mcu/nrf52_bitfields.h>
#include <util/util.h>
#include <hal/flash_map.h>

static struct flash_area bsp_flash_areas[] = {
    [FLASH_AREA_BOOTLOADER] = {
        .fa_flash_id = 0,       /* internal flash */
        .fa_off = 0x00000000,   /* beginning */
        .fa_size = (32 * 1024)
    },
    /* 2*16K and 1*64K sectors here */
    [FLASH_AREA_IMAGE_0] = {
        .fa_flash_id = 0,
        .fa_off = 0x00008000,
        .fa_size = (232 * 1024)
    },
    [FLASH_AREA_IMAGE_1] = {
        .fa_flash_id = 0,
        .fa_off = 0x00042000,
        .fa_size = (232 * 1024)
    },
    [FLASH_AREA_IMAGE_SCRATCH] = {
        .fa_flash_id = 0,
        .fa_off = 0x0007c000,
        .fa_size = (4 * 1024)
    },
    [FLASH_AREA_NFFS] = {
        .fa_flash_id = 0,
        .fa_off = 0x0007d000,
        .fa_size = (12 * 1024)
    }
};

void *_sbrk(int incr);
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
os_bsp_init(void)
{
    /*
     * XXX this reference is here to keep this function in.
     */
    _sbrk(0);
    _close(0);

    flash_area_init(bsp_flash_areas,
      sizeof(bsp_flash_areas) / sizeof(bsp_flash_areas[0]));
}

#define CALLOUT_TIMER       NRF_TIMER1
#define CALLOUT_IRQ         TIMER1_IRQn
#define CALLOUT_CMPREG      0   /* generate timer interrupt */
#define CALLOUT_COUNTER     1   /* capture current timer value */
#define CALLOUT_PRESCALER   4   /* prescaler to generate 1MHz timer freq */
#define TIMER_GEQ(__t1, __t2)   ((int32_t)((__t1) - (__t2)) >= 0)

static int timer_ticks_per_ostick;
static uint32_t lastocmp;

static inline uint32_t
nrf52_callout_counter(void)
{
    /*
     * Capture the current timer value and return it.
     */
    CALLOUT_TIMER->TASKS_CAPTURE[CALLOUT_COUNTER] = 1;
    return (CALLOUT_TIMER->CC[CALLOUT_COUNTER]);
}

static void
nrf52_timer_handler(void)
{
    int ticks;
    os_sr_t sr;
    uint32_t counter, ocmp;

    assert(CALLOUT_TIMER->EVENTS_COMPARE[CALLOUT_CMPREG]);

    OS_ENTER_CRITICAL(sr);

    /* Clear interrupt */
    CALLOUT_TIMER->EVENTS_COMPARE[CALLOUT_CMPREG] = 0;

    /* Capture the timer value */
    counter = nrf52_callout_counter();

    /* Calculate elapsed ticks */
    ticks = (counter - lastocmp) / timer_ticks_per_ostick;
    lastocmp += ticks * timer_ticks_per_ostick;

    ocmp = lastocmp;
    do {
        ocmp += timer_ticks_per_ostick;
        CALLOUT_TIMER->CC[CALLOUT_CMPREG] = ocmp;
        counter = nrf52_callout_counter();
    } while (TIMER_GEQ(counter, ocmp));

    OS_EXIT_CRITICAL(sr);

    os_time_advance(ticks, true);
}

void
os_bsp_systick_init(uint32_t os_ticks_per_sec, int prio)
{
    lastocmp = 0;
    timer_ticks_per_ostick = 1000000 / os_ticks_per_sec;

    /*
     * Program CALLOUT_TIMER to operate at 1MHz and trigger an output
     * compare interrupt at a rate of 'os_ticks_per_sec'.
     */
    CALLOUT_TIMER->TASKS_STOP = 1;
    CALLOUT_TIMER->TASKS_CLEAR = 1;
    CALLOUT_TIMER->MODE = TIMER_MODE_MODE_Timer;
    CALLOUT_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    CALLOUT_TIMER->PRESCALER = CALLOUT_PRESCALER;

    CALLOUT_TIMER->CC[CALLOUT_CMPREG] = timer_ticks_per_ostick;
    CALLOUT_TIMER->INTENSET = TIMER_COMPARE_INT_MASK(CALLOUT_CMPREG);
    CALLOUT_TIMER->EVENTS_COMPARE[CALLOUT_CMPREG] = 0;

    NVIC_SetPriority(CALLOUT_IRQ, prio);
    NVIC_SetVector(CALLOUT_IRQ, (uint32_t)nrf52_timer_handler);
    NVIC_EnableIRQ(CALLOUT_IRQ);

    CALLOUT_TIMER->TASKS_START = 1;     /* start the callout timer */
}
