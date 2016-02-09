/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/types.h>
#include <bsp/cmsis_nvic.h>
#include <hal/flash_map.h>

void *_sbrk(int incr);
void _close(int fd);

#define PEND_SV_PRIO    ((1 << __NVIC_PRIO_BITS) - 1)
#define SYSTICK_PRIO    (PEND_SV_PRIO - 1)


static struct flash_area arduino_zero_flash_areas[] = {
    [FLASH_AREA_BOOTLOADER] = {
        .fa_flash_id = 0,       /* internal flash */
        .fa_off = 0x00000000,   /* beginning */
        .fa_size = (32 * 1024)
    },
    [FLASH_AREA_IMAGE_0] = {
        .fa_flash_id = 0,
        .fa_off = 0x00008000,
        .fa_size = (104 * 1024)
    },
    [FLASH_AREA_IMAGE_1] = {
        .fa_flash_id = 0,
        .fa_off = 0x00022000,
        .fa_size = (104 * 1024)
    },
    [FLASH_AREA_IMAGE_SCRATCH] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003c000,
        .fa_size = (8 * 1024)
    },
    [FLASH_AREA_NFFS] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003e000,
        .fa_size = (8 * 1024)
    }
};

int
bsp_imgr_current_slot(void)
{
    return FLASH_AREA_IMAGE_0;
}

/**
 * os systick init
 *
 * Initializes systick for the MCU
 *
 * @param os_tick_usecs The number of microseconds in an os time tick
 */
void
os_bsp_systick_init(uint32_t os_tick_usecs)
{
    /* XXX */
    uint32_t reload_val;

    reload_val = (((uint64_t)SystemCoreClock * os_tick_usecs) / 1000000) - 1;

    /* Set the system time ticker up */
    SysTick->LOAD = reload_val;
    SysTick->VAL = 0;
    SysTick->CTRL = 0x0007;

    /* Set the system tick priority */
    NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIO);
}

void
os_bsp_init(void)
{
    /*
     * XXX these references are here to keep the functions in for libc to find.
     */
    _sbrk(0);
    _close(0);
    flash_area_init(arduino_zero_flash_areas,
      sizeof(arduino_zero_flash_areas) / sizeof(arduino_zero_flash_areas[0]));    
}
