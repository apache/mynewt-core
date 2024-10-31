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

#include <stddef.h>
#include <inttypes.h>
#include <mcu/cortex_m33.h>
#include <mcu/nrf5340_hal.h>
#include <bsp/bsp.h>
#include <nrf_gpio.h>

#if MCUBOOT_MYNEWT
#include <bootutil/bootutil.h>
#endif
#include <os/util.h>

#if MYNEWT_VAL(BOOT_LOADER) && !MYNEWT_VAL(MCU_APP_SECURE)

struct periph_id_range {
    uint8_t first;
    uint8_t last;
};

/* Array of peripheral ID ranges that will be set as unsecure before bootloader jumps to application code */
static const struct periph_id_range ns_peripheral_ids[] = {
    { 0, 0 },
    { 4, 6 },
    { 8, 12 },
    { 14, 17 },
    { 20, 21 },
    { 23, 36 },
    { 38, 38 },
    { 40, 40 },
    { 42, 43 },
    { 45, 45 },
    { 48, 48 },
    { 51, 52 },
    { 54, 55 },
    { 57, 57 },
    { 66, 66 },
    { 128, 129 },
};

/* Below is to unmangle comma separated GPIO pins from MYNEWT_VAL */
#define _Args(...) __VA_ARGS__
#define STRIP_PARENS(X) X
#define UNMANGLE_MYNEWT_VAL(X) STRIP_PARENS(_Args X)

#if MYNEWT_VAL(MCU_GPIO_PERIPH)
static const unsigned int periph_gpios[] = { UNMANGLE_MYNEWT_VAL(MYNEWT_VAL(MCU_GPIO_PERIPH)) };
#endif

extern uint8_t __StackTop[];
extern uint8_t _start_sg[];
extern uint8_t _end_sg[];

static void
init_nsc(void)
{
    int region = (uintptr_t)_start_sg / 0x4000;
    uintptr_t region_start = region * 0x4000;
    uintptr_t region_limit = region_start + 0x4000;
    int nsc_region_size = 32;
    int m = 1;
    /* Calculate NSC region size by checking _start_sg offset in last 16K region */
    while ((uintptr_t)_start_sg < region_limit - nsc_region_size) {
        m++;
        nsc_region_size <<= 1;
    }
    assert(m <= 8);
    NRF_SPU_S->FLASHNSC[0].REGION = region;
    NRF_SPU_S->FLASHNSC[0].SIZE = m;
}

void
hal_system_start(void *img_start)
{
    int i;
    int j;
    int range_count;
    struct flash_sector_range sr;
    uintptr_t *img_data;
    /* Number of 16kB flash regions used by bootloader */
    int bootloader_flash_regions;
    __attribute__((cmse_nonsecure_call, noreturn)) void (* app_reset)(void);

    __disable_irq();

    /* Mark selected peripherals as unsecure */
    for (i = 0; i < ARRAY_SIZE(ns_peripheral_ids); ++i) {
        for (j = ns_peripheral_ids[i].first; j <= ns_peripheral_ids[i].last; ++j) {
            if (((NRF_SPU->PERIPHID[j].PERM & SPU_PERIPHID_PERM_PRESENT_Msk) == 0) ||
                ((NRF_SPU->PERIPHID[j].PERM & SPU_PERIPHID_PERM_SECUREMAPPING_Msk) < SPU_PERIPHID_PERM_SECUREMAPPING_UserSelectable)) {
                continue;
            }
            NRF_SPU->PERIPHID[j].PERM &= ~SPU_PERIPHID_PERM_SECATTR_Msk;
        }
    }

    /* Route exceptions to non-secure, allow software reset from non-secure */
    SCB->AIRCR = 0x05FA0000 | (SCB->AIRCR & (~SCB_AIRCR_VECTKEY_Msk | SCB_AIRCR_SYSRESETREQS_Msk)) | SCB_AIRCR_BFHFNMINS_Msk;
    for (i = 0; i < ARRAY_SIZE(NVIC->ITNS); ++i) {
        NVIC->ITNS[i] = 0xFFFFFFFF;
    }

    /* Mark non-bootloader flash regions as non-secure */
    flash_area_to_sector_ranges(FLASH_AREA_BOOTLOADER, &range_count, &sr);
    bootloader_flash_regions = (sr.fsr_sector_count * sr.fsr_sector_size) / 0x4000;

    for (i = bootloader_flash_regions; i < 64; ++i) {
        NRF_SPU->FLASHREGION[i].PERM &= ~SPU_FLASHREGION_PERM_SECATTR_Msk;
    }

    if ((uint32_t)_start_sg < (uint32_t)_end_sg) {
        init_nsc();
    }

    /* Mark RAM as non-secure */
    for (i = 0; i < 64; ++i) {
        NRF_SPU->RAMREGION[i].PERM &= ~SPU_FLASHREGION_PERM_SECATTR_Msk;
    }

    /* Move DPPI to non-secure area */
    NRF_SPU->DPPI->PERM = 0;

    /* Move GPIO to non-secure area */
    NRF_SPU->GPIOPORT[0].PERM = 0;
    NRF_SPU->GPIOPORT[1].PERM = 0;

#if MYNEWT_VAL(MCU_GPIO_PERIPH)
    for (i = 0; i < ARRAY_SIZE(periph_gpios); ++i) {
        nrf_gpio_pin_mcu_select(periph_gpios[i], GPIO_PIN_CNF_MCUSEL_Peripheral);
    }
#endif

    /*
     * For now whole RAM is marked as non-secure. To prevent data leak from secure to
     * non-secure, whole RAM is cleared before starting application code.
     * Interrupt VTOR for secure world that was previously put in RAM is moved to
     * flash again.
     */
    SCB->VTOR = 0;
    /*
     * Normal loop here is inlined by GCC to call to memset hence asm version of
     * memset that does not use stack (that just get erased).
     */
    asm volatile("    mov     r0, #0        \n"
                 "1:  stmia   %0!, {r0}     \n"
                 "    cmp     %0, %1        \n"
                 "    blt     1b            \n"
        :
        : "r" (&_ram_start), "r" (&__StackTop)
        : "r0");
    /* Application startup code expects interrupts to be enabled */
    __enable_irq();

    img_data = img_start;
    app_reset = (void *)(img_data[1]);
    __TZ_set_MSP_NS(img_data[0]);
    app_reset();
}

#else

/**
 * Boots the image described by the supplied image header.
 *
 * @param hdr                   The header for the image to boot.
 */
void __attribute__((naked))
hal_system_start(void *img_start)
{
    uint32_t *img_data = img_start;

    asm volatile (".syntax unified        \n"
                  /* 1st word is stack pointer */
                  "    msr  msp, %0       \n"
                  /* 2nd word is a reset handler (image entry) */
                  "    bx   %1            \n"
                  : /* no output */
                  : "r" (img_data[0]), "r" (img_data[1]));
}

#endif

/**
 * Boots the image described by the supplied image header.
 * This routine is used in split-app scenario when loader decides
 * that it wants to run the app instead.
 *
 * @param hdr                   The header for the image to boot.
 */
void
hal_system_restart(void *img_start)
{
    int i;

    /*
     * NOTE: on reset, PRIMASK should have global interrupts enabled so
     * the code disables interrupts, clears the interrupt enable bits,
     * clears any pending interrupts, then enables global interrupts
     * so processor looks like state it would be in if it reset.
     */
    __disable_irq();
    for (i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xffffffff;
    }

    for (i = 0; i < sizeof(NVIC->ICPR) / sizeof(NVIC->ICPR[0]); i++) {
        NVIC->ICPR[i] = 0xffffffff;
    }
    __enable_irq();

    hal_system_start(img_start);
}
