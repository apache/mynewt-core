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

#include <stdbool.h>
#include <os/mynewt.h>
#include <hal/hal_gpio.h>
#include <hal/hal_nvreg.h>
#include <hal/hal_system.h>
#include <mcu/mcu.h>
#include <bsp/bsp.h>

/*
 * STM32 system memory bootloader is normally entered by holding BOOT0 pin
 * high during reset. This code provides another way for boards where BOOT0
 * is not available (no button, pin not routed) or does not work because of
 * option bytes (with nSWBOOT0 cleared BOOT0 pin is ignored and boot source
 * comes from nBOOT0 option bit).
 */

static bool
stm32_rom_boot_pin_active(void)
{
#if MYNEWT_VAL(STM32_ROM_BOOT_PIN) >= 0
    bool active;

    hal_gpio_init_in(MYNEWT_VAL(STM32_ROM_BOOT_PIN),
                     MYNEWT_VAL(STM32_ROM_BOOT_PIN_PULL));
    /* Give pull resistor time to charge pin */
    os_cputime_delay_usecs(100);
    active = hal_gpio_read(MYNEWT_VAL(STM32_ROM_BOOT_PIN)) ==
             MYNEWT_VAL(STM32_ROM_BOOT_PIN_VALUE);
    hal_gpio_deinit(MYNEWT_VAL(STM32_ROM_BOOT_PIN));

    return active;
#else
    return false;
#endif
}

static bool
stm32_rom_boot_reset_count_reached(void)
{
#if MYNEWT_VAL(STM32_ROM_BOOT_RESET_COUNT) > 0
    uint32_t count = 0;

    if (hal_reset_cause() == HAL_RESET_PIN) {
        count = hal_nvreg_read(MYNEWT_VAL(STM32_ROM_BOOT_RESET_COUNT_NVREG)) + 1;
    }
    if (count >= MYNEWT_VAL(STM32_ROM_BOOT_RESET_COUNT)) {
        hal_nvreg_write(MYNEWT_VAL(STM32_ROM_BOOT_RESET_COUNT_NVREG), 0);
        return true;
    }
    if (count) {
        /* Reset pin pressed again during this time is counted */
        hal_nvreg_write(MYNEWT_VAL(STM32_ROM_BOOT_RESET_COUNT_NVREG), count);
        os_cputime_delay_usecs(MYNEWT_VAL(STM32_ROM_BOOT_RESET_WINDOW_MS) * 1000);
    }
    hal_nvreg_write(MYNEWT_VAL(STM32_ROM_BOOT_RESET_COUNT_NVREG), 0);
#endif

    return false;
}

void
stm32_rom_boot_init(void)
{
    if (stm32_rom_boot_pin_active() || stm32_rom_boot_reset_count_reached()) {
        stm32_start_bootloader();
    }
}
