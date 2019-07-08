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

/**
 * Driver for 6 axis IMU LSM6DSO
 * For more details please refers to www.st.com AN5192
 */
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "os/mynewt.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#else /* BUS_DRIVER_PRESENT */
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"
#endif /* BUS_DRIVER_PRESENT */
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/gyro.h"
#include "sensor/temperature.h"
#include "lsm6dso/lsm6dso.h"
#include "lsm6dso_priv.h"
#include "hal/hal_gpio.h"
#include "modlog/modlog.h"
#include "stats/stats.h"
#include <syscfg/syscfg.h>

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
static struct hal_spi_settings spi_lsm6dso_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};
#endif /* BUS_DRIVER_PRESENT */

/* Default event notification */
const struct lsm6dso_notif_cfg dflt_notif_cfg[] = {
    {
        .event = SENSOR_EVENT_TYPE_SINGLE_TAP,
        .int_num = 0,
        .int_mask = LSM6DSO_INT_SINGLE_TAP,
        .int_en = LSM6DSO_INT1_SINGLE_TAP_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_DOUBLE_TAP,
        .int_num = 0,
        .int_mask = LSM6DSO_INT_DOUBLE_TAP,
        .int_en = LSM6DSO_INT1_DOUBLE_TAP_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_FREE_FALL,
        .int_num = 0,
        .int_mask = LSM6DSO_INT_FF,
        .int_en = LSM6DSO_INT1_FF_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_WAKEUP,
        .int_num = 0,
        .int_mask = LSM6DSO_INT_WU,
        .int_en = LSM6DSO_INT1_WU_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_SLEEP,
        .int_num = 0,
        .int_mask = LSM6DSO_INT_SLEEP_CHANGE,
        .int_en = LSM6DSO_INT1_SLEEP_CHANGE_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_ORIENT_CHANGE,
        .int_num = 0,
        .int_mask = LSM6DSO_INT_6D,
        .int_en = LSM6DSO_INT1_6D_MASK
    }
};

/* Define the stats section and records */
STATS_SECT_START(lsm6dso_stat_section)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(read_errors)
#if MYNEWT_VAL(LSM6DSO_NOTIF_STATS)
    STATS_SECT_ENTRY(single_tap_notify)
    STATS_SECT_ENTRY(double_tap_notify)
    STATS_SECT_ENTRY(free_fall_notify)
    STATS_SECT_ENTRY(sleep_notify)
    STATS_SECT_ENTRY(orientation_notify)
    STATS_SECT_ENTRY(wakeup_notify)
#endif /* LSM6DSO_NOTIF_STATS */
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lsm6dso_stat_section)
    STATS_NAME(lsm6dso_stat_section, write_errors)
    STATS_NAME(lsm6dso_stat_section, read_errors)
#if MYNEWT_VAL(LSM6DSO_NOTIF_STATS)
    STATS_NAME(lsm6dso_stat_section, single_tap_notify)
    STATS_NAME(lsm6dso_stat_section, double_tap_notify)
    STATS_NAME(lsm6dso_stat_section, free_fall_notify)
    STATS_NAME(lsm6dso_stat_section, sleep_notify)
    STATS_NAME(lsm6dso_stat_section, orientation_notify)
    STATS_NAME(lsm6dso_stat_section, wakeup_notify)
#endif /* LSM6DSO_NOTIF_STATS */
STATS_NAME_END(lsm6dso_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lsm6dso_stat_section) g_lsm6dsostats;

#define LSM6DSO_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(LSM6DSO_LOG_MODULE), __VA_ARGS__)

/* Exports for the sensor API */
static int lsm6dso_sensor_read(struct sensor *, sensor_type_t,
                               sensor_data_func_t, void *, uint32_t);
static int lsm6dso_sensor_get_config(struct sensor *, sensor_type_t,
                                     struct sensor_cfg *);
static int lsm6dso_sensor_set_notification(struct sensor *,
                                           sensor_event_type_t);
static int lsm6dso_sensor_unset_notification(struct sensor *,
                                             sensor_event_type_t);
static int lsm6dso_sensor_handle_interrupt(struct sensor *);
static int lsm6dso_sensor_set_config(struct sensor *, void *);
static int lsm6dso_sensor_reset(struct sensor *);

static const struct sensor_driver g_lsm6dso_sensor_driver = {
    .sd_read               = lsm6dso_sensor_read,
    .sd_get_config         = lsm6dso_sensor_get_config,
    .sd_set_config         = lsm6dso_sensor_set_config,
    .sd_set_notification   = lsm6dso_sensor_set_notification,
    .sd_unset_notification = lsm6dso_sensor_unset_notification,
    .sd_handle_interrupt   = lsm6dso_sensor_handle_interrupt,
    .sd_reset              = lsm6dso_sensor_reset,
};

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Read multiple length data from LSM6DSO sensor over I2C
 *
 * @param The sensor interface
 * @param register address
 * @param variable length buffer
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_i2c_readlen(struct sensor_itf *itf, uint8_t addr,
                               uint8_t *buffer, uint8_t len)
{
    int rc;
    uint8_t payload[20];
    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    if (len > sizeof(payload))
        return OS_EINVAL;

    /* First byte is register address */
    payload[0] = addr;
    rc = i2cn_master_write(itf->si_num,
                           &data_struct, MYNEWT_VAL(LSM6DSO_I2C_TIMEOUT_TICKS),
                           1,
                           MYNEWT_VAL(LSM6DSO_I2C_RETRIES));
    if (rc) {
        LSM6DSO_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                     data_struct.address);
        STATS_INC(g_lsm6dsostats, read_errors);
        goto err;
    }

    memset(payload, 0, len);
    data_struct.len = len;

    /* Read data from register */
    rc = i2cn_master_read(itf->si_num, &data_struct,
                          MYNEWT_VAL(LSM6DSO_I2C_TIMEOUT_TICKS), len,
                          MYNEWT_VAL(LSM6DSO_I2C_RETRIES));
    if (rc) {
        LSM6DSO_LOG(ERROR, "Failed to read from 0x%02X:0x%02X\n",
                     data_struct.address, addr);
        STATS_INC(g_lsm6dsostats, read_errors);
        goto err;
    }

    /* Copy the I2C results into the supplied buffer */
    memcpy(buffer, payload, len);

    return 0;
err:

    return rc;
}

/**
 * Read multiple length data from LSM6DSO sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_spi_readlen(struct sensor_itf *itf, uint8_t addr,
                               uint8_t *payload, uint8_t len)
{
    int i;
    uint16_t retval;
    int rc;

    rc = 0;

    /* Insert SPI read command into register address */
    LSM6DSO_SPI_READ_CMD_BIT(addr);

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, addr);
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        LSM6DSO_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                     itf->si_num, addr);
        STATS_INC(g_lsm6dsostats, read_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0xFF);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            LSM6DSO_LOG(ERROR, "SPI_%u read failed addr:0x%02X\n",
                         itf->si_num, addr);
            STATS_INC(g_lsm6dsostats, read_errors);
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
 * Write multiple length data to LSM6DSO sensor over I2C
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_i2c_writelen(struct sensor_itf *itf, uint8_t addr,
                                uint8_t *buffer, uint8_t len)
{
    int rc;
    uint8_t payload[20] = { addr };
    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = len + 1,
        .buffer = payload
    };

    /* Max tx payload can be sizeof(payload) less register adddress */
    if (len > (sizeof(payload) - 1))
        return OS_EINVAL;

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = i2cn_master_write(itf->si_num, &data_struct,
                           MYNEWT_VAL(LSM6DSO_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(LSM6DSO_I2C_RETRIES));
    if (rc) {
        LSM6DSO_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                     data_struct.address);
        STATS_INC(g_lsm6dsostats, write_errors);
        return rc;
    }

    return 0;
}

/**
 * Write multiple length data to LSM6DSO sensor over SPI
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_spi_writelen(struct sensor_itf *itf, uint8_t addr,
                                uint8_t *payload, uint8_t len)
{
    int i;
    int rc;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, addr);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        LSM6DSO_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                     itf->si_num, addr);
        STATS_INC(g_lsm6dsostats, write_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read register data */
        rc = hal_spi_tx_val(itf->si_num, payload[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            LSM6DSO_LOG(ERROR, "SPI_%u write failed addr:0x%02X\n",
                         itf->si_num, addr);
            STATS_INC(g_lsm6dsostats, write_errors);
            goto err;
        }
    }

    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}
#endif /* BUS_DRIVER_PRESENT */

/**
 * Write multiple length data to LSM6DSO sensor over different interfaces
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_writelen(struct sensor_itf *itf, uint8_t addr,
                     uint8_t *payload, uint8_t len)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct {
        uint8_t addr;
        /*
         * XXX lsm6dso_i2c_writelen has max payload of 20 including addr, not
         * sure where it comes from
         */
        uint8_t payload[19];
    } write_data;
    struct lsm6dso *dev = (struct lsm6dso *)itf->si_dev;

    if (len > sizeof(write_data.payload)) {
        return -1;
    }

    write_data.addr = addr;
    memcpy(write_data.payload, payload, len);

    rc = bus_node_simple_write(itf->si_dev, &write_data, len + 1);
#else /* BUS_DRIVER_PRESENT */
    rc = sensor_itf_lock(itf, MYNEWT_VAL(LSM6DSO_ITF_LOCK_TMO));
    if (rc) {
        goto err;
    }

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lsm6dso_i2c_writelen(itf, addr, payload, len);
    } else {
        rc = lsm6dso_spi_writelen(itf, addr, payload, len);
    }

    sensor_itf_unlock(itf);
#endif /* BUS_DRIVER_PRESENT */

err:
    return rc;
}

/**
 * Read multiple length data from LSM6DSO sensor over different interfaces
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_readlen(struct sensor_itf *itf, uint8_t addr,
                    uint8_t *payload, uint8_t len)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct lsm6dso *dev = (struct lsm6dso *)itf->si_dev;

    if (dev->node_is_spi) {
        LSM6DSO_SPI_READ_CMD_BIT(addr);
    }

    rc = bus_node_simple_write_read_transact(itf->si_dev, &addr, 1,
                                             payload, len);
#else /* BUS_DRIVER_PRESENT */
    rc = sensor_itf_lock(itf, MYNEWT_VAL(LSM6DSO_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lsm6dso_i2c_readlen(itf, addr, payload, len);
    } else {
        rc = lsm6dso_spi_readlen(itf, addr, payload, len);
    }

    sensor_itf_unlock(itf);
#endif /* BUS_DRIVER_PRESENT */

    return rc;
}

 /**
 * Write register with mask over different interfaces
 *
 * @param sensor interface
 * @param register address
 * @param register mask
 * @param register data
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_write_data_with_mask(struct sensor_itf *itf, uint8_t addr,
                                        uint8_t mask, uint8_t data)
{
    int rc;
    uint8_t new_data, old_data;

    rc = lsm6dso_readlen(itf, addr, &old_data, 1);
    if (rc) {
        return rc;
    }

    new_data = ((old_data & (~mask)) | LSM6DSO_SHIFT_DATA_MASK(data, mask));

    /* Try to limit bus access if possible */
    if (new_data == old_data)
        return 0;

    return lsm6dso_writelen(itf, addr, &new_data, 1);
}

/**
 * Fix fot SPI restart
 *
 * @param Sensor
 * @param Interface
 * @param initialize CS pin to inactive
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_spi_fixup(struct sensor *sensor, struct sensor_itf *itf,
                             uint8_t init)
{
#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    int rc;

    if (itf->si_type == SENSOR_ITF_SPI) {
        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            return rc;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_lsm6dso_settings);
        if (rc == EINVAL) {
            /*
             * If spi is already enabled, for nrf52, it returns -1
             * We should not fail if the spi is already enabled
             */
            return rc;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            return rc;

        if (init)
            return hal_gpio_init_out(sensor->s_itf.si_cs_pin, 1);
        }
    }
#endif /* BUS_DRIVER_PRESENT */

    return SYS_EOK;
}

/**
 * Read lsm6dso gyro sensitivity
 *
 * @param Sensor Full scale
 * @param Value
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_get_gyro_sensitivity(uint8_t fs, int *val)
{
    switch(fs) {
    case LSM6DSO_GYRO_FS_250DPS_VAL:
        *val = LSM6DSO_G_SENSITIVITY_250DPS;
        break;
    case LSM6DSO_GYRO_FS_500DPS_VAL:
        *val = LSM6DSO_G_SENSITIVITY_500DPS;
        break;
    case LSM6DSO_GYRO_FS_1000DPS_VAL:
        *val = LSM6DSO_G_SENSITIVITY_1000DPS;
        break;
    case LSM6DSO_GYRO_FS_2000DPS_VAL:
        *val = LSM6DSO_G_SENSITIVITY_2000DPS;
        break;
    default:
        LSM6DSO_LOG(ERROR, "Invalid Gyro FS: %d\n", fs);

        return SYS_EINVAL;
    }

    return SYS_EOK;
}

/**
 * Read lsm6dso accelerometer sensitivity
 *
 * @param Sensor Full scale
 * @param Value
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_get_acc_sensitivity(uint8_t fs, int *val)
{
    switch(fs) {
    case LSM6DSO_ACCEL_FS_2G_VAL:
        *val = LSM6DSO_XL_SENSITIVITY_2G;
        break;
    case LSM6DSO_ACCEL_FS_4G_VAL:
        *val = LSM6DSO_XL_SENSITIVITY_4G;
        break;
    case LSM6DSO_ACCEL_FS_8G_VAL:
        *val = LSM6DSO_XL_SENSITIVITY_8G;
        break;
    case LSM6DSO_ACCEL_FS_16G_VAL:
        *val = LSM6DSO_XL_SENSITIVITY_16G;
        break;
    default:
        LSM6DSO_LOG(ERROR, "Invalid Acc FS: %d\n", fs);

        return SYS_EINVAL;
    }

    return SYS_EOK;
}

/**
 * Reset lsm6dso
 *
 * - Set both accelerometer and gyroscope in Power-Down mode
 * - Set BOOT bit of CTRL3_C register to 1
 * - Wait 10 ms
 * - Set to 1 the SW_RESET bit of CTRL3_C to 1
 * - Wait 50 μs
 *
 * @param Sensor interface
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_reset(struct sensor_itf *itf)
{
    int rc;

    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL1_XL_ADDR,
                                      LSM6DSO_ODR_XL_MASK,
                                      LSM6DSO_ACCEL_OFF_VAL);
    if (rc) {
        return rc;
    }

    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL2_G_ADDR,
                                      LSM6DSO_ODR_G_MASK,
                                      LSM6DSO_GYRO_OFF_VAL);
    if (rc) {
        return rc;
    }

    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL3_C_ADDR,
                                      LSM6DSO_BOOT_MASK,
                                      LSM6DSO_EN_BIT);
    if (rc) {
        return rc;
    }

    os_time_delay((OS_TICKS_PER_SEC * 10 / 1000) + 1);

    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL3_C_ADDR,
                                        LSM6DSO_SW_RESET_MASK,
                                        LSM6DSO_EN_BIT);
}

/**
 * Enable channels
 *
 * @param sensor interface
 * @param chan
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_chan_enable(struct sensor_itf *itf, uint8_t chan)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL9_XL_ADDR,
                                        LSM6DSO_DEN_ALL_MASK, chan);
}

/**
 * Get chip ID
 *
 * @param sensor interface
 * @param ptr to chip id to be filled up
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id)
{
    uint8_t reg;
    int rc;

    rc = lsm6dso_readlen(itf, LSM6DSO_WHO_AM_I_REG, &reg, 1);
    if (rc) {
        return rc;
    }

    *chip_id = reg;

    return 0;
}

/**
 * Sets gyro full scale selection
 *
 * @param The sensor interface
 * @param The fs setting
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_set_g_full_scale(struct sensor_itf *itf, uint8_t fs)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL2_G_ADDR,
                                        LSM6DSO_FS_G_MASK, fs);
}

/**
 * Sets accelerometer full scale selection
 *
 * @param The sensor interface
 * @param The fs setting
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_set_xl_full_scale(struct sensor_itf *itf, uint8_t fs)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL1_XL_ADDR,
                                        LSM6DSO_FS_XL_MASK, fs);
}

/**
 * Sets accelerometer rate
 *
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_set_xl_rate(struct sensor_itf *itf, uint8_t rate)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL1_XL_ADDR,
                                        LSM6DSO_ODR_XL_MASK, rate);
}

/**
 * Sets gyro rate
 *
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_set_g_rate(struct sensor_itf *itf, uint8_t rate)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL2_G_ADDR,
                                        LSM6DSO_ODR_G_MASK, rate);
}

/**
 * Set FIFO mode
 *
 * @param the sensor interface
 * @param mode
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_set_fifo_mode(struct sensor_itf *itf, uint8_t mode)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_FIFO_CTRL4_ADDR,
                                        LSM6DSO_FIFO_MODE_MASK, mode);
}

/**
 * Set FIFO watermark
 *
 * @param the sensor interface
 * @param mode
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_set_fifo_watermark(struct sensor_itf *itf, uint16_t wtm)
{
    int rc;
    uint16_t fifo_wtm;

    if (wtm > LSM6DSO_MAX_FIFO_DEPTH)
  	   return SYS_EINVAL;

    rc = lsm6dso_readlen(itf, LSM6DSO_FIFO_CTRL1_ADDR,
                         (uint8_t *)&fifo_wtm,
                         sizeof(fifo_wtm));
    if (rc) {
        return rc;
    }

    fifo_wtm &= LSM6DSO_FIFO_WTM_MASK;
    fifo_wtm |= wtm;
    rc = lsm6dso_writelen(itf, LSM6DSO_FIFO_CTRL1_ADDR,
                          (uint8_t *)&fifo_wtm, sizeof(fifo_wtm));

    return 0;
}

/**
 * Get Number of Samples in FIFO
 *
 * @param the sensor interface
 * @param Pointer to return number of samples in FIFO, 0 empty, 512 for full
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_fifo_samples(struct sensor_itf *itf, uint16_t *samples)
{
    uint16_t fifo_status;
    int rc;

    rc = lsm6dso_readlen(itf, LSM6DSO_FIFO_STS1_ADDR,
			 (uint8_t *)&fifo_status, sizeof(fifo_status));
    if (rc) {
        return rc;
    }

    fifo_status &= LSM6DSO_FIFO_DIFF_MASK;
    *samples = fifo_status;

    return 0;
}

/**
 * Set block data update
 *
 * @param the sensor interface
 * @param enable bdu
 *
 * @return 0 on success, non-zero on failure
 */
static int lsm6dso_set_bdu(struct sensor_itf *itf, bool en)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL3_C_ADDR,
                                        LSM6DSO_BDU_MASK, en);
}

/**
 * Sets accelerometer sensor user offsets
 *
 * This features is valid only for low pass accelerometer path
 * Offset weight is 2^(-10) g/LSB independently to the selected accelerometer
 * full scale
 *
 * @param The sensor interface
 * @param X offset
 * @param Y offset
 * @param Z offset
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm6dso_set_offsets(struct sensor_itf *itf, int8_t offset_x, int8_t offset_y,
                    int8_t offset_z)
{
    int8_t offset[] = { offset_x, offset_y, offset_z };

    return lsm6dso_writelen(itf, LSM6DSO_X_OFS_USR_ADDR,
                            (uint8_t *)offset, sizeof(offset));
}

/**
 * Gets accelerometer sensor user offsets
 *
 * Offset weight is 2^(-10) g/LSB independently to the selected accelerometer
 * full scale
 *
 * @param The sensor interface
 * @param Pointer to location to store X offset
 * @param Pointer to location to store Y offset
 * @param Pointer to location to store Z offset
 *
 * @return 0 on success, non-zero error on failure.
 */
int lsm6dso_get_offsets(struct sensor_itf *itf, int8_t *offset_x,
                        int8_t *offset_y, int8_t *offset_z)
{
    int8_t offset[3];
    int rc;

    rc = lsm6dso_readlen(itf, LSM6DSO_X_OFS_USR_ADDR,
                         (uint8_t *)offset, sizeof(offset));
    if (rc) {
        return rc;
    }

    *offset_x = offset[0];
    *offset_y = offset[1];
    *offset_z = offset[2];

    return 0;
}

/**
 * Sets whether user offset are enabled
 *
 * @param The sensor interface
 * @param value to set (LSM6DSO_DIS_BIT = disabled, LSM6DSO_EN_BIT = enabled)
 *
 * @return 0 on success, non-zero error on failure.
 */
int lsm6dso_set_offset_enable(struct sensor_itf *itf, bool en)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL7_G_ADDR,
                                   LSM6DSO_USR_OFF_ON_OUT_MASK, en);
}

/**
 * Sets push-pull/open-drain on INT1 and INT2 pins
 *
 * @param The sensor interface
 * @param interrupt setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_set_int_pp_od(struct sensor_itf *itf, bool mode)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL3_C_ADDR,
                                   LSM6DSO_PP_OD_MASK, mode);
}

/**
 * Gets push-pull/open-drain on INT1 and INT2 pins
 *
 * @param The sensor interface
 * @param ptr to store setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_int_pp_od(struct sensor_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t reg;

    rc = lsm6dso_readlen(itf, LSM6DSO_CTRL3_C_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    *mode = (reg & LSM6DSO_PP_OD_MASK) ? 1 : 0;

    return 0;
}

/**
 * Sets whether latched interrupts are enabled
 *
 * @param The sensor interface
 * @param value to set (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_set_latched_int(struct sensor_itf *itf, bool en)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_TAP_CFG0_ADDR,
                                   LSM6DSO_LIR_MASK, en);
}

/**
 * Gets whether latched interrupts are enabled
 *
 * @param The sensor interface
 * @param ptr to store value (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_latched_int(struct sensor_itf *itf, uint8_t *en)
{
    int rc;
    uint8_t reg;

    rc = lsm6dso_readlen(itf, LSM6DSO_TAP_CFG0_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    *en = (reg & LSM6DSO_LIR_MASK) ? 1 : 0;

    return 0;
}

/**
 * Sets whether interrupts are active high or low
 *
 * @param The sensor interface
 * @param value to set (0 = active high, 1 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_set_int_active_low(struct sensor_itf *itf, uint8_t low)
{
    return lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL3_C_ADDR,
                                   LSM6DSO_H_L_ACTIVE_MASK, low);
}

/**
 * Gets whether interrupts are active high or low
 *
 * @param The sensor interface
 * @param ptr to store value (0 = active high, 1 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_int_active_low(struct sensor_itf *itf, uint8_t *low)
{
    int rc;
    uint8_t reg;

    rc = lsm6dso_readlen(itf, LSM6DSO_CTRL3_C_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    *low = (reg & LSM6DSO_H_L_ACTIVE_MASK) ? 1 : 0;

    return 0;
}

/**
 * Clear interrupt pin configuration for interrupt
 *
 * @param the sensor interface
 * @param interrupt pin
 * @param interrupt mask
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_clear_int_pin_cfg(struct sensor_itf *itf, uint8_t int_pin,
                              uint8_t int_mask)
{
    uint8_t reg;

    switch(int_pin) {
    case 0:
        reg = LSM6DSO_MD1_CFG_ADDR;
        break;
    case 1:
        reg = LSM6DSO_MD2_CFG_ADDR;
        break;
    default:
        LSM6DSO_LOG(ERROR, "Invalid int pin %d\n", int_pin);

        return SYS_EINVAL;
    }

    return lsm6dso_write_data_with_mask(itf, reg, int_mask, LSM6DSO_DIS_BIT);
}

/**
 * Clear all interrupts by reading all four interrupt registers status
 *
 * @param itf The sensor interface
 * @param src Ptr to return 4 interrupt sources in
 *
 * @return 0 on success, non-zero on failure
 */
int
lsm6dso_clear_int(struct sensor_itf *itf, uint8_t *int_src)
{
   return lsm6dso_readlen(itf, LSM6DSO_ALL_INT_SRC_ADDR, int_src, 4);
}

/**
 * Set interrupt pin configuration for interrupt
 *
 * @param the sensor interface
 * @param interrupt pin
 * @param interrupt mask
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_set_int_pin_cfg(struct sensor_itf *itf, uint8_t int_pin,
                            uint8_t int_mask)
{
    uint8_t reg;

    switch(int_pin) {
    case 0:
        reg = LSM6DSO_MD1_CFG_ADDR;
        break;
    case 1:
        reg = LSM6DSO_MD2_CFG_ADDR;
        break;
    default:
        LSM6DSO_LOG(ERROR, "Invalid int pin %d\n", int_pin);

        return SYS_EINVAL;
    }

    return lsm6dso_write_data_with_mask(itf, reg, int_mask, LSM6DSO_EN_BIT);
}

/**
 * Set orientation configuration
 *
 * @param the sensor interface
 * @param the orientation settings
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_set_orientation(struct sensor_itf *itf,
                            struct lsm6dso_orientation_settings *cfg)
{
    int rc;
    uint8_t reg;

    rc = lsm6dso_readlen(itf, LSM6DSO_TAP_THS_6D_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    reg &= ~(LSM6DSO_D4D_EN_MASK | LSM6DSO_SIXD_THS_MASK);
    reg |= LSM6DSO_SHIFT_DATA_MASK(cfg->en_4d, LSM6DSO_D4D_EN_MASK);
    reg |= LSM6DSO_SHIFT_DATA_MASK(cfg->ths_6d, LSM6DSO_SIXD_THS_MASK);

    rc = lsm6dso_writelen(itf, LSM6DSO_TAP_THS_6D_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    return 0;
}

/**
 * Get orientation configuration
 *
 * @param the sensor interface
 * @param the orientation settings
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_orientation_cfg(struct sensor_itf *itf,
                                struct lsm6dso_orientation_settings *cfg)
{
    int rc;
    uint8_t reg;

    rc = lsm6dso_readlen(itf, LSM6DSO_TAP_THS_6D_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    cfg->en_4d = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_D4D_EN_MASK);
    cfg->ths_6d = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_SIXD_THS_MASK);

    return 0;
}

/**
 * Set tap detection configuration
 *
 * @param the sensor interface
 * @param the tap settings
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_set_tap_cfg(struct sensor_itf *itf,
                        struct lsm6dso_tap_settings *cfg)
{
    int rc;
    uint8_t reg;

    rc = lsm6dso_readlen(itf, LSM6DSO_TAP_CFG2_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    reg &= ~LSM6DSO_TAP_THS_Y_MASK;
    reg |= cfg->tap_ths & LSM6DSO_TAP_THS_Y_MASK;

    rc = lsm6dso_writelen(itf, LSM6DSO_TAP_CFG2_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    reg = cfg->tap_ths & LSM6DSO_TAP_THS_X_MASK;
    reg |= cfg->tap_prio & LSM6DSO_TAP_PRIORITY_MASK;

    rc = lsm6dso_writelen(itf, LSM6DSO_TAP_CFG1_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    reg = LSM6DSO_SHIFT_DATA_MASK(cfg->dur, LSM6DSO_DUR_MASK);
    reg |= LSM6DSO_SHIFT_DATA_MASK(cfg->quiet, LSM6DSO_QUIET_MASK);
    reg |= LSM6DSO_SHIFT_DATA_MASK(cfg->shock, LSM6DSO_SHOCK_MASK);

    rc = lsm6dso_writelen(itf, LSM6DSO_INT_DUR2_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_WAKE_UP_THS_ADDR,
                                   LSM6DSO_SINGLE_DOUBLE_TAP_MASK,
                                   cfg->en_dtap);
    if (rc) {
        return rc;
    }

    rc = lsm6dso_readlen(itf, LSM6DSO_TAP_CFG0_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_TAP_THS_6D_ADDR,
                                   LSM6DSO_TAP_THS_Z_MASK,
                                   cfg->tap_ths);
    if (rc) {
        return rc;
    }

    reg &= ~(LSM6DSO_TAP_X_EN_MASK | LSM6DSO_TAP_Y_EN_MASK | LSM6DSO_TAP_Z_EN_MASK);
    reg |= cfg->en_x ? LSM6DSO_TAP_X_EN_MASK : 0;
    reg |= cfg->en_y ? LSM6DSO_TAP_Y_EN_MASK : 0;
    reg |= cfg->en_z ? LSM6DSO_TAP_Z_EN_MASK : 0;
    reg |= LSM6DSO_LIR_MASK;

    rc = lsm6dso_writelen(itf, LSM6DSO_TAP_CFG0_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    return 0;
}

/**
 * Get tap detection config
 *
 * @param the sensor interface
 * @param ptr to the tap settings
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_tap_cfg(struct sensor_itf *itf,
                        struct lsm6dso_tap_settings *cfg)
{
    int rc;
    uint8_t reg;

    rc = lsm6dso_readlen(itf, LSM6DSO_TAP_CFG0_ADDR, &reg, 1);
    if (rc) {
	return rc;
    }

    cfg->en_x = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_TAP_X_EN_MASK);
    cfg->en_y = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_TAP_Y_EN_MASK);
    cfg->en_z = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_TAP_Z_EN_MASK);

    rc = lsm6dso_readlen(itf, LSM6DSO_TAP_CFG1_ADDR, &reg, 1);
    if (rc) {
	return rc;
    }

    cfg->tap_ths = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_TAP_THS_X_MASK);
    cfg->tap_prio = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_TAP_PRIORITY_MASK);

    rc = lsm6dso_readlen(itf, LSM6DSO_INT_DUR2_ADDR, &reg, 1);
    if (rc) {
	return rc;
    }

    cfg->dur = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_DUR_MASK);
    cfg->quiet = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_QUIET_MASK);
    cfg->shock = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_SHOCK_MASK);

    rc = lsm6dso_readlen(itf, LSM6DSO_WAKE_UP_THS_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    cfg->en_dtap = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_SINGLE_DOUBLE_TAP_MASK);

    return 0;
}

/**
 * Set freefall detection configuration
 *
 * @param the sensor interface
 * @param freefall duration (6 bits LSB = 1/ODR)
 * @param freefall threshold (3 bits)
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_set_freefall(struct sensor_itf *itf, struct lsm6dso_ff_settings *ff)
{
    int rc;
    uint8_t reg;

    reg = LSM6DSO_SHIFT_DATA_MASK(ff->freefall_dur, LSM6DSO_FF_DUR_MASK);
    reg |= LSM6DSO_SHIFT_DATA_MASK(ff->freefall_ths, LSM6DSO_FF_THS_MASK);

    rc = lsm6dso_writelen(itf, LSM6DSO_FREE_FALL_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    return lsm6dso_write_data_with_mask(itf, LSM6DSO_WAKE_UP_DUR_ADDR,
                LSM6DSO_FF_DUR5_MASK,
                (ff->freefall_dur & LSM6DSO_FF_DUR5_MASK) ? 1 : 0);
}

/**
 * Get freefall detection config
 *
 * @param the sensor interface
 * @param ptr to freefall duration
 * @param ptr to freefall threshold
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_freefall(struct sensor_itf *itf, struct lsm6dso_ff_settings *ff)
{
    int rc;
    uint8_t regs[2];

    rc = lsm6dso_readlen(itf, LSM6DSO_WAKE_UP_DUR_ADDR, regs, sizeof(regs));
    if (rc) {
        return rc;
    }

    ff->freefall_dur = LSM6DSO_DESHIFT_DATA_MASK(regs[1], LSM6DSO_FF_DUR_MASK) |
	   LSM6DSO_DESHIFT_DATA_MASK(regs[0], LSM6DSO_FF_DUR5_MASK);
    ff->freefall_ths = LSM6DSO_DESHIFT_DATA_MASK(regs[1], LSM6DSO_FF_THS_MASK);

    return 0;
}

/**
 * Set Wake Up Duration
 *
 * @param the sensor interface
 * @param wk configuration to set
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_set_wake_up(struct sensor_itf *itf, struct lsm6dso_wk_settings *wk)
{
    int rc;

    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_WAKE_UP_THS_ADDR,
                                   LSM6DSO_WK_THS_MASK, wk->wake_up_ths);
    if (rc) {
        return rc;
    }

    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_WAKE_UP_DUR_ADDR,
                                   LSM6DSO_WAKE_DUR_MASK, wk->wake_up_dur);
    if (rc) {
        return rc;
    }

    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_WAKE_UP_DUR_ADDR,
                                   LSM6DSO_SLEEP_DUR_MASK, wk->sleep_duration);
    if (rc) {
        return rc;
    }

    return lsm6dso_write_data_with_mask(itf, LSM6DSO_TAP_CFG0_ADDR,
                                   LSM6DSO_SLOPE_FDS_MASK, wk->hpf_slope);
}

/**
 * Get Wake Up Duration
 *
 * @param the sensor interface
 * @param ptr to wake up configuration
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_wake_up(struct sensor_itf *itf, struct lsm6dso_wk_settings *wk)
{
    int rc;
    uint8_t reg;

    rc = lsm6dso_readlen(itf, LSM6DSO_WAKE_UP_THS_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    wk->wake_up_ths = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_WAKE_DUR_MASK);

    rc = lsm6dso_readlen(itf, LSM6DSO_WAKE_UP_DUR_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    wk->wake_up_dur = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_WAKE_DUR_MASK);
    wk->sleep_duration = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_SLEEP_DUR_MASK);

    rc = lsm6dso_readlen(itf, LSM6DSO_TAP_CFG0_ADDR, &reg, 1);
    if (rc) {
        return rc;
    }

    wk->hpf_slope = LSM6DSO_DESHIFT_DATA_MASK(reg, LSM6DSO_SLOPE_FDS_MASK);

    return 0;
}

static void init_interrupt(struct lsm6dso_int *interrupt, struct sensor_int *ints)
{
    os_error_t error;

    /* Init semaphore for task to wait on when irq asleep */
    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints = ints;
}

void undo_interrupt(struct lsm6dso_int * interrupt)
{
    OS_ENTER_CRITICAL(interrupt->lock);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(interrupt->lock);
}

/**
 * Wait on interrupt->wait lock
 *
 * This call suspend task until wake_interrupt is called
 *
 * @param the interrupt structure
 * @param int_num registered
 *
 * @return 0 on success, non-zero on failure
 */
int wait_interrupt(struct lsm6dso_int *interrupt, uint8_t int_num)
{
    bool wait;
    os_error_t error;

    OS_ENTER_CRITICAL(interrupt->lock);

    /* Check if we did not missed interrupt */
    if (hal_gpio_read(interrupt->ints[int_num].host_pin) ==
                      interrupt->ints[int_num].active) {
        OS_EXIT_CRITICAL(interrupt->lock);
        return OS_OK;
    }

    if (interrupt->active) {
        interrupt->active = false;
        wait = false;
    } else {
        interrupt->asleep = true;
        wait = true;
    }
    OS_EXIT_CRITICAL(interrupt->lock);

    if (wait) {
        error = os_sem_pend(&interrupt->wait, LSM6DSO_MAX_INT_WAIT);
        if (error == OS_TIMEOUT) {
            return error;
        }
        assert(error == OS_OK);
    }

    return OS_OK;
}

/**
 * Wake tasks waiting on interrupt->wait lock
 *
 * This call resume task waiting on wake_interrupt lock
 *
 * @param the interrupt structure
 *
 * @return 0 on success, non-zero on failure
 */
static void wake_interrupt(struct lsm6dso_int *interrupt)
{
    bool wake;

    OS_ENTER_CRITICAL(interrupt->lock);
    if (interrupt->asleep) {
        interrupt->asleep = false;
        wake = true;
    } else {
        interrupt->active = true;
        wake = false;
    }
    OS_EXIT_CRITICAL(interrupt->lock);

    if (wake) {
        os_error_t error;

        /* Release semaphore to wait_interrupt routine */
        error = os_sem_release(&interrupt->wait);
        assert(error == OS_OK);
    }
}

/**
 * Wake tasks waiting on interrupt->wait lock
 *
 * This call resume task waiting on wake_interrupt lock
 *
 * @param the interrupt structure
 *
 * @return 0 on success, non-zero on failure
 */
static void lsm6dso_int_irq_handler(void *arg)
{
    struct sensor *sensor = arg;
    struct lsm6dso *lsm6dso;

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);

    if(lsm6dso->pdd.interrupt) {
        wake_interrupt(lsm6dso->pdd.interrupt);
    }

    sensor_mgr_put_interrupt_evt(sensor);
}

/**
 * Register irq pin and handler
 *
 * @param the lsm6dso device
 * @param handler to register to gpio
 * @param arg for handler
 *
 * @return 0 on success, non-zero on failure
 */
static int init_intpin(struct lsm6dso *lsm6dso, hal_gpio_irq_handler_t handler,
                       void *arg)
{
    hal_gpio_irq_trig_t trig;
    int pin = -1;
    int rc;
    int i;

    /* Select interrupt pin configured */
    for (i = 0; i < MYNEWT_VAL(SENSOR_MAX_INTERRUPTS_PINS); i++){
        pin = lsm6dso->sensor.s_itf.si_ints[i].host_pin;
        if (pin >= 0) {
            break;
        }
    }

    if (pin < 0) {
        LSM6DSO_LOG(ERROR, "Interrupt pin not configured\n");

        return SYS_EINVAL;
    }

    if (lsm6dso->sensor.s_itf.si_ints[i].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }

    rc = hal_gpio_irq_init(pin, handler, arg,
                           trig, HAL_GPIO_PULL_NONE);
    if (rc) {
        LSM6DSO_LOG(ERROR, "Failed to initialise interrupt pin %d\n", pin);

        return rc;
    }

    return 0;
}

/**
 * Disable sensor interrupt
 *
 * @param sensor device
 * @param int_mask interrupt bit
 * @param int_num interrupt pin
 *
 * @return 0 on success, non-zero on failure
 */
static int disable_interrupt(struct sensor *sensor, uint8_t int_mask,
                             uint8_t int_num)
{
    struct lsm6dso *lsm6dso;
    struct lsm6dso_pdd *pdd;
    struct sensor_itf *itf;

    if (int_mask == 0) {
        return SYS_EINVAL;
    }

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lsm6dso->pdd;

    pdd->int_enable &= ~(int_mask << (int_num * 8));

    /* disable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_disable(itf->si_ints[int_num].host_pin);
    }

    /* update interrupt setup in device */
    return lsm6dso_clear_int_pin_cfg(itf, int_num, int_mask);
}

/**
 * Enable sensor interrupt
 *
 * @param sensor device
 * @param int_to_enable interrupt bit
 * @param int_num interrupt pin
 *
 * @return 0 on success, non-zero on failure
 */
int enable_interrupt(struct sensor *sensor, uint8_t int_mask, uint8_t int_num)
{
    struct lsm6dso *lsm6dso;
    struct lsm6dso_pdd *pdd;
    struct sensor_itf *itf;
    uint8_t int_src[4];
    int rc;

    if (!int_mask) {
        return SYS_EINVAL;
    }

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lsm6dso->pdd;

    rc = lsm6dso_clear_int(itf, int_src);
    if (rc) {
        return rc;
    }

    /* if no interrupts are currently in use disable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_enable(itf->si_ints[int_num].host_pin);
    }

    /* Save bitmask of enabled events interrupt */
    pdd->int_enable |= (int_mask << (int_num * 8));

    /* enable interrupt in device */
    rc = lsm6dso_set_int_pin_cfg(itf, int_num, int_mask);
    if (rc) {
        disable_interrupt(sensor, int_mask, int_num);
    }

    return rc;
}

/**
 * Disable sensor fifo interrupt
 *
 * @param sensor device
 * @param lsm6dso_cfg configuration
 *
 * @return 0 on success, non-zero on failure
 */
int disable_fifo_interrupt(struct sensor *sensor, sensor_type_t type,
                           struct lsm6dso_cfg *cfg)
{
    struct lsm6dso *lsm6dso;
    struct lsm6dso_pdd *pdd;
    struct sensor_itf *itf;
    uint8_t reg;
    uint8_t int_pin;
    int rc;

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lsm6dso->pdd;
    int_pin = cfg->read.int_num;

    /* Clear it in interrupt bitmask */
    pdd->int_enable &= ~(LSM6DSO_INT_FIFO_TH_MASK << (int_pin * 8));

    /* The last one closes the door */
    if (!pdd->int_enable) {
        hal_gpio_irq_disable(itf->si_ints[int_pin].host_pin);
    }

    switch(int_pin) {
    case 0:
        reg = LSM6DSO_INT1_CTRL;
        break;
    case 1:
        reg = LSM6DSO_INT2_CTRL;
        break;
    default:
        LSM6DSO_LOG(ERROR, "Invalid int pin %d\n", int_pin);
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lsm6dso_write_data_with_mask(itf, reg, LSM6DSO_INT_FIFO_TH_MASK,
                                        LSM6DSO_DIS_BIT);
    if (rc) {
        goto err;
    }

    if (type & SENSOR_TYPE_GYROSCOPE) {
        rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_FIFO_CTRL3_ADDR,
                                          LSM6DSO_FIFO_BDR_GY_MASK, 0);
        if (rc) {
            goto err;
        }
    }

    if (type & SENSOR_TYPE_ACCELEROMETER) {
        rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_FIFO_CTRL3_ADDR,
                                          LSM6DSO_FIFO_BDR_XL_MASK, 0);
        if (rc) {
            goto err;
        }
    }

err:
    return rc;
}

/**
 * Enable sensor fifo interrupt
 *
 * @param sensor device
 * @param lsm6dso_cfg configuration
 *
 * @return 0 on success, non-zero on failure
 */
int enable_fifo_interrupt(struct sensor *sensor, sensor_type_t type,
                          struct lsm6dso_cfg *cfg)
{
    struct lsm6dso *lsm6dso;
    struct lsm6dso_pdd *pdd;
    struct sensor_itf *itf;
    uint8_t reg;
    uint8_t int_pin;
    int rc;

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lsm6dso->pdd;
    int_pin = cfg->read.int_num;

    /* if no interrupts are currently in use enable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_enable(itf->si_ints[int_pin].host_pin);
    }

    switch(int_pin) {
    case 0:
        reg = LSM6DSO_INT1_CTRL;
        break;
    case 1:
        reg = LSM6DSO_INT2_CTRL;
        break;
    default:
        rc = SYS_EINVAL;
        LSM6DSO_LOG(ERROR, "Invalid int pin %d\n", int_pin);
        goto err;
    }

    rc = lsm6dso_write_data_with_mask(itf, reg, LSM6DSO_INT_FIFO_TH_MASK,
                                        LSM6DSO_EN_BIT);
    if (rc) {
        goto err;
    }

    /* Update enabled interrupt bitmask */
    pdd->int_enable |= (LSM6DSO_INT_FIFO_TH_MASK << (int_pin * 8));

    if (type & SENSOR_TYPE_GYROSCOPE) {
        rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_FIFO_CTRL3_ADDR,
                                     LSM6DSO_FIFO_BDR_GY_MASK, cfg->gyro_rate);
        if (rc) {
            goto err;
        }
    }

    if (type & SENSOR_TYPE_ACCELEROMETER) {
        rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_FIFO_CTRL3_ADDR,
                                     LSM6DSO_FIFO_BDR_XL_MASK, cfg->acc_rate);
        if (rc) {
            goto err;
        }
    }

    return rc;

err:
    disable_fifo_interrupt(sensor, type, cfg);

    return rc;
}

/**
 * Enable embedded function interrupt
 *
 * @param sensor device
 * @param enable/disable embedded function interrupt
 *
 * @return 0 on success, non-zero on error
 */
static int enable_embedded_interrupt(struct sensor *sensor, bool en)
{
  struct sensor_itf *itf;

  itf = SENSOR_GET_ITF(sensor);

  return lsm6dso_write_data_with_mask(itf, LSM6DSO_TAP_CFG2_ADDR,
                                 LSM6DSO_INTERRUPTS_ENABLE_MASK, en);
}

/**
 * Get temperature data
 *
 * If both the accelerometer and the gyroscope sensors are in Power-Down mode,
 * the temperature sensor is off.
 * The maximum output data rate of the temperature sensor is 52 Hz and its
 * value depends on how the accelerometer and gyroscope sensors are configured:
 *
 *  - If the gyroscope is in Power-Down mode:
 *     - If the accelerometer is configured in Ultra-Low-Power or Low-Power mode
 *       and its ODR is lower than 52 Hz, the temperature data rate is equal to
 *       the configured accelerometer ODR;
 *     - The temperature data rate is equal to 52 Hz for all other accelerometer
 *       configurations.
 *  - If the gyroscope is not in Power-Down mode, the temperature data rate is
 *    equal to 52 Hz, regardless of the accelerometer and gyroscope
 *    configuration.
 *
 * @param The sensor interface
 * @param pointer to the temperature data structure
 *
 * @return 0 on success, non-zero on error
 */
static int lsm6dso_get_temp_data(struct sensor_itf *itf,
                                 struct sensor_temp_data *std)
{
    int rc;
    int16_t temp;

    rc = lsm6dso_readlen(itf, LSM6DSO_OUT_TEMP_L_ADDR,
                         (uint8_t *)&temp, sizeof(temp));
    if (rc) {
        return rc;
    }

    std->std_temp = ((float)temp / 100) + 25.0f;
    std->std_temp_is_valid = 1;

    return 0;
}

/**
 * Gets a raw sensor data from the acc/gyro/temp sensor.
 *
 * @param The sensor interface
 * @param Sensor type
 * @param axis data
 *
 * @return 0 on success, non-zero on failure
 */
static inline int lsm6dso_get_ag_raw_data(struct sensor_itf *itf,
                                          sensor_type_t type, int16_t *data)
{
    int rc;
    uint8_t reg;
    uint8_t payload[6] = { 0 };

    /* Set data out base register for sensor */
    reg = LSM6DSO_GET_OUT_REG(type);
    rc = lsm6dso_readlen(itf, reg, &payload[0], 6);
    if (rc) {
        return rc;
    }

    /*
     * Both acc and gyro data are represented as 16-bit word in
     * two’s complement
     */
    data[0] = (int16_t)(payload[0] | (payload[1] << 8));
    data[1] = (int16_t)(payload[2] | (payload[3] << 8));
    data[2] = (int16_t)(payload[4] | (payload[5] << 8));

    return 0;
}

/**
 * Run Self test on sensor
 *
 * @param the sensor interface
 * @param pointer to return test result in
 *        0 on pass
 *        1 on XL failure
 *        2 on Gyro failure
 *
 * @return 0 on sucess, non-zero on failure
 */
int lsm6dso_run_self_test(struct sensor_itf *itf, int *result)
{
    int rc;
    int min, max;
    int16_t data[3];
    int diff[3] = { 0 };
    uint8_t i;
    uint8_t prev_config[10];
    uint8_t st_xl_config[] = {
        0x38, 0x00, 0x44, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t st_g_config[] = {
        0x00, 0x5c, 0x44, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00
    };

    *result = 0;

    /* Save accelerometer configuration */
    rc = lsm6dso_readlen(itf, LSM6DSO_CTRL1_XL_ADDR,
                         prev_config, sizeof(prev_config));
    if (rc) {
        return rc;
    }

    /* Configure xl as for AN5192 */
    rc = lsm6dso_writelen(itf, LSM6DSO_CTRL1_XL_ADDR,
                          st_xl_config, sizeof(st_xl_config));
    if (rc) {
        return rc;
    }

    /* Wait 100ms for stable output data */
    os_time_delay(OS_TICKS_PER_SEC / 10);

    /* Read and discard first data sample */
    rc = lsm6dso_get_ag_raw_data(itf, SENSOR_TYPE_ACCELEROMETER, data);
    if (rc) {
        return rc;
    }

    /* Take 5 samples */
    for(i = 0; i < 5; i++) {
        rc = lsm6dso_get_ag_raw_data(itf, SENSOR_TYPE_ACCELEROMETER, data);
        if (rc) {
            return rc;
        }
        diff[0] += data[0];
        diff[1] += data[1];
        diff[2] += data[2];

        /* Wait at least 1/52 s ~ 20 ms */
        os_time_delay(OS_TICKS_PER_SEC / 52);
    }

    /* Enable positive sign self-test mode */
    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL5_C_ADDR,
                                      LSM6DSO_ST_XL_MASK,
                                      LSM6DSO_XL_SELF_TEST_POS_SIGN);
    if (rc) {
        return rc;
    }

    /* Wait 100ms for stable output data */
    os_time_delay(OS_TICKS_PER_SEC / 10);

    /* Read and discard first data sample */
    rc = lsm6dso_get_ag_raw_data(itf, SENSOR_TYPE_ACCELEROMETER, data);
    if (rc) {
        return rc;
    }

    /* Take 5 samples */
    for(i = 0; i < 5; i++) {
        rc = lsm6dso_get_ag_raw_data(itf, SENSOR_TYPE_ACCELEROMETER, data);
        if (rc) {
            return rc;
        }

        diff[0] -= data[0];
        diff[1] -= data[1];
        diff[2] -= data[2];

        /* Wait at least 1/52 s ~ 20 ms */
        os_time_delay(OS_TICKS_PER_SEC / 52);
    }

    /* Restore register configuration */
    rc = lsm6dso_writelen(itf, LSM6DSO_CTRL1_XL_ADDR,
                          prev_config, sizeof(prev_config));
        if (rc) {
            return rc;
        }

    /* Compare values to thresholds */
    min = LSM6DSO_XL_ST_MIN * 5 * 2;
    max = LSM6DSO_XL_ST_MAX * 5 * 2;
    for (i = 0; i < 3; i++) {
        /* ABS */
        if (diff[i] < 0)
            diff[i] = -diff[i];

        if ((diff[i] < min) || (diff[i] > max)) {
            *result |= 1;
        }
    }

    /* Configure gyro as for AN5192 */
    rc = lsm6dso_writelen(itf, LSM6DSO_CTRL1_XL_ADDR,
                          st_g_config, sizeof(st_g_config));
    if (rc) {
        return rc;
    }

    /* Wait 100ms for stable output data */
    os_time_delay(OS_TICKS_PER_SEC / 10);

    /* Read and discard first gyro data sample */
    rc = lsm6dso_get_ag_raw_data(itf, SENSOR_TYPE_GYROSCOPE, data);
    if (rc) {
        return rc;
    }

    memset(diff, 0, sizeof(diff));

    /* Take 5 samples */
    for(i = 0; i < 5; i++) {
        rc = lsm6dso_get_ag_raw_data(itf, SENSOR_TYPE_GYROSCOPE, data);
        if (rc) {
            return rc;
        }

        diff[0] += data[0];
        diff[1] += data[1];
        diff[2] += data[2];

        /* Wait at least 1/208 s ~ 5 ms */
        os_time_delay(OS_TICKS_PER_SEC / 208);
    }

    /* Enable positive sign self-test mode */
    rc = lsm6dso_write_data_with_mask(itf, LSM6DSO_CTRL5_C_ADDR,
                                      LSM6DSO_ST_G_MASK,
                                      LSM6DSO_G_SELF_TEST_POS_SIGN);
    if (rc) {
        return rc;
    }

    /* Wait 100ms for stable output data */
    os_time_delay(OS_TICKS_PER_SEC / 10);

    /* Read and discard first data sample */
    rc = lsm6dso_get_ag_raw_data(itf, SENSOR_TYPE_GYROSCOPE, data);
    if (rc) {
        return rc;
    }

    /* Take 5 samples */
    for(i = 0; i < 5; i++) {
        rc = lsm6dso_get_ag_raw_data(itf, SENSOR_TYPE_GYROSCOPE, data);
        if (rc) {
            return rc;
        }
        diff[0] -= data[0];
        diff[1] -= data[1];
        diff[2] -= data[2];

        /* Wait at least 1/208 s ~ 20 ms */
        os_time_delay(OS_TICKS_PER_SEC / 208);
    }

    /* Restore register configuration */
    rc = lsm6dso_writelen(itf, LSM6DSO_CTRL1_XL_ADDR,
                          prev_config, sizeof(prev_config));
        if (rc) {
            return rc;
        }

    /* Compare values to thresholds */
    min = LSM6DSO_G_ST_MIN * 5 * 2;
    max = LSM6DSO_G_ST_MAX * 5 * 2;
    for (i = 0; i < 3; i++) {
        /* ABS */
        if (diff[i] < 0)
            diff[i] = -diff[i];
        if ((diff[i] < min) || (diff[i] > max)) {
            *result |= 2;
        }
    }

    return 0;
}

/**
 * Gets a new data sample from the acc/gyro/temp sensor.
 *
 * @param The sensor interface
 * @param Sensor type
 * @param axis data
 * @param lsm6dso_cfg data structure
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_get_ag_data(struct sensor_itf *itf, sensor_type_t type, void *data,
                        struct lsm6dso_cfg *cfg)
{
    int rc;
    int16_t x_y_z[3];
    struct sensor_accel_data *sad = data;
    int sensitivity;

    rc = lsm6dso_get_ag_raw_data(itf, type, (void *)x_y_z);
    if (rc) {
        return rc;
    }

    switch(type) {
        case SENSOR_TYPE_GYROSCOPE:
            sensitivity = cfg->gyro_sensitivity;
            break;
        case SENSOR_TYPE_ACCELEROMETER:
            sensitivity = cfg->acc_sensitivity;
           break;
        default:
            LSM6DSO_LOG(ERROR, "Invalid sensor type: %d\n", type);
            return SYS_EINVAL;
    }

    sad->sad_x = (((float)x_y_z[0]) * sensitivity) / 1000;
    sad->sad_y = (((float)x_y_z[1]) * sensitivity) / 1000;
    sad->sad_z = (((float)x_y_z[2]) * sensitivity) / 1000;

    sad->sad_x_is_valid = 1;
    sad->sad_y_is_valid = 1;
    sad->sad_z_is_valid = 1;

    return 0;
}

/**
 * Gets a data sample of acc/gyro/temp sensor from FIFO.
 *
 * @param The sensor interface
 * @param axis data
 * @param lsm6dso_cfg data structure
 *
 * @return 0 on success, non-zero on failure
 */
int lsm6dso_read_fifo(struct sensor_itf *itf,
                      void *data, sensor_type_t *type,
                      struct lsm6dso_cfg *cfg)
{
    int rc;
    uint8_t payload[LSM6DSO_FIFO_SAMPLE_SIZE];
    int16_t x, y, z;
    struct sensor_accel_data *sad = data;
    int sensitivity;

    rc = lsm6dso_readlen(itf, LSM6DSO_FIFO_DATA_ADDR_TAG,
                         &payload[0], LSM6DSO_FIFO_SAMPLE_SIZE);
    if (rc) {
        return rc;
    }

    /*
     * Both acc and gyro data are represented as 16-bit word in
     * two’s complement
     */
    x = (int16_t)(payload[1] | (payload[2] << 8));
    y = (int16_t)(payload[3] | (payload[4] << 8));
    z = (int16_t)(payload[5] | (payload[6] << 8));

    switch(LSM6DSO_DESHIFT_DATA_MASK(payload[0], LSM6DSO_FIFO_TAG_MASK)) {
    case LSM6DSO_GYRO_TAG:
        *type = SENSOR_TYPE_GYROSCOPE;
        sensitivity = cfg->gyro_sensitivity;
        break;
    case LSM6DSO_ACC_TAG:
        *type = SENSOR_TYPE_ACCELEROMETER;
        sensitivity = cfg->acc_sensitivity;
        break;
    default:
         LSM6DSO_LOG(ERROR, "Invalid sensor tag: %d\n", payload[0]);
         return SYS_ENODEV;
    }

    sad->sad_x = (((float)x) * sensitivity) / 1000;
    sad->sad_y = (((float)y) * sensitivity) / 1000;
    sad->sad_z = (((float)z) * sensitivity) / 1000;

    sad->sad_x_is_valid = 1;
    sad->sad_y_is_valid = 1;
    sad->sad_z_is_valid = 1;

    return 0;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this sensor
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int lsm6dso_init(struct os_dev *dev, void *arg)
{
    struct lsm6dso *lsm6dso;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    lsm6dso = (struct lsm6dso *)dev;
    lsm6dso->cfg.lc_s_mask = SENSOR_TYPE_ALL;
    sensor = &lsm6dso->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lsm6dsostats),
        STATS_SIZE_INIT_PARMS(g_lsm6dsostats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lsm6dso_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lsm6dsostats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc) {
        goto err;
    }

    /* Add the IMU driver plus temperature sensor */
    rc = sensor_set_driver(sensor,
                           SENSOR_TYPE_ACCELEROMETER |
                           SENSOR_TYPE_GYROSCOPE |
                           SENSOR_TYPE_TEMPERATURE,
                           (struct sensor_driver *)&g_lsm6dso_sensor_driver);
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

    rc = lsm6dso_spi_fixup(sensor, &sensor->s_itf, 1);
    if (rc) {
        goto err;
    }

    init_interrupt(&lsm6dso->intr, lsm6dso->sensor.s_itf.si_ints);

    lsm6dso->pdd.notify_ctx.snec_sensor = sensor;
    lsm6dso->pdd.interrupt = NULL;

    rc = init_intpin(lsm6dso, lsm6dso_int_irq_handler, sensor);

err:
    return rc;
}

/**
 * Read data samples from FIFO.
 *
 * The sensor_type can be a bitmask like this
 * (SENSOR_TYPE_ACCELEROMETER | SENSOR_TYPE_GYROSCOPE)
 *
 * @param sensor The sensor object
 * @param type sensor type bitmask
 * @param data_func callback register data function
 * @param data_arg function arguments
 * @param timeout
 *
 * @return 0 on success, non-zero error on failure.
 */
int lsm6dso_stream_read(struct sensor *sensor,
                        sensor_type_t sensor_type,
                        sensor_data_func_t read_func,
                        void *data_arg,
                        uint32_t time_ms)
{
    struct lsm6dso_pdd *pdd;
    struct lsm6dso *lsm6dso;
    struct sensor_itf *itf;
    struct lsm6dso_cfg *cfg;
    os_time_t time_ticks;
    os_time_t stop_ticks = 0;
    uint16_t fifo_samples;
    int rc;
    struct sensor_accel_data sad;
    sensor_type_t r_type;

    /* Temperature reading not supported in FIFO */
    if (!(sensor_type & SENSOR_TYPE_ACCELEROMETER) &&
        !(sensor_type & SENSOR_TYPE_GYROSCOPE)) {
        return SYS_EINVAL;
    }

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lsm6dso->pdd;
    cfg = &lsm6dso->cfg;

    if (cfg->read.mode != LSM6DSO_READ_STREAM) {
        return SYS_EINVAL;
    }

    undo_interrupt(&lsm6dso->intr);

   if (pdd->interrupt) {
        return SYS_EBUSY;
    }

    /* Enable interrupt */
    pdd->interrupt = &lsm6dso->intr;

    rc = enable_fifo_interrupt(sensor, sensor_type, cfg);
    if (rc) {
        return rc;
    }

    /* Set FIFO to configuration value */
    rc = lsm6dso_set_fifo_mode(itf, cfg->fifo.mode);
    if (rc) {
        goto err;
    }

    if (time_ms > 0) {
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc) {
            goto err;
        }
        stop_ticks = os_time_get() + time_ticks;
    }

    for (;;) {
        /* Force at least one read for cases when fifo is disabled */
        rc = wait_interrupt(&lsm6dso->intr, cfg->read.int_num);
        if (rc) {
            goto err;
        }

        rc = lsm6dso_get_fifo_samples(itf, &fifo_samples);
            if (rc) {
                goto err;
            }

        do {
            /* Read all fifo samples */
            rc = lsm6dso_read_fifo(itf, &sad, &r_type, cfg);
            if (rc) {
                goto err;
            }

            if ((sensor_type & r_type) == r_type) {
                rc = read_func(sensor, data_arg, &sad, r_type);
                if (rc) {
                    goto err;
                }

            }
            fifo_samples--;
        } while (fifo_samples > 0);

        if (time_ms > 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
           break;
        }
    }

err:
    /* Disable FIFO */
    rc |= lsm6dso_set_fifo_mode(itf, LSM6DSO_FIFO_MODE_BYPASS_VAL);

    /* Disable interrupt */
    pdd->interrupt = NULL;

    rc |= disable_fifo_interrupt(sensor, sensor_type, cfg);

    return rc;
}

/**
 * Single sensor read
 *
 * @param sensor The sensor object
 * @param type sensor type (acc, gyro etc)
 * @param data_func callback register data function
 * @param data_arg function arguments
 * @param timeout
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lsm6dso_poll_read(struct sensor *sensor, sensor_type_t type,
                             sensor_data_func_t data_func, void *data_arg,
                             uint32_t timeout)
{
    int rc;
    struct sensor_itf *itf;
    struct lsm6dso *lsm6dso;
    struct lsm6dso_cfg *cfg;

    /* Check if requested sensor type is supported */
    if (!(type & SENSOR_TYPE_ACCELEROMETER) &&
        !(type & SENSOR_TYPE_GYROSCOPE) &&
        !(type & SENSOR_TYPE_TEMPERATURE)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);

    /* Retrive configuration from device for sensitivity */
    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    cfg = &lsm6dso->cfg;

    rc = lsm6dso_spi_fixup(sensor, itf, 0);
    if (rc) {
        goto err;
    }

    if ((type & SENSOR_TYPE_ACCELEROMETER) ||
        (type & SENSOR_TYPE_GYROSCOPE)) {
        struct sensor_accel_data sad;

        /* Acc and Gyro data can share the same data structure */
        rc = lsm6dso_get_ag_data(itf, type, &sad, cfg);
        if (rc) {
            goto err;
        }

        rc = data_func(sensor, data_arg, &sad, type);
        if (rc) {
            goto err;
        }
    }

    if (type & SENSOR_TYPE_TEMPERATURE) {
        struct sensor_temp_data std;

        rc = lsm6dso_get_temp_data(itf, &std);
        if (rc) {
            goto err;
        }

        /* Call data function */
        rc = data_func(sensor, data_arg, &std, SENSOR_TYPE_TEMPERATURE);
        if (rc) {
            goto err;
        }
    }

err:
    return rc;
}

/**
 * Read sensor data
 *
 * @param sensor The sensor object
 * @param type sensor type (acc, gyro etc)
 * @param data_func callback register data function
 * @param data_arg function arguments
 * @param timeout
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lsm6dso_sensor_read(struct sensor *sensor, sensor_type_t type,
                               sensor_data_func_t data_func, void *data_arg,
                               uint32_t timeout)
{
    struct lsm6dso *lsm6dso;
    struct lsm6dso_cfg *cfg;

    /* Check if requested sensor type is supported */
    if (!(type & SENSOR_TYPE_ACCELEROMETER) &&
        !(type & SENSOR_TYPE_GYROSCOPE) &&
        !(type & SENSOR_TYPE_TEMPERATURE)) {
        return SYS_EINVAL;
    }

    /* Retrive configuration from device for sensitivity */
    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    cfg = &lsm6dso->cfg;

    if (cfg->read.mode == LSM6DSO_READ_POLL) {
        return lsm6dso_poll_read(sensor, type, data_func, data_arg, timeout);
    }

    return lsm6dso_stream_read(sensor, type, data_func, data_arg, timeout);
}

static struct lsm6dso_notif_cfg *
    lsm6dso_find_notif_cfg_by_event(sensor_event_type_t event,
                                    struct lsm6dso_cfg *cfg)
{
    int i;
    struct lsm6dso_notif_cfg *notif_cfg = NULL;

    if (!cfg) {
        return NULL;
    }

    for (i = 0; i < cfg->max_num_notif; i++) {
        if (event == cfg->notif_cfg[i].event) {
            notif_cfg = &cfg->notif_cfg[i];
            break;
        }
    }

    if (i == cfg->max_num_notif) {
        return NULL;
    }

    return notif_cfg;
}

static int lsm6dso_notify(struct lsm6dso *lsm6dso, uint8_t src,
                          sensor_event_type_t event_type)
{
    struct lsm6dso_notif_cfg *notif_cfg;

    notif_cfg = lsm6dso_find_notif_cfg_by_event(event_type, &lsm6dso->cfg);
    if (!notif_cfg) {
        return SYS_EINVAL;
    }

    if (src & notif_cfg->int_mask) {
        sensor_mgr_put_notify_evt(&lsm6dso->pdd.notify_ctx, event_type);
        return 0;
    }

    return -1;
}

static void lsm6dso_inc_notif_stats(sensor_event_type_t event)
{
#if MYNEWT_VAL(LSM6DSO_NOTIF_STATS)
    switch (event) {
        case SENSOR_EVENT_TYPE_SINGLE_TAP:
            STATS_INC(g_lsm6dsostats, single_tap_notify);
            break;
        case SENSOR_EVENT_TYPE_DOUBLE_TAP:
            STATS_INC(g_lsm6dsostats, double_tap_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_CHANGE:
            STATS_INC(g_lsm6dsostats, orientation_notify);
            break;
        case SENSOR_EVENT_TYPE_SLEEP_CHANGE:
            STATS_INC(g_lsm6dsostats, sleep_notify);
            break;
        case SENSOR_EVENT_TYPE_WAKEUP:
            STATS_INC(g_lsm6dsostats, wakeup_notify);
            break;
        case SENSOR_EVENT_TYPE_FREE_FALL:
            STATS_INC(g_lsm6dsostats, free_fall_notify);
            break;
        default:
            break;
    }
#endif /* LSM6DSO_NOTIF_STATS */
}

/**
 * Manage events from sensor
 *
 * @param sensor The sensor object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lsm6dso_sensor_handle_interrupt(struct sensor *sensor)
{
    struct lsm6dso *lsm6dso;
    struct sensor_itf *itf;
    uint8_t int_src[4];
    int rc;

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);

    rc = lsm6dso_clear_int(itf, int_src);
    if (rc) {
        LSM6DSO_LOG(ERROR, "Could not read int src err=0x%02x\n", rc);
        return rc;
    }

    rc = lsm6dso_notify(lsm6dso, int_src[0], SENSOR_EVENT_TYPE_SINGLE_TAP);
    if (!rc) {
        lsm6dso_inc_notif_stats(SENSOR_EVENT_TYPE_SINGLE_TAP);
    }

    rc = lsm6dso_notify(lsm6dso, int_src[0], SENSOR_EVENT_TYPE_DOUBLE_TAP);
    if (!rc) {
        lsm6dso_inc_notif_stats(SENSOR_EVENT_TYPE_DOUBLE_TAP);
    }

    rc = lsm6dso_notify(lsm6dso, int_src[0], SENSOR_EVENT_TYPE_FREE_FALL);
    if (!rc) {
        lsm6dso_inc_notif_stats(SENSOR_EVENT_TYPE_FREE_FALL);
    }

    rc = lsm6dso_notify(lsm6dso, int_src[0], SENSOR_EVENT_TYPE_WAKEUP);
    if (!rc) {
        lsm6dso_inc_notif_stats(SENSOR_EVENT_TYPE_WAKEUP);
    }

    rc = lsm6dso_notify(lsm6dso, int_src[0], SENSOR_EVENT_TYPE_SLEEP_CHANGE);
    if (!rc) {
        lsm6dso_inc_notif_stats(SENSOR_EVENT_TYPE_SLEEP_CHANGE);
    }

    rc = lsm6dso_notify(lsm6dso, int_src[0], SENSOR_EVENT_TYPE_ORIENT_CHANGE);
    if (!rc) {
        lsm6dso_inc_notif_stats(SENSOR_EVENT_TYPE_ORIENT_CHANGE);
    }

    return 0;
}

/**
 * Find registered events in available event list
 *
 * @param event object
 * @param int_cfg interrupt bit configuration
 * @param int_reg interrupt register
 * @param int_num interrupt number
 * @param cfg lsm6dso_cfg configuration structure
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lsm6dso_find_int_by_event(sensor_event_type_t event, uint8_t *int_en,
                          uint8_t *int_num, struct lsm6dso_cfg *cfg)
{
    int i;

    *int_num = 0;
    *int_en = 0;

    if (!cfg) {
        return SYS_EINVAL;
    }

    /* Search in enabled event list */
    for (i = 0; i < cfg->max_num_notif; i++) {
        if (event == cfg->notif_cfg[i].event) {
            *int_en = cfg->notif_cfg[i].int_en;
            *int_num = cfg->notif_cfg[i].int_num;
            break;
        }
    }

    if (i == cfg->max_num_notif) {
        return SYS_EINVAL;
    }

    return 0;
}

/**
 * Read notification event enabled
 *
 * @param sensor object
 * @param cfg lsm6dso_cfg object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lsm6dso_sensor_reset(struct sensor *sensor)
{
    struct lsm6dso *lsm6dso;
    struct sensor_itf *itf;

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(&(lsm6dso->sensor));

    return lsm6dso_reset(itf);
}

/**
 * Enable notification event
 *
 * @param sensor object
 * @param event object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lsm6dso_sensor_set_notification(struct sensor *sensor,
                                           sensor_event_type_t event)
{
    struct lsm6dso *lsm6dso;
    struct lsm6dso_pdd *pdd;
    uint8_t int_num;
    uint8_t int_mask;
    int rc;

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    pdd = &lsm6dso->pdd;

    rc = lsm6dso_find_int_by_event(event, &int_mask, &int_num, &lsm6dso->cfg);
    if (rc) {
        goto err;
    }

    rc = enable_interrupt(sensor, int_mask, int_num);
    if (rc) {
        goto err;
    }

    pdd->notify_ctx.snec_evtype |= event;

    if (pdd->notify_ctx.snec_evtype) {
        rc = enable_embedded_interrupt(sensor, 1);
      }

err:
    return rc;
}

/**
 * Disable notification event
 *
 * @param sensor object
 * @param event object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lsm6dso_sensor_unset_notification(struct sensor *sensor,
                                             sensor_event_type_t event)
{
    struct lsm6dso *lsm6dso;
    uint8_t int_num;
    uint8_t int_mask;
    int rc;

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);
    lsm6dso->pdd.notify_ctx.snec_evtype &= ~event;

    rc = lsm6dso_find_int_by_event(event, &int_mask, &int_num, &lsm6dso->cfg);
    if (rc) {
        goto err;
    }

    rc = disable_interrupt(sensor, int_mask, int_num);
    if (rc) {
        goto err;
    }

    if (lsm6dso->pdd.notify_ctx.snec_evtype)
        rc = enable_embedded_interrupt(sensor, 0);

err:
    return rc;
}

/**
 * Read notification event enabled
 *
 * @param sensor object
 * @param type of sensor
 * @param cfg sensor_cfg object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lsm6dso_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                                     struct sensor_cfg *cfg)
{
    if ((type != SENSOR_TYPE_ACCELEROMETER) &&
        (type != SENSOR_TYPE_GYROSCOPE) &&
        (type != SENSOR_TYPE_TEMPERATURE)) {
        return SYS_EINVAL;
    }

    if (type != SENSOR_TYPE_TEMPERATURE) {
        cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;
    } else {
        cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;
    }

    return 0;
}

/**
 * Read notification event enabled
 *
 * @param sensor object
 * @param cfg lsm6dso_cfg object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int lsm6dso_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct lsm6dso *lsm6dso;

    lsm6dso = (struct lsm6dso *)SENSOR_GET_DEVICE(sensor);

    return lsm6dso_config(lsm6dso, (struct lsm6dso_cfg*)cfg);
}

/**
 * Configure the sensor
 *
 * @param sensor driver
 * @param sensor driver config
 *
 * @return 0 on success, non-zero error on failure.
 */
int lsm6dso_config(struct lsm6dso *lsm6dso, struct lsm6dso_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;
    uint8_t chip_id;
    struct sensor *sensor;

    itf = SENSOR_GET_ITF(&(lsm6dso->sensor));
    sensor = &(lsm6dso->sensor);

    rc = lsm6dso_spi_fixup(sensor, itf, 0);
    if (rc) {
        goto err;
    }

    rc = lsm6dso_get_chip_id(itf, &chip_id);
    if (rc) {
        goto err;
    }

    if (chip_id != LSM6DSO_WHO_AM_I) {
        rc = SYS_EINVAL;
        goto err;
    }

   rc = lsm6dso_reset(itf);
    if (rc) {
        goto err;
    }

    rc = lsm6dso_set_bdu(itf, LSM6DSO_EN_BIT);
    if (rc) {
        goto err;
    }

    rc = lsm6dso_set_g_full_scale(itf, cfg->gyro_fs);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.gyro_fs = cfg->gyro_fs;

    rc = lsm6dso_get_gyro_sensitivity(lsm6dso->cfg.gyro_fs,
                                      &lsm6dso->cfg.gyro_sensitivity);
    if (rc) {
        goto err;
    }

    rc = lsm6dso_set_xl_full_scale(itf, cfg->acc_fs);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.acc_fs = cfg->acc_fs;

    rc = lsm6dso_get_acc_sensitivity(lsm6dso->cfg.acc_fs,
                                     &lsm6dso->cfg.acc_sensitivity);
    if (rc) {
        goto err;
    }

    rc = lsm6dso_set_g_rate(itf, cfg->gyro_rate);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.gyro_rate = cfg->gyro_rate;

    rc = lsm6dso_set_xl_rate(itf, cfg->acc_rate);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.acc_rate = cfg->acc_rate;

    rc = lsm6dso_chan_enable(itf, LSM6DSO_DEN_X_MASK |
                             LSM6DSO_DEN_Y_MASK |
                             LSM6DSO_DEN_Z_MASK);
    if (rc) {
        goto err;
    }

    rc = lsm6dso_set_offsets(itf, 0, 0, 0);
    if (rc) {
        goto err;
    }

    rc = lsm6dso_set_offset_enable(itf, LSM6DSO_EN_BIT);
    if (rc) {
        goto err;
    }

    /*
     * Disable FIFO by default
     * Save FIFO default configuration value to be used later on
     */
    rc = lsm6dso_set_fifo_mode(itf, LSM6DSO_FIFO_MODE_BYPASS_VAL);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.fifo.mode = cfg->fifo.mode;

    rc = lsm6dso_set_fifo_watermark(itf, cfg->fifo.wtm);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.fifo.wtm = cfg->fifo.wtm;

    /* Add embedded gesture configuration */
    rc = lsm6dso_set_wake_up(itf, &cfg->wk);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.wk = cfg->wk;

    rc = lsm6dso_set_freefall(itf, &cfg->ff);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.ff = cfg->ff;

    rc = lsm6dso_set_tap_cfg(itf, &cfg->tap);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.tap = cfg->tap;

    rc = lsm6dso_set_orientation(itf, &cfg->orientation);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.orientation = cfg->orientation;

    if (!cfg->notif_cfg) {
        lsm6dso->cfg.notif_cfg = (struct lsm6dso_notif_cfg *)dflt_notif_cfg;
        lsm6dso->cfg.max_num_notif = LSM6DSO_ARRAY_SIZE(dflt_notif_cfg);
    } else {
        lsm6dso->cfg.notif_cfg = cfg->notif_cfg;
        lsm6dso->cfg.max_num_notif = cfg->max_num_notif;
    }

    rc = sensor_set_type_mask(sensor, cfg->lc_s_mask);
    if (rc) {
        goto err;
    }
    lsm6dso->cfg.lc_s_mask = cfg->lc_s_mask;

    lsm6dso->cfg.read.int_cfg = cfg->read.int_cfg;
    lsm6dso->cfg.read.int_num = cfg->read.int_num;
    lsm6dso->cfg.read.mode = cfg->read.mode;

err:
    return rc;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void init_node_cb(struct bus_node *bnode, void *arg)
{
    struct sensor_itf *itf = arg;

    lsm6dso_init((struct os_dev *)bnode, itf);
}

int
lsm6dso_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                               const struct bus_i2c_node_cfg *i2c_cfg,
                               struct sensor_itf *sensor_itf)
{
    struct lsm6dso *dev = (struct lsm6dso *)node;
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    dev->node_is_spi = false;

    sensor_itf->si_dev = &node->bnode.odev;
    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_i2c_node_create(name, node, i2c_cfg, sensor_itf);

    return rc;
}

int
lsm6dso_create_spi_sensor_dev(struct bus_spi_node *node, const char *name,
                               const struct bus_spi_node_cfg *spi_cfg,
                               struct sensor_itf *sensor_itf)
{
    struct lsm6dso *dev = (struct lsm6dso *)node;
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    dev->node_is_spi = true;

    sensor_itf->si_dev = &node->bnode.odev;
    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_spi_node_create(name, node, spi_cfg, sensor_itf);

    return rc;
}
#endif /* BUS_DRIVER_PRESENT */
