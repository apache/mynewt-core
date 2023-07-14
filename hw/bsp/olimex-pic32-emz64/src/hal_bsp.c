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

#include "os/mynewt.h"

#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_bsp.h>
#include <mcu/mips_bsp.h>
#include <mcu/mips_hal.h>
#include <mcu/pic32mz_periph.h>
#include <string.h>
#include <xc.h>

#if MYNEWT_VAL(SPIFLASH)
#include <spiflash/spiflash.h>
#endif

#if MYNEWT_VAL(BOOT_LOADER) || MYNEWT_VAL(MCU_NO_BOOTLOADER_BUILD)
/* JTAG on, WDT off */
#pragma config FDMTEN=0, FSOSCEN=0, DMTCNT=1
#pragma config DEBUG=ON
#pragma config JTAGEN=OFF
#pragma config FSLEEP=OFF
#pragma config TRCEN=OFF
#pragma config ICESEL=ICS_PGx2

#if MYNEWT_VAL(CLOCK_FREQ) == 8000000
#pragma config POSCMOD = OFF
#pragma config FNOSC = FRCDIV
#pragma config FPLLICLK=0
#else
#pragma config POSCMOD = EC
#if MYNEWT_VAL(CLOCK_FREQ) == 24000000
#pragma config FNOSC = POSC
/* 24MHz posc input -> 50mhz*/
#pragma config FPLLICLK=0
#elif MYNEWT_VAL(CLOCK_FREQ) == 50000000
#pragma config FNOSC = SPLL
/* 24MHz posc input to pll, div by 3 -> 8, multiply by 50 -> 400, div by 8 -> 50mhz*/
#pragma config FPLLICLK=0, FPLLIDIV=DIV_3, FPLLRNG=RANGE_5_10_MHZ, FPLLMULT=MUL_50, FPLLODIV=DIV_8
#elif MYNEWT_VAL(CLOCK_FREQ) == 100000000
#pragma config FNOSC = SPLL
/* 24MHz posc input to pll, div by 3, multiply by 50, div by 4 -> 100mhz*/
#pragma config FPLLICLK=0, FPLLIDIV=DIV_3, FPLLRNG=RANGE_5_10_MHZ, FPLLMULT=MUL_50, FPLLODIV=DIV_4
#elif MYNEWT_VAL(CLOCK_FREQ) == 200000000
#pragma config FNOSC = SPLL
/* 24MHz posc input to pll, div by 3, multiply by 50, div by 2 -> 200mhz*/
#pragma config FPLLICLK=0, FPLLIDIV=DIV_3, FPLLRNG=RANGE_5_10_MHZ, FPLLMULT=MUL_50, FPLLODIV=DIV_2
#else
#error Clock requency not supported
#endif
#endif
/* USB off */
#pragma config FUSBIDIO=0
/*
 * Watchdog in non-window mode, watchdog disabled during flash programming,
 * period: 32s
 */
#pragma config WINDIS=1, WDTSPGM=1, WDTPS=15

#if MYNEWT_VAL(ETH_0)
#if MYNEWT_VAL_CHOICE(PIC32_ETH_0_PHY_ITF, RMII)
#pragma config FMIIEN=OFF
#else
#pragma config FMIIEN=ON
#endif

#if MYNEWT_VAL(PIC32_ETH_0_PHY_ALT_PINS)
#pragma config FETHIO=OFF
#else
#pragma config FETHIO=ON
#endif
#endif

#endif

#if MYNEWT_VAL(SPIFLASH)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
struct bus_spi_node_cfg flash_spi_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(BSP_FLASH_SPI_BUS),
    .pin_cs = MYNEWT_VAL(SPIFLASH_SPI_CS_PIN),
    .mode = MYNEWT_VAL(SPIFLASH_SPI_MODE),
    .data_order = HAL_SPI_MSB_FIRST,
    .freq = MYNEWT_VAL(SPIFLASH_BAUDRATE),
};
#endif
#endif

static const struct hal_flash *flash_devs[] = {
    [0] = &pic32mz_flash_dev,
#if MYNEWT_VAL(SPIFLASH)
    [1] = &spiflash_dev.hal,
#endif
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id >= ARRAY_SIZE(flash_devs)) {
        return NULL;
    }

    return flash_devs[id];
}

void
hal_bsp_init(void)
{
    if (MYNEWT_VAL(ETH_0)) {
        /* Remove reset from LAN8720 */
        hal_gpio_init_out(MCU_GPIO_PORTB(11), 1);
    }

    pic32mz_periph_create();
#if MYNEWT_VAL(SPIFLASH) && MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = spiflash_create_spi_dev(&spiflash_dev.dev,
                                 MYNEWT_VAL(BSP_FLASH_SPI_NAME), &flash_spi_cfg);
    assert(rc == 0);
#endif
}

void
hal_bsp_deinit(void)
{
    IEC0 = 0;
    IEC1 = 0;
    IEC2 = 0;
    IEC3 = 0;
    IEC4 = 0;
    IEC5 = 0;
    IEC6 = 0;
    IFS0 = 0;
    IFS1 = 0;
    IFS2 = 0;
    IFS3 = 0;
    IFS4 = 0;
    IFS5 = 0;
    IFS6 = 0;
}

int
hal_bsp_hw_id_len(void)
{
    return sizeof(DEVID);
}

int
hal_bsp_hw_id(uint8_t *id, int max_len)
{
    if (max_len > sizeof(DEVID)) {
        max_len = sizeof(DEVID);
    }

    memcpy(id, (const void *)&DEVID, max_len);
    return max_len;
}
