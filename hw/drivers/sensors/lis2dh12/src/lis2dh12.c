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

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "defs/error.h"
#include "os/os.h"
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "lis2dh12/lis2dh12.h"
#include "lis2dh12_priv.h"
#include "hal/hal_gpio.h"

static struct hal_spi_settings spi_lis2dh12_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};


#if MYNEWT_VAL(LIS2DH12_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(LIS2DH12_STATS)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(LIS2DH12_STATS)
/* Define the stats section and records */
STATS_SECT_START(lis2dh12_stat_section)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(read_errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lis2dh12_stat_section)
    STATS_NAME(lis2dh12_stat_section, write_errors)
    STATS_NAME(lis2dh12_stat_section, read_errors)
STATS_NAME_END(lis2dh12_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lis2dh12_stat_section) g_lis2dh12stats;
#endif

#if MYNEWT_VAL(LIS2DH12_LOG)
#define LOG_MODULE_LIS2DH12    (212)
#define LIS2DH12_INFO(...)     LOG_INFO(&_log, LOG_MODULE_LIS2DH12, __VA_ARGS__)
#define LIS2DH12_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_LIS2DH12, __VA_ARGS__)
static struct log _log;
#else
#define LIS2DH12_INFO(...)
#define LIS2DH12_ERR(...)
#endif

/* Exports for the sensor API */
static int lis2dh12_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int lis2dh12_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_lis2dh12_sensor_driver = {
    lis2dh12_sensor_read,
    lis2dh12_sensor_get_config
};

/**
 * Read multiple length data from LIS2DH12 sensor over I2C
 *
 * @param The sensor interface
 * @param register address
 * @param variable length buffer
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dh12_i2c_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *buffer,
                     uint8_t len)
{
    int rc;
    uint8_t payload[20] = { addr, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    /* Clear the supplied buffer */
    memset(buffer, 0, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LIS2DH12_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(LIS2DH12_STATS)
        STATS_INC(g_lis2dh12stats, read_errors);
#endif
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = hal_i2c_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LIS2DH12_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, addr);
#if MYNEWT_VAL(LIS2DH12_STATS)
        STATS_INC(g_lis2dh12stats, read_errors);
#endif
        goto err;
    }

    /* Copy the I2C results into the supplied buffer */
    memcpy(buffer, payload, len);

    return 0;
err:

    return rc;
}

/**
 * Read multiple length data from LIS2DH12 sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dh12_spi_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                     uint8_t len)
{
    int i;
    uint16_t retval;
    int rc;

    rc = 0;

    addr |= LIS2DH12_SPI_READ_CMD_BIT;

    /*
     * Auto register address increment is needed if the length
     * requested is more than 1
     */
    if (len > 1) {
        addr |= LIS2DH12_SPI_ADR_INC;
    }

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, addr);
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        LIS2DH12_ERR("SPI_%u register write failed addr:0x%02X\n",
                     itf->si_num, addr);
#if MYNEWT_VAL(LIS2DH12_STATS)
        STATS_INC(g_lis2dh12stats, read_errors);
#endif
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0x55);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            LIS2DH12_ERR("SPI_%u read failed addr:0x%02X\n",
                         itf->si_num, addr);
#if MYNEWT_VAL(LIS2DH12_STATS)
            STATS_INC(g_lis2dh12stats, read_errors);
#endif
            goto err;
        }
        payload[i] = retval;
    }

    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}


/**
 * Write multiple length data to LIS2DH12 sensor over I2C
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dh12_i2c_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *buffer,
                      uint8_t len)
{
    int rc;
    uint8_t payload[20] = { addr, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = len + 1,
        .buffer = payload
    };

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        LIS2DH12_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(LIS2DH12_STATS)
        STATS_INC(g_lis2dh12stats, write_errors);
#endif
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Write multiple length data to LIS2DH12 sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dh12_spi_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                      uint8_t len)
{
    int i;
    int rc;

    /*
     * Auto register address increment is needed if the length
     * requested is moret than 1
     */
    if (len > 1) {
        addr |= LIS2DH12_SPI_ADR_INC;
    }

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);


    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, addr);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        LIS2DH12_ERR("SPI_%u register write failed addr:0x%02X\n",
                     itf->si_num, addr);
#if MYNEWT_VAL(LIS2DH12_STATS)
        STATS_INC(g_lis2dh12stats, write_errors);
#endif
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        rc = hal_spi_tx_val(itf->si_num, payload[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            LIS2DH12_ERR("SPI_%u write failed addr:0x%02X:0x%02X\n",
                         itf->si_num, addr);
#if MYNEWT_VAL(LIS2DH12_STATS)
            STATS_INC(g_lis2dh12stats, write_errors);
#endif
            goto err;
        }
    }


    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}

/**
 * Write multiple length data to LIS2DH12 sensor over different interfaces
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                  uint8_t len)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lis2dh12_i2c_writelen(itf, addr, payload, len);
    } else {
        rc = lis2dh12_spi_writelen(itf, addr, payload, len);
    }

    return rc;
}

/**
 * Read multiple length data from LIS2DH12 sensor over different interfaces
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                 uint8_t len)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lis2dh12_i2c_readlen(itf, addr, payload, len);
    } else {
        rc = lis2dh12_spi_readlen(itf, addr, payload, len);
    }

    return rc;
}

/**
 * Reset lis2dh12
 *
 * @param The sensor interface
 */
int
lis2dh12_reset(struct sensor_itf *itf)
{
    int rc;
    uint8_t reg;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG5, &reg, 1);
    if (rc) {
        goto err;
    }

    reg |= LIS2DH12_CTRL_REG5_BOOT;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG5, &reg, 1);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 6/1000) + 1);

err:
    return rc;
}

/**
 * Pull up disconnect
 *
 * @param The sensor interface
 * @param disconnect pull up
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_pull_up_disc(struct sensor_itf *itf, uint8_t disconnect)
{
    uint8_t reg;

    reg = 0;

    reg |= ((disconnect ? LIS2DH12_CTRL_REG0_SPD : 0) |
            LIS2DH12_CTRL_REG0_CORR_OP);

    return lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG0, &reg, 1);
}

/**
 * Enable channels
 *
 * @param sensor interface
 * @param chan
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_chan_enable(struct sensor_itf *itf, uint8_t chan)
{
    uint8_t reg;
    int rc;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG1, &reg, 1);
    if (rc) {
        goto err;
    }

    reg &= 0xF0;
    reg |= chan;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG1, &reg, 1);

err:
    return rc;
}

/**
 * Get chip ID
 *
 * @param sensor interface
 * @param ptr to chip id to be filled up
 */
int
lis2dh12_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id)
{
    uint8_t reg;
    int rc;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_WHO_AM_I, &reg, 1);

    if (rc) {
        goto err;
    }

    *chip_id = reg;

err:
    return rc;
}

/**
 * Sets the full scale selection
 *
 * @param The sensor interface
 * @param The fs setting
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_full_scale(struct sensor_itf *itf, uint8_t fs)
{
    int rc;
    uint8_t reg;

    if (fs > LIS2DH12_FS_16G) {
        LIS2DH12_ERR("Invalid full scale value\n");
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG4,
                          &reg, 1);
    if (rc) {
        goto err;
    }

    reg = (reg & ~LIS2DH12_CTRL_REG4_FS) | fs;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG4,
                           &reg, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the full scale selection
 *
 * @param The sensor interface
 * @param ptr to fs
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_full_scale(struct sensor_itf *itf, uint8_t *fs)
{
    int rc;
    uint8_t reg;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG4,
                          &reg, 1);
    if (rc) {
        goto err;
    }

    *fs = (reg & LIS2DH12_CTRL_REG4_FS) >> 4;

    return 0;
err:
    return rc;
}

/**
 * Calculates the acceleration in m/s^2 from mg
 *
 * @param raw acc value
 * @param float ptr to return calculated value
 */
void
lis2dh12_calc_acc_ms2(int16_t raw_acc, float *facc)
{
    *facc = raw_acc * STANDARD_ACCEL_GRAVITY;
}

/**
 * Sets the rate
 *
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_rate(struct sensor_itf *itf, uint8_t rate)
{
    int rc;
    uint8_t reg;

    if (rate > LIS2DH12_DATA_RATE_HN_1344HZ_L_5376HZ) {
        LIS2DH12_ERR("Invalid rate value\n");
        rc = SYS_EINVAL;
        goto err;
    }

    /*
     * As per the datasheet, REFERENCE(26h) needs to be read
     * for a reset of the filter block before switching to
     * normal/high-performance mode from power down mode
     */
    if (rate != LIS2DH12_DATA_RATE_0HZ || rate != LIS2DH12_DATA_RATE_L_1620HZ) {

        rc = lis2dh12_readlen(itf, LIS2DH12_REG_REFERENCE, &reg, 1);
        if (rc) {
            goto err;
        }
    }

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG1,
                          &reg, 1);
    if (rc) {
        goto err;
    }

    reg = (reg & ~LIS2DH12_CTRL_REG1_ODR) | rate;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG1,
                           &reg, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the rate
 *
 * @param The sensor ineterface
 * @param ptr to the rate
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_rate(struct sensor_itf *itf, uint8_t *rate)
{
    int rc;
    uint8_t reg;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG1, &reg, 1);
    if (rc) {
        goto err;
    }

    *rate = reg & LIS2DH12_CTRL_REG1_ODR;

    return 0;
err:
    return rc;
}

/**
 *
 * Get operating mode
 *
 * @param the sensor interface
 * @param ptr to mode
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_op_mode(struct sensor_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t reg1;
    uint8_t reg4;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG1, &reg1, 1);
    if (rc) {
        goto err;
    }

    reg1 = (reg1 & LIS2DH12_CTRL_REG1_LPEN) << 4;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG4, &reg4, 1);
    if (rc) {
        goto err;
    }

    reg4 = (reg4 & LIS2DH12_CTRL_REG4_HR);

    *mode = reg1 | reg4;

    return 0;
err:
    return rc;
}

/**
 * Set operating mode
 *
 * @param the sensor interface
 * @param mode CTRL_REG1[3:0]:CTRL_REG4[3:0]
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_op_mode(struct sensor_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG1, &reg, 1);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DH12_CTRL_REG1_LPEN;
    reg |= ((mode & 0x80) >> 4);

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG1, &reg, 1);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG4, &reg, 1);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DH12_CTRL_REG4_HR;
    reg |= (mode & 0x08);

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG4, &reg, 1);
    if (rc) {
        goto err;
    }

    os_time_delay(OS_TICKS_PER_SEC/1000 + 1);

    return 0;
err:
    return rc;
}

/**
 * Gets a new data sample from the light sensor.
 *
 * @param The sensor interface
 * @param x axis data
 * @param y axis data
 * @param z axis data
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_data(struct sensor_itf *itf, int16_t *x, int16_t *y, int16_t *z)
{
    int rc;
    uint8_t payload[6] = {0};
    uint8_t fs;

    *x = *y = *z = 0;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_OUT_X_L, payload, 1);
    rc |= lis2dh12_readlen(itf, LIS2DH12_REG_OUT_X_H, &payload[1], 1);
    rc |= lis2dh12_readlen(itf, LIS2DH12_REG_OUT_Y_L, &payload[2], 1);
    rc |= lis2dh12_readlen(itf, LIS2DH12_REG_OUT_Y_H, &payload[3], 1);
    rc |= lis2dh12_readlen(itf, LIS2DH12_REG_OUT_Z_L, &payload[4], 1);
    rc |= lis2dh12_readlen(itf, LIS2DH12_REG_OUT_Z_H, &payload[5], 1);
    if (rc) {
        goto err;
    }

    *x = payload[0] | (payload[1] << 8);
    *y = payload[2] | (payload[3] << 8);
    *z = payload[4] | (payload[5] << 8);

    rc = lis2dh12_get_full_scale(itf, &fs);
    if (rc) {
        goto err;
    }

    if (fs == LIS2DH12_FS_2G) {
        fs = 2;
    } else if (fs == LIS2DH12_FS_4G) {
        fs = 4;
    } else if (fs == LIS2DH12_FS_8G) {
        fs = 8;
    } else if (fs == LIS2DH12_FS_16G) {
        fs = 16;
    } else {
        rc = SYS_EINVAL;
        goto err;
    }

    /*
     * Since full scale is +/-(fs)g,
     * fs should be multiplied by 2 to account for full scale.
     * To calculate mg from g we use the 1000 multiple.
     * Since the full scale is represented by 16 bit value,
     * we use that as a divisor.
     * The calculation is based on an example present in AN5005
     * application note
     */
    *x = (fs * 2 * 1000 * *x)/UINT16_MAX;
    *y = (fs * 2 * 1000 * *y)/UINT16_MAX;
    *z = (fs * 2 * 1000 * *z)/UINT16_MAX;

    return 0;
err:
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
lis2dh12_init(struct os_dev *dev, void *arg)
{
    struct lis2dh12 *lis2dh12;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    lis2dh12 = (struct lis2dh12 *) dev;

    lis2dh12->cfg.lc_s_mask = SENSOR_TYPE_ALL;

#if MYNEWT_VAL(LIS2DH12_LOG)
    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

    sensor = &lis2dh12->sensor;

#if MYNEWT_VAL(LIS2DH12_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lis2dh12stats),
        STATS_SIZE_INIT_PARMS(g_lis2dh12stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lis2dh12_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lis2dh12stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
    rc = sensor_init(sensor, dev);
    if (rc) {
        goto err;
    }

    /* Add the light driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER,
            (struct sensor_driver *) &g_lis2dh12_sensor_driver);
    if (rc) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc) {
        goto err;
    }

    if (sensor->s_itf.si_type == SENSOR_ITF_SPI) {

        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_lis2dh12_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
             * fail if the spi is already enabled
             */
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_gpio_init_out(sensor->s_itf.si_cs_pin, 1);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;

}

/**
 * Self test mode
 *
 * @param the sensor interface
 * @param mode to set
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_self_test_mode(struct sensor_itf *itf, uint8_t mode)
{
    uint8_t reg;
    int rc;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG4, &reg, 1);
    if (rc) {
        goto err;
    }

    reg &= ~LIS2DH12_CTRL_REG4_ST;

    reg |= mode;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG4, &reg, 1);

err:
    return rc;
}

static int
lis2dh12_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    struct sensor_accel_data sad;
    struct sensor_itf *itf;
    int16_t x, y ,z;
    float fx, fy ,fz;
    int rc;

    /* If the read isn't looking for accel or mag data, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);

    x = y = z = 0;

    if (itf->si_type == SENSOR_ITF_SPI) {

        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_lis2dh12_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
             * fail if the spi is already enabled
             */
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }
    }

    rc = lis2dh12_get_data(itf, &x, &y, &z);
    if (rc) {
        goto err;
    }

    lis2dh12_calc_acc_ms2(x, &fx);
    lis2dh12_calc_acc_ms2(y, &fy);
    lis2dh12_calc_acc_ms2(z, &fz);

    sad.sad_x = fx;
    sad.sad_y = fy;
    sad.sad_z = fz;

    sad.sad_x_is_valid = 1;
    sad.sad_y_is_valid = 1;
    sad.sad_z_is_valid = 1;

    /* Call data function */
    rc = data_func(sensor, data_arg, &sad, SENSOR_TYPE_ACCELEROMETER);
    if (rc != 0) {
        goto err;
    }

    return 0;
err:
    return rc;
}

static int
lis2dh12_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return 0;
err:
    return rc;
}

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int
lis2dh12_config(struct lis2dh12 *lis2dh12, struct lis2dh12_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;
    uint8_t chip_id;
    struct sensor *sensor;

    itf = SENSOR_GET_ITF(&(lis2dh12->sensor));
    sensor = &(lis2dh12->sensor);

    if (itf->si_type == SENSOR_ITF_SPI) {

        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_lis2dh12_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
             * fail if the spi is already enabled
             */
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }
    }

    rc = lis2dh12_get_chip_id(itf, &chip_id);
    if (rc) {
        goto err;
    }

    if (chip_id != LIS2DH12_ID) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dh12_reset(itf);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_pull_up_disc(itf, cfg->lc_pull_up_disc);
    if (rc) {
        goto err;
    }

    lis2dh12->cfg.lc_pull_up_disc = cfg->lc_pull_up_disc;

    rc = lis2dh12_set_full_scale(itf, cfg->lc_fs);
    if (rc) {
        goto err;
    }

    lis2dh12->cfg.lc_fs = cfg->lc_fs;

    rc = lis2dh12_set_rate(itf, cfg->lc_rate);
    if (rc) {
        goto err;
    }

    lis2dh12->cfg.lc_rate = cfg->lc_rate;

    rc = lis2dh12_chan_enable(itf, LIS2DH12_CTRL_REG1_XPEN |
                                   LIS2DH12_CTRL_REG1_YPEN |
                                   LIS2DH12_CTRL_REG1_ZPEN);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_set_self_test_mode(itf, LIS2DH12_ST_MODE_DISABLE);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_set_op_mode(itf, LIS2DH12_OM_HIGH_RESOLUTION);
    if (rc) {
        goto err;
    }

    rc = sensor_set_type_mask(&(lis2dh12->sensor), cfg->lc_s_mask);
    if (rc) {
        goto err;
    }

    lis2dh12->cfg.lc_s_mask = cfg->lc_s_mask;

    return 0;
err:
    return rc;
}
