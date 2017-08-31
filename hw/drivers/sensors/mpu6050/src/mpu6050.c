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
#include "sensor/gyro.h"
#include "mpu6050/mpu6050.h"
#include "mpu6050_priv.h"

#if MYNEWT_VAL(MPU6050_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(MPU6050_STATS)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(MPU6050_STATS)
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
#endif

#if MYNEWT_VAL(MPU6050_LOG)
#define LOG_MODULE_MPU6050    (6050)
#define MPU6050_INFO(...)     LOG_INFO(&_log, LOG_MODULE_MPU6050, __VA_ARGS__)
#define MPU6050_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_MPU6050, __VA_ARGS__)
static struct log _log;
#else
#define MPU6050_INFO(...)
#define MPU6050_ERR(...)
#endif

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
 * @param The I2C address to use
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
mpu6050_write8(struct sensor_itf *itf, uint8_t addr, uint8_t reg,
                  uint32_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);

    if (rc != 0) {
        MPU6050_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                       addr, reg, value);
#if MYNEWT_VAL(MPU6050_STATS)
        STATS_INC(g_mpu6050stats, read_errors);
#endif
    }

    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @param The sensor interface
 * @param The I2C address to use
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
mpu6050_read8(struct sensor_itf *itf, uint8_t addr, uint8_t reg,
                 uint8_t *value)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 1,
        .buffer = &reg
    };

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 0);
    if (rc != 0) {
        MPU6050_ERR("I2C access failed at address 0x%02X\n", addr);
#if MYNEWT_VAL(MPU6050_STATS)
        STATS_INC(g_mpu6050stats, write_errors);
#endif
        return rc;
    }

    /* Read one byte back */
    data_struct.buffer = value;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc != 0) {
         MPU6050_ERR("Failed to read from 0x%02X:0x%02X\n", addr, reg);
 #if MYNEWT_VAL(MPU6050_STATS)
         STATS_INC(g_mpu6050stats, read_errors);
 #endif
    }
    return rc;
}

/**
 * Reads a six bytes from the specified register
 *
 * @param The sensor interface
 * @param The I2C address to use
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
mpu6050_read48(struct sensor_itf *itf, uint8_t addr, uint8_t reg,
                  uint8_t *buffer)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = addr,
        .len = 1,
        .buffer = &reg
    };

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 0);
    if (rc != 0) {
        MPU6050_ERR("I2C access failed at address 0x%02X\n", addr);
#if MYNEWT_VAL(MPU6050_STATS)
        STATS_INC(g_mpu6050stats, write_errors);
#endif
        return rc;
    }

    /* Read six bytes back */
    data_struct.len = 6;
    data_struct.buffer = buffer;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc != 0) {
         MPU6050_ERR("Failed to read from 0x%02X:0x%02X\n", addr, reg);
 #if MYNEWT_VAL(MPU6050_STATS)
         STATS_INC(g_mpu6050stats, read_errors);
 #endif
    }
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

#if MYNEWT_VAL(MPU6050_LOG)
    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

    sensor = &mpu->sensor;

#if MYNEWT_VAL(MPU6050_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_mpu6050stats),
        STATS_SIZE_INIT_PARMS(g_mpu6050stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(mpu6050_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_mpu6050stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        return rc;
    }

    /* Add the accelerometer/gyroscope driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_GYROSCOPE |
        SENSOR_TYPE_ACCELEROMETER,
            (struct sensor_driver *) &g_mpu6050_sensor_driver);
    if (rc != 0) {
        return rc;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc != 0) {
        return rc;
    }

    rc = sensor_mgr_register(sensor);
    return rc;
}

int
mpu6050_config(struct mpu6050 *mpu, struct mpu6050_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(mpu->sensor));

    /* Power on */
    rc = mpu6050_write8(itf, cfg->addr, MPU6050_PWR_MGMT_1, 1);
    if (rc != 0) {
        return rc;
    }

    uint8_t val;
    rc = mpu6050_read8(itf, cfg->addr, MPU6050_WHO_AM_I, &val);
    if (rc != 0) {
        return rc;
    }
    if (val != MPU6050_WHO_AM_I_VAL) {
        return SYS_EIO;
    }

    /* Set LPF */
    rc = mpu6050_write8(itf, cfg->addr, MPU6050_CONFIG, cfg->lpf_cfg);
    if (rc != 0) {
        return rc;
    }

    mpu->cfg.lpf_cfg = cfg->lpf_cfg;

    /* Set gyro data rate */
    rc = mpu6050_write8(itf, cfg->addr, MPU6050_SMPRT_DIV, cfg->gyro_rate_div);
    if (rc != 0) {
        return rc;
    }

    mpu->cfg.gyro_rate_div = cfg->gyro_rate_div;

    /* Set the gyroscope range */
    rc = mpu6050_write8(itf, cfg->addr, MPU6050_GYRO_CONFIG, cfg->gyro_range);
    if (rc != 0) {
        return rc;
    }

    mpu->cfg.gyro_range = cfg->gyro_range;

    /* Set accelerometer range */
    rc = mpu6050_write8(itf, cfg->addr, MPU6050_ACCEL_CONFIG, cfg->accel_range);
    if (rc != 0) {
        return rc;
    }

    mpu->cfg.accel_range = cfg->accel_range;

    rc = sensor_set_type_mask(&(mpu->sensor),  cfg->mask);
    if (rc != 0) {
        return rc;
    }

    mpu->cfg.mask = cfg->mask;
    mpu->cfg.addr = cfg->addr;

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
        rc = mpu6050_read48(itf, mpu->cfg.addr, MPU6050_ACCEL_XOUT_H,
                          payload);
        if (rc != 0) {
            return rc;
        }

        x = (int16_t)((payload[0] << 8) | payload[1]);
        y = (int16_t)((payload[2] << 8) | payload[3]);
        z = (int16_t)((payload[4] << 8) | payload[5]);

        switch (mpu->cfg.gyro_range) {
            case MPU6050_ACCEL_RANGE_2: /* +/- 2g - 16384 LSB/g */
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
        if (rc != 0) {
            return rc;
        }
    }

    /* Get a new gyroscope sample */
    if (type & SENSOR_TYPE_GYROSCOPE) {
        rc = mpu6050_read48(itf, mpu->cfg.addr, MPU6050_GYRO_XOUT_H,
                          payload);
        if (rc != 0) {
            return rc;
        }

        x = (int16_t)((payload[0] << 8) | payload[1]);
        y = (int16_t)((payload[2] << 8) | payload[3]);
        z = (int16_t)((payload[4] << 8) | payload[5]);

        switch (mpu->cfg.gyro_range) {
            case MPU6050_GYRO_RANGE_250: /* +/- 250 Deg/s - 131 LSB/Deg/s */
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
        if (rc != 0) {
            return rc;
        }
    }

    return rc;
}

static int
mpu6050_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    /* If the read isn't looking for accel or gyro, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER) &&
       (!(type & SENSOR_TYPE_GYROSCOPE))) {
        return SYS_EINVAL;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return (rc);
}
