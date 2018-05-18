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

#include "os/mynewt.h"
#include "hal/hal_i2c.h"
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/gyro.h"
#include "mpu6050/mpu6050.h"
#include "mpu6050_priv.h"
#include "log/log.h"
#include "stats/stats.h"

/* Define the stats section and records */
STATS_SECT_START(mpu6050_stat_section)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(mpu6050_stat_section)
    STATS_NAME(mpu6050_stat_section, read_errors)
    STATS_NAME(mpu6050_stat_section, write_errors)
STATS_NAME_END(mpu6050_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(mpu6050_stat_section) g_mpu6050stats;

#define LOG_MODULE_MPU6050    (6050)
#define MPU6050_INFO(...)     LOG_INFO(&_log, LOG_MODULE_MPU6050, __VA_ARGS__)
#define MPU6050_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_MPU6050, __VA_ARGS__)
static struct log _log;

/* Exports for the sensor API */
static int mpu6050_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int mpu6050_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_mpu6050_sensor_driver = {
    mpu6050_sensor_read,
    mpu6050_sensor_get_config
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
mpu6050_write8(struct sensor_itf *itf, uint8_t reg, uint32_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        MPU6050_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02lX\n",
                       itf->si_addr, reg, value);
        STATS_INC(g_mpu6050stats, read_errors);
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
mpu6050_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &reg
    };

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 0);
    if (rc) {
        MPU6050_ERR("I2C access failed at address 0x%02X\n", itf->si_addr);
        STATS_INC(g_mpu6050stats, write_errors);
        return rc;
    }

    /* Read one byte back */
    data_struct.buffer = value;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
         MPU6050_ERR("Failed to read from 0x%02X:0x%02X\n", itf->si_addr, reg);
         STATS_INC(g_mpu6050stats, read_errors);
    }
    return rc;
}

/**
 * Reads a six bytes from the specified register
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
mpu6050_read48(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &reg
    };

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 0);
    if (rc) {
        MPU6050_ERR("I2C access failed at address 0x%02X\n", itf->si_addr);
        STATS_INC(g_mpu6050stats, write_errors);
        return rc;
    }

    /* Read six bytes back */
    data_struct.len = 6;
    data_struct.buffer = buffer;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
         MPU6050_ERR("Failed to read from 0x%02X:0x%02X\n", itf->si_addr, reg);
         STATS_INC(g_mpu6050stats, read_errors);
    }
    return rc;
}

int
mpu6050_reset(struct sensor_itf *itf)
{
    return mpu6050_write8(itf, MPU6050_PWR_MGMT_1, MPU6050_DEVICE_RESET);
}

int
mpu6050_sleep(struct sensor_itf *itf, uint8_t enable)
{
    uint8_t reg;
    int rc;

    rc = mpu6050_read8(itf, MPU6050_PWR_MGMT_1, &reg);
    if (rc) {
        return rc;
    }

    if (enable) {
        reg |= MPU6050_SLEEP;
    } else {
        reg &= ~MPU6050_SLEEP;
    }

    return mpu6050_write8(itf, MPU6050_PWR_MGMT_1, reg);
}

int
mpu6050_set_clock_source(struct sensor_itf *itf,
    enum mpu6050_clock_select source)
{
    uint8_t reg;
    int rc;

    rc = mpu6050_read8(itf, MPU6050_PWR_MGMT_1, &reg);
    if (rc) {
        return rc;
    }

    reg &= 0xf8;
    reg |= source & 0x07;

    return mpu6050_write8(itf, MPU6050_PWR_MGMT_1, reg);
}

int
mpu6050_get_clock_source(struct sensor_itf *itf,
        enum mpu6050_clock_select *source)
{
    uint8_t reg;
    int rc;

    rc = mpu6050_read8(itf, MPU6050_PWR_MGMT_1, &reg);
    if (rc) {
        return rc;
    }

    *source = (enum mpu6050_clock_select)(reg & 0x07);

    return 0;
}

int
mpu6050_set_lpf(struct sensor_itf *itf, uint8_t cfg)
{
    return mpu6050_write8(itf, MPU6050_CONFIG, cfg & 0x07);
}

int
mpu6050_get_lpf(struct sensor_itf *itf, uint8_t *cfg)
{
    uint8_t reg;
    int rc;

    rc = mpu6050_read8(itf, MPU6050_CONFIG, &reg);
    if (rc) {
        return rc;
    }

    *cfg = reg & 0x07;

    return 0;
}

int
mpu6050_set_sample_rate(struct sensor_itf *itf, uint8_t rate_div)
{
    return mpu6050_write8(itf, MPU6050_SMPRT_DIV, rate_div);
}

int
mpu6050_get_sample_rate(struct sensor_itf *itf, uint8_t *rate_div)
{
    return mpu6050_read8(itf, MPU6050_SMPRT_DIV, rate_div);
}

int
mpu6050_set_gyro_range(struct sensor_itf *itf, enum mpu6050_gyro_range range)
{
    return mpu6050_write8(itf, MPU6050_GYRO_CONFIG, (uint8_t)range);
}

int
mpu6050_get_gyro_range(struct sensor_itf *itf, enum mpu6050_gyro_range *range)
{
    uint8_t reg;
    int rc;

    rc = mpu6050_read8(itf, MPU6050_GYRO_CONFIG, &reg);
    if (rc) {
        return rc;
    }

    *range = (enum mpu6050_gyro_range)(reg & 0x18);

    return 0;
}

int
mpu6050_set_accel_range(struct sensor_itf *itf, enum mpu6050_accel_range range)
{
    return mpu6050_write8(itf, MPU6050_ACCEL_CONFIG, (uint8_t)range);
}

int
mpu6050_get_accel_range(struct sensor_itf *itf, enum mpu6050_accel_range *range)
{
    uint8_t reg;
    int rc;

    rc = mpu6050_read8(itf, MPU6050_ACCEL_CONFIG, &reg);
    if (rc) {
        return rc;
    }

    *range = (enum mpu6050_accel_range)(reg & 0x18);

    return 0;
}

int
mpu6050_enable_interrupt(struct sensor_itf *itf, uint8_t enable)
{
    uint8_t reg;
    int rc;

    rc = mpu6050_read8(itf, MPU6050_INT_ENABLE, &reg);
    if (rc) {
        return rc;
    }

    if (enable) {
        reg |= MPU6050_DATA_RDY_EN;
    } else {
        reg &= ~MPU6050_DATA_RDY_EN;
    }

    return mpu6050_write8(itf, MPU6050_INT_ENABLE, reg);
}

int
mpu6050_config_interrupt(struct sensor_itf *itf, uint8_t cfg)
{
    uint8_t reg;
    int rc;

    rc = mpu6050_read8(itf, MPU6050_INT_PIN_CFG, &reg);
    if (rc) {
        return rc;
    }

    reg &= 0x0f;
    reg |= cfg & 0xf0;

    return mpu6050_write8(itf, MPU6050_INT_PIN_CFG, reg);
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accelerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
mpu6050_init(struct os_dev *dev, void *arg)
{
    struct mpu6050 *mpu;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        return SYS_ENODEV;
    }

    mpu = (struct mpu6050 *) dev;

    mpu->cfg.mask = SENSOR_TYPE_ALL;

    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    sensor = &mpu->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_mpu6050stats),
        STATS_SIZE_INIT_PARMS(g_mpu6050stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(mpu6050_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_mpu6050stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc) {
        return rc;
    }

    /* Add the accelerometer/gyroscope driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_GYROSCOPE |
        SENSOR_TYPE_ACCELEROMETER,
            (struct sensor_driver *) &g_mpu6050_sensor_driver);
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
mpu6050_config(struct mpu6050 *mpu, struct mpu6050_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(mpu->sensor));

    /* Wake up */
    rc = mpu6050_sleep(itf, 0);
    if (rc) {
        return rc;
    }

    rc = mpu6050_set_clock_source(itf, cfg->clock_source);
    if (rc) {
        return rc;
    }

    mpu->cfg.clock_source = cfg->clock_source;

    uint8_t val;
    rc = mpu6050_read8(itf, MPU6050_WHO_AM_I, &val);
    if (rc) {
        return rc;
    }
    if (val != MPU6050_WHO_AM_I_VAL) {
        return SYS_EINVAL;
    }

    rc = mpu6050_set_lpf(itf, cfg->lpf_cfg);
    if (rc) {
        return rc;
    }

    mpu->cfg.lpf_cfg = cfg->lpf_cfg;

    rc = mpu6050_set_sample_rate(itf, cfg->sample_rate_div);
    if (rc) {
        return rc;
    }

    mpu->cfg.sample_rate_div = cfg->sample_rate_div;

    rc = mpu6050_set_gyro_range(itf, cfg->gyro_range);
    if (rc) {
        return rc;
    }

    mpu->cfg.gyro_range = cfg->gyro_range;

    rc = mpu6050_set_accel_range(itf, cfg->accel_range);
    if (rc) {
        return rc;
    }

    mpu->cfg.accel_range = cfg->accel_range;

    rc = mpu6050_config_interrupt(itf, cfg->int_cfg);
    if (rc) {
        return rc;
    }

    mpu->cfg.int_cfg = cfg->int_cfg;

    /* Enable/disable interrupt */
    rc = mpu6050_enable_interrupt(itf, cfg->int_enable);
    if (rc) {
        return rc;
    }

    mpu->cfg.int_enable = cfg->int_enable;

    rc = sensor_set_type_mask(&(mpu->sensor), cfg->mask);
    if (rc) {
        return rc;
    }

    mpu->cfg.mask = cfg->mask;

    return 0;
}

static int
mpu6050_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    (void)timeout;
    int rc;
    int16_t x, y, z;
    uint8_t payload[6];
    float lsb;
    struct sensor_itf *itf;
    struct mpu6050 *mpu;
    union {
        struct sensor_accel_data sad;
        struct sensor_gyro_data sgd;
    } databuf;

    /* If the read isn't looking for accel or gyro, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER) &&
       (!(type & SENSOR_TYPE_GYROSCOPE))) {
        return SYS_EINVAL;
    }

    itf = SENSOR_GET_ITF(sensor);
    mpu = (struct mpu6050 *) SENSOR_GET_DEVICE(sensor);

    /* Get a new accelerometer sample */
    if (type & SENSOR_TYPE_ACCELEROMETER) {
        rc = mpu6050_read48(itf, MPU6050_ACCEL_XOUT_H, payload);
        if (rc) {
            return rc;
        }

        x = (((int16_t)payload[0]) << 8) | payload[1];
        y = (((int16_t)payload[2]) << 8) | payload[3];
        z = (((int16_t)payload[4]) << 8) | payload[5];

        switch (mpu->cfg.accel_range) {
            case MPU6050_ACCEL_RANGE_2: /* +/- 2g - 16384 LSB/g */
            /* Falls through */
            default:
                lsb = 16384.0F;
            break;
            case MPU6050_ACCEL_RANGE_4: /* +/- 4g - 8192 LSB/g */
                lsb = 8192.0F;
            break;
            case MPU6050_ACCEL_RANGE_8: /* +/- 8g - 4096 LSB/g */
                lsb = 4096.0F;
            break;
            case MPU6050_ACCEL_RANGE_16: /* +/- 16g - 2048 LSB/g */
                lsb = 2048.0F;
            break;
        }

        databuf.sad.sad_x = (x / lsb) * STANDARD_ACCEL_GRAVITY;
        databuf.sad.sad_x_is_valid = 1;
        databuf.sad.sad_y = (y / lsb) * STANDARD_ACCEL_GRAVITY;
        databuf.sad.sad_y_is_valid = 1;
        databuf.sad.sad_z = (z / lsb) * STANDARD_ACCEL_GRAVITY;
        databuf.sad.sad_z_is_valid = 1;

        rc = data_func(sensor, data_arg, &databuf.sad,
                SENSOR_TYPE_ACCELEROMETER);
        if (rc) {
            return rc;
        }
    }

    /* Get a new gyroscope sample */
    if (type & SENSOR_TYPE_GYROSCOPE) {
        rc = mpu6050_read48(itf, MPU6050_GYRO_XOUT_H, payload);
        if (rc) {
            return rc;
        }

        x = (((int16_t)payload[0]) << 8) | payload[1];
        y = (((int16_t)payload[2]) << 8) | payload[3];
        z = (((int16_t)payload[4]) << 8) | payload[5];

        switch (mpu->cfg.gyro_range) {
            case MPU6050_GYRO_RANGE_250: /* +/- 250 Deg/s - 131 LSB/Deg/s */
            /* Falls through */
            default:
                lsb = 131.0F;
            break;
            case MPU6050_GYRO_RANGE_500: /* +/- 500 Deg/s - 65.5 LSB/Deg/s */
                lsb = 65.5F;
            break;
            case MPU6050_GYRO_RANGE_1000: /* +/- 1000 Deg/s - 32.8 LSB/Deg/s */
                lsb = 32.8F;
            break;
            case MPU6050_GYRO_RANGE_2000: /* +/- 2000 Deg/s - 16.4 LSB/Deg/s */
                lsb = 16.4F;
            break;
        }

        databuf.sgd.sgd_x = x / lsb;
        databuf.sgd.sgd_x_is_valid = 1;
        databuf.sgd.sgd_y = y / lsb;
        databuf.sgd.sgd_y_is_valid = 1;
        databuf.sgd.sgd_z = z / lsb;
        databuf.sgd.sgd_z_is_valid = 1;

        rc = data_func(sensor, data_arg, &databuf.sgd, SENSOR_TYPE_GYROSCOPE);
        if (rc) {
            return rc;
        }
    }

    return 0;
}

static int
mpu6050_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    /* If the read isn't looking for accel or gyro, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER) &&
       (!(type & SENSOR_TYPE_GYROSCOPE))) {
        return SYS_EINVAL;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return 0;
}
