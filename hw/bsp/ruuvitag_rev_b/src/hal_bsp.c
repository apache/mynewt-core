
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

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "os/mynewt.h"
#include "nrfx.h"
#include "flash_map/flash_map.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_system.h"
#include "mcu/nrf52_hal.h"
#include "mcu/nrf52_periph.h"
#include "bsp/bsp.h"
#include "defs/sections.h"
#if MYNEWT_VAL(BME280_ONB)
#include "bme280/bme280.h"
static struct bme280 bme280;
#endif
#if MYNEWT_VAL(LIS2DH12_ONB)
#include "lis2dh12/lis2dh12.h"
static struct lis2dh12 lis2dh12;
#endif

#if MYNEWT_VAL(BME280_ONB)
static const struct sensor_itf spi_0_itf_bme = {
    .si_type = SENSOR_ITF_SPI,
    .si_num = 0,
    .si_cs_pin = 3
};
#endif

#if MYNEWT_VAL(LIS2DH12_ONB)
static const struct sensor_itf spi_0_itf_lis = {
    .si_type = SENSOR_ITF_SPI,
    .si_num = 0,
    .si_cs_pin = 8,
    .si_low_pin = 2,
    .si_high_pin = 6
};
#endif

/*
 * What memory to include in coredump.
 */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    }
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id == 0) {
        return &nrf52k_flash_dev;
    }

    return NULL;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return (0);
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
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

int
config_bme280_sensor(void)
{
#if MYNEWT_VAL(BME280_ONB)
    int rc;
    struct os_dev *dev;
    struct bme280_cfg bmecfg;

    dev = (struct os_dev *) os_dev_open("bme280_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    memset(&bmecfg, 0, sizeof(bmecfg));

    bmecfg.bc_mode = BME280_MODE_FORCED;
    bmecfg.bc_iir = BME280_FILTER_OFF;
    bmecfg.bc_sby_dur = BME280_STANDBY_MS_0_5;
    bmecfg.bc_boc[0].boc_type = SENSOR_TYPE_RELATIVE_HUMIDITY;
    bmecfg.bc_boc[1].boc_type = SENSOR_TYPE_PRESSURE;
    bmecfg.bc_boc[2].boc_type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    bmecfg.bc_boc[0].boc_oversample = BME280_SAMPLING_X1;
    bmecfg.bc_boc[1].boc_oversample = BME280_SAMPLING_X1;
    bmecfg.bc_boc[2].boc_oversample = BME280_SAMPLING_X1;
    bmecfg.bc_s_mask = SENSOR_TYPE_AMBIENT_TEMPERATURE|
                       SENSOR_TYPE_PRESSURE|
                       SENSOR_TYPE_RELATIVE_HUMIDITY;

    rc = bme280_config((struct bme280 *)dev, &bmecfg);
    SYSINIT_PANIC_ASSERT(rc == 0);

    os_dev_close(dev);
#endif
    return 0;
}

int
config_lis2dh12_sensor(void)
{
#if MYNEWT_VAL(LIS2DH12_ONB)
    int rc;
    struct os_dev *dev;
    struct lis2dh12_cfg cfg;

    dev = (struct os_dev *) os_dev_open("lis2dh12_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    memset(&cfg, 0, sizeof(cfg));

    cfg.lc_s_mask = SENSOR_TYPE_ACCELEROMETER;
    cfg.lc_rate = LIS2DH12_DATA_RATE_HN_1344HZ_L_5376HZ;
    cfg.lc_fs = LIS2DH12_FS_2G;

    rc = lis2dh12_config((struct lis2dh12 *)dev, &cfg);
    SYSINIT_PANIC_ASSERT(rc == 0);

    os_dev_close(dev);
#endif
    return 0;
}

static void
sensor_dev_create(void)
{
    int rc;
    (void)rc;

#if MYNEWT_VAL(BME280_ONB)
    rc = os_dev_create((struct os_dev *) &bme280, "bme280_0",
                       OS_DEV_INIT_PRIMARY, 0, bme280_init,
                       (void *)&spi_0_itf_bme);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(LIS2DH12_ONB)
    rc = os_dev_create((struct os_dev *) &lis2dh12, "lis2dh12_0",
                       OS_DEV_INIT_PRIMARY, 0, lis2dh12_init,
                       (void *)&spi_0_itf_lis);
    assert(rc == 0);
#endif
}

void
hal_bsp_init(void)
{
    /* Make sure system clocks have started */
    hal_system_clock_start();

    /* Create all available nRF52832 peripherals */
    nrf52_periph_create();

    sensor_dev_create();
}
