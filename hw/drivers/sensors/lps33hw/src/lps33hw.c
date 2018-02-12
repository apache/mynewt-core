/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include "defs/error.h"
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "hal/hal_i2c.h"
#include "sensor/sensor.h"
#include "sensor/pressure.h"
#include "lps33hw/lps33hw.h"
#include "lps33hw_priv.h"
#include "log/log.h"
#include "stats/stats.h"

#include "lps33hw_priv.h"

/* Define the stats section and records */
STATS_SECT_START(lps33hw_stat_section)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lps33hw_stat_section)
    STATS_NAME(lps33hw_stat_section, read_errors)
    STATS_NAME(lps33hw_stat_section, write_errors)
STATS_NAME_END(lps33hw_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lps33hw_stat_section) g_lps33hwstats;

#define LOG_MODULE_LPS33HW    (33)
#define LPS33HW_INFO(...)     LOG_INFO(&_log, LOG_MODULE_LPS33HW, __VA_ARGS__)
#define LPS33HW_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_LPS33HW, __VA_ARGS__)
static struct log _log;

/* Exports for the sensor API */
static int lps33hw_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int lps33hw_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_lps33hw_sensor_driver = {
    lps33hw_sensor_read,
    lps33hw_sensor_get_config
};

/**
 * Writes a single byte to the specified register
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lps33hw_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value };

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        LPS33HW_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                       itf->si_addr, reg, value);
        STATS_INC(g_lps33hwstats, read_errors);
    }

    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lps33hw_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &reg
    };

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LPS33HW_ERR("I2C access failed at address 0x%02X\n", itf->si_addr);
        STATS_INC(g_lps33hwstats, write_errors);
        return rc;
    }

    /* Read one byte back */
    data_struct.buffer = value;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
         LPS33HW_ERR("Failed to read from 0x%02X:0x%02X\n", itf->si_addr, reg);
         STATS_INC(g_lps33hwstats, read_errors);
    }
    return rc;
}

/**
 * Reads 3 bytes from the specified register
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lps33hw_read24(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &reg
    };

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LPS33HW_ERR("I2C access failed at address 0x%02X\n", itf->si_addr);
        STATS_INC(g_lps33hwstats, write_errors);
        return rc;
    }

    /* Read 3 bytes back */
    data_struct.len = 3;
    data_struct.buffer = buffer;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
         LPS33HW_ERR("Failed to read from 0x%02X:0x%02X\n", itf->si_addr, reg);
         STATS_INC(g_lps33hwstats, read_errors);
    }
    return rc;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this pressure sensor
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lps33hw_init(struct os_dev *dev, void *arg)
{
    struct lps33hw *lps;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        return SYS_ENODEV;
    }

    lps = (struct lps33hw *) dev;

    lps->cfg.mask = SENSOR_TYPE_ALL;

    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    sensor = &lps->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lps33hwstats),
        STATS_SIZE_INIT_PARMS(g_lps33hwstats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lps33hw_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lps33hwstats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc) {
        return rc;
    }

    /* Add the pressure driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_PRESSURE,
            (struct sensor_driver *) &g_lps33hw_sensor_driver);
    if (rc) {
        return rc;
    }

    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        return rc;
    }

    return sensor_mgr_register(sensor);
}

int
lps33hw_config(struct lps33hw *lps, struct lps33hw_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(lps->sensor));

    uint8_t val;
    rc = lps33hw_read8(itf, LPS33HW_WHO_AM_I, &val);
    if (rc) {
        return rc;
    }
    if (val != LPS33HW_WHO_AM_I_VAL) {
        return SYS_EINVAL;
    }

    /* set the poll rate to 10Hz */
    rc = lps33hw_write8(itf, LPS33HW_CTRL_REG1, 0x20);
    if (rc) {
        return rc;
    }

    rc = sensor_set_type_mask(&(lps->sensor), cfg->mask);
    if (rc) {
        return rc;
    }

    lps->cfg.mask = cfg->mask;

    return 0;
}

static int
lps33hw_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    (void)timeout;
    int rc;
    uint8_t payload[3];
    struct sensor_itf *itf;
   // struct lps33hw *lps;
   struct sensor_press_data spd;


    /* If the read isn't looking for pressure, don't do anything. */
    if (!(type & SENSOR_TYPE_PRESSURE)) {
        return SYS_EINVAL;
    }

    itf = SENSOR_GET_ITF(sensor);
    //lps = (struct lps33hw *) SENSOR_GET_DEVICE(sensor);

    rc = lps33hw_read24(itf, LPS33HW_PRESS_OUT_XL, payload);
    if (rc) {
        return rc;
    }

    spd.spd_press = (((uint32_t)payload[2] << 16) |
        ((uint32_t)payload[1] << 8) | payload[0]);

    spd.spd_press_is_valid = 1;

    rc = data_func(sensor, data_arg, &spd,
            SENSOR_TYPE_PRESSURE);
    return rc;
}

static int
lps33hw_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    /* If the read isn't looking for pressure, don't do anything. */
    if (!(type & SENSOR_TYPE_PRESSURE)) {
        return SYS_EINVAL;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;

    return 0;
}
