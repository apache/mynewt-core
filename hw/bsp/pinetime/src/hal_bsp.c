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

#include <inttypes.h>
#include <assert.h>
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "hal/hal_system.h"
#include "hal/hal_flash_int.h"
#include "hal/hal_timer.h"
#include "hal/hal_bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "mcu/nrf52_hal.h"
#include "mcu/nrf52_periph.h"
#if MYNEWT_VAL(BSP_CHARGER)
#include "sgm4056/sgm4056.h"
#endif
#if MYNEWT_VAL(BSP_BATTERY)
#include "battery/battery_adc.h"
#include "adc_nrf52/adc_nrf52.h"
#include <nrf_saadc.h>
#endif
#if MYNEWT_VAL(SPIFLASH)
#include <spiflash/spiflash.h>
#endif

/** What memory to include in coredump. */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE,
    }
};

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

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
    /* MCU internal flash. */
    [0] = &nrf52k_flash_dev,
#if MYNEWT_VAL(SPIFLASH)
    /* External SPI Flash. */
    [1] = &spiflash_dev.hal,
#endif
};

/**
 * Retrieves the flash device with the specified ID.  Returns NULL if no such
 * device exists.
 */
const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id >= ARRAY_SIZE(flash_devs)) {
        return NULL;
    }
    return flash_devs[id];
}

/**
 * Retrieves the configured priority for the given interrupt. If no priority
 * is configured, returns the priority passed in.
 *
 * @param irq_num               The IRQ being queried.
 * @param pri                   The default priority if none is configured.
 *
 * @return uint32_t             The specified IRQ's priority.
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    uint32_t cfg_pri;

    switch (irq_num) {
    /* Radio gets highest priority */
    case RADIO_IRQn:
        cfg_pri = 0;
        break;
    default:
        cfg_pri = pri;
    }
    return cfg_pri;
}

#if MYNEWT_VAL(BSP_BATTERY)
static struct adc_dev_cfg hal_bsp_adc_dev_config = {
    .resolution = ADC_RESOLUTION_10BIT,
    .oversample = ADC_OVERSAMPLE_DISABLED,
    .calibrate = false,
};

static struct adc_chan_cfg hal_bsp_adc_channel_config = {
    .gain = ADC_GAIN1_6,
    .reference = ADC_REFERENCE_INTERNAL,
    .acq_time = ADC_ACQTIME_10US,
    .pin = NRF_SAADC_INPUT_AIN7,
    .differential = false,
    .pin_negative = -1,
};

static struct battery hal_bsp_battery_dev;

static struct battery_adc hal_bsp_battery_adc_dev;

static struct battery_adc_cfg hal_bsp_battery_config = {
    .battery = &hal_bsp_battery_dev.b_dev,
    .adc_dev_name = "adc0",
    .adc_open_arg = &hal_bsp_adc_dev_config,
    .adc_channel_cfg = &hal_bsp_adc_channel_config,
    .channel = 0,
    .mul = 2,
    .div = 1,
};

static void
hal_bsp_battery_init(void)
{
    int rc;

    rc = os_dev_create(&hal_bsp_battery_dev.b_dev, "battery",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       battery_init, NULL);
    assert(rc == 0);

    rc = os_dev_create(&hal_bsp_battery_adc_dev.dev.dev, "battery_adc",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       battery_adc_init, &hal_bsp_battery_config);
    assert(rc == 0);
}
#endif

#if MYNEWT_VAL(BSP_CHARGER)
static struct sgm4056_dev os_bsp_charger;
static struct sgm4056_dev_config os_bsp_charger_config = {
    .power_presence_pin = CHARGER_POWER_PRESENCE_PIN,
    .charge_indicator_pin = CHARGER_CHARGE_PIN,
};

static void
hal_bsp_charger_init(void)
{
    int rc;

    rc = os_dev_create(&os_bsp_charger.dev, "charger",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       sgm4056_dev_init, &os_bsp_charger_config);
    assert(rc == 0);
}
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static struct bus_spi_node hal_bsp_display_spi;
struct bus_spi_node_cfg hal_bsp_display_spi_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(BSP_FLASH_SPI_BUS),
    .pin_cs = LCD_CHIP_SELECT_PIN,
    .mode = BUS_SPI_MODE_3,
    .data_order = HAL_SPI_MSB_FIRST,
    .freq = 8000,
};

static void
hal_bsp_display_spi_init(void)
{
    int rc;

    rc = bus_spi_node_create("spidisplay", &hal_bsp_display_spi, &hal_bsp_display_spi_cfg, NULL);
    assert(rc == 0);
}
#endif

void
hal_bsp_init(void)
{
    int rc;

    (void)rc;

    /* Make sure system clocks have started. */
    hal_system_clock_start();

    /* Create all available nRF52840 peripherals */
    nrf52_periph_create();

    #if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    /* Create display spi node */
    hal_bsp_display_spi_init();

    #if MYNEWT_VAL(SPIFLASH)
    /* Create external flash dev */
    rc = spiflash_create_spi_dev(&spiflash_dev.dev,
                                 MYNEWT_VAL(BSP_FLASH_SPI_NAME), &flash_spi_cfg);
    assert(rc == 0);
    #endif
    #endif

    #if MYNEWT_VAL(BSP_CHARGER)
    /* Create charge controller */
    hal_bsp_charger_init();
    #endif

    #if MYNEWT_VAL(BSP_BATTERY)
    /* Create adc and battery driver */
    hal_bsp_battery_init();
    #endif
}

void
hal_bsp_deinit(void)
{
}
