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
#include "sensor/accel.h"
#include "lsm303dlhc/lsm303dlhc.h"
#include "lsm303dlhc_priv.h"

#if MYNEWT_VAL(LSM303DLHC_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(LSM303DLHC_STATS)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(LSM303DLHC_STATS)
/* Define the stats section and records */
STATS_SECT_START(lsm303dlhc_stat_section)
    STATS_SECT_ENTRY(samples_acc_2g)
    STATS_SECT_ENTRY(samples_acc_4g)
    STATS_SECT_ENTRY(samples_acc_8g)
    STATS_SECT_ENTRY(samples_acc_16g)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lsm303dlhc_stat_section)
    STATS_NAME(lsm303dlhc_stat_section, samples_acc_2g)
    STATS_NAME(lsm303dlhc_stat_section, samples_acc_4g)
    STATS_NAME(lsm303dlhc_stat_section, samples_acc_8g)
    STATS_NAME(lsm303dlhc_stat_section, samples_acc_16g)
    STATS_NAME(lsm303dlhc_stat_section, errors)
STATS_NAME_END(lsm303dlhc_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lsm303dlhc_stat_section) g_lsm303dlhcstats;
#endif

#if MYNEWT_VAL(LSM303DLHC_LOG)
#define LOG_MODULE_LSM303DLHC (303)
#define LSM303DLHC_INFO(...)  LOG_INFO(&_log, LOG_MODULE_LSM303DLHC, __VA_ARGS__)
#define LSM303DLHC_ERR(...)   LOG_ERROR(&_log, LOG_MODULE_LSM303DLHC, __VA_ARGS__)
static struct log _log;
#else
#define LSM303DLHC_INFO(...)
#define LSM303DLHC_ERR(...)
#endif

/* Exports for the sensor interface.
 */
static void *lsm303dlhc_sensor_get_interface(struct sensor *, sensor_type_t);
static int lsm303dlhc_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int lsm303dlhc_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_lsm303dlhc_sensor_driver = {
    lsm303dlhc_sensor_get_interface,
    lsm303dlhc_sensor_read,
    lsm303dlhc_sensor_get_config
};

/**
 * Writes a single byte to the specified register
 *
 * @param The I2C address to use
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm303dlhc_write8(uint8_t addr, uint8_t reg, uint32_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(MYNEWT_VAL(LSM303DLHC_I2CBUS), &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LSM303DLHC_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                       addr, reg, value);
        #if MYNEWT_VAL(LSM303DLHC_STATS)
        STATS_INC(g_lsm303dlhcstats, errors);
        #endif
    }

    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @param The I2C address to use
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm303dlhc_read8(uint8_t addr, uint8_t reg, uint8_t *value)
{
    int rc;
    uint8_t payload;

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 1,
        .buffer = &payload
    };

    /* Register write */
    payload = reg;
    rc = hal_i2c_master_write(MYNEWT_VAL(LSM303DLHC_I2CBUS), &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LSM303DLHC_ERR("I2C access failed at address 0x%02X\n", addr);
        #if MYNEWT_VAL(LSM303DLHC_STATS)
        STATS_INC(g_lsm303dlhcstats, errors);
        #endif
        goto error;
    }

    /* Read one byte back */
    payload = 0;
    rc = hal_i2c_master_read(MYNEWT_VAL(LSM303DLHC_I2CBUS), &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);
    *value = payload;
    if (rc) {
        LSM303DLHC_ERR("Failed to read from 0x%02X:0x%02X\n", addr, reg);
        #if MYNEWT_VAL(LSM303DLHC_STATS)
        STATS_INC(g_lsm303dlhcstats, errors);
        #endif
    }

error:
    return rc;
}

int
lsm303dlhc_read48(uint8_t addr, uint8_t reg, int16_t *x, int16_t*y, int16_t *z)
{
    int rc;
    uint8_t payload[7] = { reg | 0x80, 0, 0, 0, 0, 0, 0 };

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 1,
        .buffer = payload
    };

    /* Register write */
    rc = hal_i2c_master_write(MYNEWT_VAL(LSM303DLHC_I2CBUS), &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LSM303DLHC_ERR("I2C access failed at address 0x%02X\n", addr);
        #if MYNEWT_VAL(LSM303DLHC_STATS)
        STATS_INC(g_lsm303dlhcstats, errors);
        #endif
        goto error;
    }

    /* Read six bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = 6;
    rc = hal_i2c_master_read(MYNEWT_VAL(LSM303DLHC_I2CBUS), &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    /* Shift 12-bit left-aligned accel values into 16-bit int */
    *x = ((int16_t)(payload[0] | (payload[1] << 8))) >> 4;
    *y = ((int16_t)(payload[2] | (payload[3] << 8))) >> 4;
    *z = ((int16_t)(payload[4] | (payload[5] << 8))) >> 4;

    if (rc) {
        LSM303DLHC_ERR("Failed to read from 0x%02X:0x%02X\n", addr, reg);
        #if MYNEWT_VAL(LSM303DLHC_STATS)
        STATS_INC(g_lsm303dlhcstats, errors);
        #endif
        goto error;
    }

    /* ToDo: Log raw reads */
    // console_printf("0x%04X\n", (uint16_t)payload[0] | ((uint16_t)payload[1] << 8));

error:
    return rc;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accellerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm303dlhc_init(struct os_dev *dev, void *arg)
{
    struct lsm303dlhc *lsm;
    struct sensor *sensor;
    int rc;

    lsm = (struct lsm303dlhc *) dev;

    #if MYNEWT_VAL(LSM303DLHC_LOG)
    log_register("lsm303dlhc", &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
    #endif

    sensor = &lsm->sensor;

    #if MYNEWT_VAL(LSM303DLHC_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lsm303dlhcstats),
        STATS_SIZE_INIT_PARMS(g_lsm303dlhcstats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lsm303dlhc_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register("lsm303dlhc", STATS_HDR(g_lsm303dlhcstats));
    SYSINIT_PANIC_ASSERT(rc == 0);
    #endif

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER,
            (struct sensor_driver *) &g_lsm303dlhc_sensor_driver);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
lsm303dlhc_config(struct lsm303dlhc *lsm, struct lsm303dlhc_cfg *cfg)
{
    int rc;

    /* Overwrite the configuration associated with this generic accelleromter. */
    memcpy(&lsm->cfg, cfg, sizeof(*cfg));

    /* Set data rate (or power down) and enable XYZ output */
    rc = lsm303dlhc_write8(LSM303DLHC_ADDR_ACCEL,
        LSM303DLHC_REGISTER_ACCEL_CTRL_REG1_A,
        lsm->cfg.accel_rate | 0x07);
    if (rc != 0) {
        goto err;
    }

    /* Set scale */
    rc = lsm303dlhc_write8(LSM303DLHC_ADDR_ACCEL,
        LSM303DLHC_REGISTER_ACCEL_CTRL_REG4_A,
        lsm->cfg.accel_range);
    if (rc != 0) {
        goto err;
    }

err:
    return (rc);
}

static void *
lsm303dlhc_sensor_get_interface(struct sensor *sensor, sensor_type_t type)
{
    return (NULL);
}

static int
lsm303dlhc_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    struct lsm303dlhc *lsm;
    struct sensor_accel_data sad;
    int rc;
    int16_t x, y, z;
    float mg_lsb;

    /* If the read isn't looking for accel data, then don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER)) {
        rc = SYS_EINVAL;
        goto err;
    }

    lsm = (struct lsm303dlhc *) SENSOR_GET_DEVICE(sensor);

    x = y = z = 0;
    lsm303dlhc_read48(LSM303DLHC_ADDR_ACCEL,
                      LSM303DLHC_REGISTER_ACCEL_OUT_X_L_A,
                      &x, &y, &z);

    /* Determine mg per lsb based on range */
    switch(lsm->cfg.accel_range) {
        case LSM303DLHC_ACCEL_RANGE_2:
            #if MYNEWT_VAL(LSM303DLHC_STATS)
            STATS_INC(g_lsm303dlhcstats, samples_acc_2g);
            #endif
            mg_lsb = 0.001F;
            break;
        case LSM303DLHC_ACCEL_RANGE_4:
            #if MYNEWT_VAL(LSM303DLHC_STATS)
            STATS_INC(g_lsm303dlhcstats, samples_acc_4g);
            #endif
            mg_lsb = 0.002F;
            break;
        case LSM303DLHC_ACCEL_RANGE_8:
            #if MYNEWT_VAL(LSM303DLHC_STATS)
            STATS_INC(g_lsm303dlhcstats, samples_acc_8g);
            #endif
            mg_lsb = 0.004F;
            break;
        case LSM303DLHC_ACCEL_RANGE_16:
            #if MYNEWT_VAL(LSM303DLHC_STATS)
            STATS_INC(g_lsm303dlhcstats, samples_acc_16g);
            #endif
            mg_lsb = 0.012F;
            break;
        default:
            LSM303DLHC_ERR("Unknown accel range: 0x%02X. Assuming +/-2G.\n",
                lsm->cfg.accel_range);
            mg_lsb = 0.001F;
            break;
    }

    /* Convert from mg to Earth gravity in m/s^2 */
    sad.sad_x = (float)x * mg_lsb * 9.80665F;
    sad.sad_y = (float)y * mg_lsb * 9.80665F;
    sad.sad_z = (float)z * mg_lsb * 9.80665F;

    /* Call data function */
    rc = data_func(sensor, data_arg, &sad);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int
lsm303dlhc_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return (0);
err:
    return (rc);
}
