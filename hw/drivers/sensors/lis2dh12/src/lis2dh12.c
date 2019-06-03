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

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "os/mynewt.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#else
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"
#endif
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "lis2dh12/lis2dh12.h"
#include "lis2dh12_priv.h"
#include "hal/hal_gpio.h"
#include "modlog/modlog.h"
#include "stats/stats.h"
#include <syscfg/syscfg.h>
#include <sensor/sensor.h>
#if LIS2DH12_PRINT_INTR
#include "console/console.h"
#endif

#ifndef LIS2DH12_PRINT_INTR
#define LIS2DH12_PRINT_INTR     (0)
#endif

/*
 * Max time to wait for interrupt.
 */
#define LIS2DH12_MAX_INT_WAIT (4 * OS_TICKS_PER_SEC)

static const struct lis2dh12_notif_cfg dflt_notif_cfg[] = {
    {
      .event     = SENSOR_EVENT_TYPE_SINGLE_TAP,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_CLICK_SRC_SCLICK,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_CLICK
    },
    {
      .event     = SENSOR_EVENT_TYPE_DOUBLE_TAP,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_CLICK_SRC_DCLICK,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_CLICK
    },
    {
      .event     = SENSOR_EVENT_TYPE_SLEEP,
      .int_num   = 1, // Must be 1
      .notif_src = 0, // Not used host pin is read for state
      .int_cfg   = LIS2DH12_CTRL_REG6_I2_ACT
    },
    {
      .event     = SENSOR_EVENT_TYPE_FREE_FALL,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_NOTIF_SRC_INT2_IA,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_IA2
    },
    {
      .event     = SENSOR_EVENT_TYPE_WAKEUP,
      .int_num   = 1, // Must be 1
      .notif_src = 0, // Not used host pin is read for state
      .int_cfg   = LIS2DH12_CTRL_REG6_I2_ACT
    },
    {
      .event     = SENSOR_EVENT_TYPE_SLEEP_CHANGE,
      .int_num   = 1, // Must be 1
      .notif_src = 0, // Not used host pin is read for state
      .int_cfg   = LIS2DH12_CTRL_REG6_I2_ACT
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_CHANGE,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_NOTIF_SRC_INT1_IA,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_IA1
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_NOTIF_SRC_INT1_XL,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_IA1
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_NOTIF_SRC_INT1_XH,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_IA1
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_NOTIF_SRC_INT1_YL,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_IA1
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_NOTIF_SRC_INT1_YH,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_IA1
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_NOTIF_SRC_INT1_ZL,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_IA1
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE,
      .int_num   = 0, // Should be 0, unless inactivity interrupt is not used
      .notif_src = LIS2DH12_NOTIF_SRC_INT1_ZH,
      .int_cfg   = LIS2DH12_CTRL_REG3_I1_IA1
    }
};

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
static struct hal_spi_settings spi_lis2dh12_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};
#endif

/* Define the stats section and records */
STATS_SECT_START(lis2dh12_stat_section)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(read_errors)
#if MYNEWT_VAL(LIS2DH12_NOTIF_STATS)
    STATS_SECT_ENTRY(single_tap_notify)
    STATS_SECT_ENTRY(double_tap_notify)
    STATS_SECT_ENTRY(free_fall_notify)
    STATS_SECT_ENTRY(sleep_notify)
    STATS_SECT_ENTRY(wakeup_notify)
    STATS_SECT_ENTRY(sleep_chg_notify)
    STATS_SECT_ENTRY(orient_chg_notify)
    STATS_SECT_ENTRY(orient_chg_x_l_notify)
    STATS_SECT_ENTRY(orient_chg_y_l_notify)
    STATS_SECT_ENTRY(orient_chg_z_l_notify)
    STATS_SECT_ENTRY(orient_chg_x_h_notify)
    STATS_SECT_ENTRY(orient_chg_y_h_notify)
    STATS_SECT_ENTRY(orient_chg_z_h_notify)
#endif
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lis2dh12_stat_section)
    STATS_NAME(lis2dh12_stat_section, write_errors)
    STATS_NAME(lis2dh12_stat_section, read_errors)
#if MYNEWT_VAL(LIS2DH12_NOTIF_STATS)
    STATS_NAME(lis2dh12_stat_section, single_tap_notify)
    STATS_NAME(lis2dh12_stat_section, double_tap_notify)
    STATS_NAME(lis2dh12_stat_section, free_fall_notify)
    STATS_NAME(lis2dh12_stat_section, sleep_notify)
    STATS_NAME(lis2dh12_stat_section, wakeup_notify)
    STATS_NAME(lis2dh12_stat_section, sleep_chg_notify)
    STATS_NAME(lis2dh12_stat_section, orient_chg_notify)
    STATS_NAME(lis2dh12_stat_section, orient_chg_x_l_notify)
    STATS_NAME(lis2dh12_stat_section, orient_chg_y_l_notify)
    STATS_NAME(lis2dh12_stat_section, orient_chg_z_l_notify)
    STATS_NAME(lis2dh12_stat_section, orient_chg_x_h_notify)
    STATS_NAME(lis2dh12_stat_section, orient_chg_y_h_notify)
    STATS_NAME(lis2dh12_stat_section, orient_chg_z_h_notify)
#endif
STATS_NAME_END(lis2dh12_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lis2dh12_stat_section) g_lis2dh12stats;

#define LIS2DH12_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(LIS2DH12_LOG_MODULE), __VA_ARGS__)

/* Exports for the sensor API */
static int lis2dh12_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int lis2dh12_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);
static int lis2dh12_sensor_set_config(struct sensor *, void *);
static int
lis2dh12_sensor_set_trigger_thresh(struct sensor *, sensor_type_t,
                                   struct sensor_type_traits *);
static int
lis2dh12_sensor_clear_low_thresh(struct sensor *, sensor_type_t);

static int
lis2dh12_sensor_clear_high_thresh(struct sensor *, sensor_type_t);

static int lis2dh12_sensor_set_notification(struct sensor *,
                                            sensor_event_type_t);
static int lis2dh12_sensor_unset_notification(struct sensor *,
                                              sensor_event_type_t);
static int lis2dh12_sensor_handle_interrupt(struct sensor *);

static const struct sensor_driver g_lis2dh12_sensor_driver = {
    .sd_read = lis2dh12_sensor_read,
    .sd_set_config = lis2dh12_sensor_set_config,
    .sd_get_config = lis2dh12_sensor_get_config,
    /* Setting trigger threshold is optional */
    .sd_set_trigger_thresh = lis2dh12_sensor_set_trigger_thresh,
    .sd_clear_low_trigger_thresh = lis2dh12_sensor_clear_low_thresh,
    .sd_clear_high_trigger_thresh = lis2dh12_sensor_clear_high_thresh,
    .sd_set_notification   = lis2dh12_sensor_set_notification,
    .sd_unset_notification = lis2dh12_sensor_unset_notification,
    .sd_handle_interrupt   = lis2dh12_sensor_handle_interrupt
};

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
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
    if (len > 1)
    {
        addr |= LIS2DH12_I2C_ADDR_INC;
    }

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
    rc = i2cn_master_write(itf->si_num, &data_struct, MYNEWT_VAL(LIS2DH12_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(LIS2DH12_I2C_RETRIES));
    if (rc) {
        LIS2DH12_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                     data_struct.address);
        STATS_INC(g_lis2dh12stats, read_errors);
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = i2cn_master_read(itf->si_num, &data_struct, MYNEWT_VAL(LIS2DH12_I2C_TIMEOUT_TICKS), 1,
                          MYNEWT_VAL(LIS2DH12_I2C_RETRIES));
    if (rc) {
        LIS2DH12_LOG(ERROR, "Failed to read from 0x%02X:0x%02X\n",
                     data_struct.address, addr);
        STATS_INC(g_lis2dh12stats, read_errors);
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
        addr |= LIS2DH12_SPI_ADDR_INC;
    }

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, addr);
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        LIS2DH12_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                     itf->si_num, addr);
        STATS_INC(g_lis2dh12stats, read_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0x55);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            LIS2DH12_LOG(ERROR, "SPI_%u read failed addr:0x%02X\n",
                         itf->si_num, addr);
            STATS_INC(g_lis2dh12stats, read_errors);
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
 * Write multiple length data to LIS2DH12 sensor over I2C  (MAX: 19 bytes)
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

    if (len > (sizeof(payload) - 1)) {
        rc = OS_EINVAL;
        goto err;
    }

    if (len > 1) {
        payload[0] += LIS2DH12_I2C_ADDR_INC;
    }
    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = i2cn_master_write(itf->si_num, &data_struct, MYNEWT_VAL(LIS2DH12_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(LIS2DH12_I2C_RETRIES));
    if (rc) {
        LIS2DH12_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                     data_struct.address);
        STATS_INC(g_lis2dh12stats, write_errors);
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
        addr |= LIS2DH12_SPI_ADDR_INC;
    }

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);


    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, addr);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        LIS2DH12_LOG(ERROR, "SPI_%u register write failed addr:0x%02X\n",
                     itf->si_num, addr);
        STATS_INC(g_lis2dh12stats, write_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        rc = hal_spi_tx_val(itf->si_num, payload[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            LIS2DH12_LOG(ERROR, "SPI_%u write failed addr:0x%02X\n",
                         itf->si_num, addr);
            STATS_INC(g_lis2dh12stats, write_errors);
            goto err;
        }
    }


    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    return rc;
}
#endif

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

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct {
        uint8_t addr;
        /*
         * XXX lis2dh12_i2c_writelen has max payload of 20 including addr, not
         * sure where it comes from
         */
        uint8_t payload[19];
    } write_data;
    struct lis2dh12 *dev = (struct lis2dh12 *)itf->si_dev;

    if (dev->node_is_spi) {
        addr |= LIS2DH12_SPI_ADDR_INC;
    } else {
        addr |= LIS2DH12_I2C_ADDR_INC;
    }

    if (len > sizeof(write_data.payload)) {
        return -1;
    }

    write_data.addr = addr;
    memcpy(write_data.payload, payload, len);

    rc = bus_node_simple_write(itf->si_dev, &write_data, len + 1);
#else
    rc = sensor_itf_lock(itf, MYNEWT_VAL(LIS2DH12_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lis2dh12_i2c_writelen(itf, addr, payload, len);
    } else {
        rc = lis2dh12_spi_writelen(itf, addr, payload, len);
    }

    sensor_itf_unlock(itf);
#endif

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

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct lis2dh12 *dev = (struct lis2dh12 *)itf->si_dev;

    if (dev->node_is_spi) {
        addr |= LIS2DH12_SPI_READ_CMD_BIT;
        addr |= LIS2DH12_SPI_ADDR_INC;
    } else {
        addr |= LIS2DH12_I2C_ADDR_INC;
    }

    rc = bus_node_simple_write_read_transact(itf->si_dev, &addr, 1, payload, len);
#else
    rc = sensor_itf_lock(itf, MYNEWT_VAL(LIS2DH12_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = lis2dh12_i2c_readlen(itf, addr, payload, len);
    } else {
        rc = lis2dh12_spi_readlen(itf, addr, payload, len);
    }

    sensor_itf_unlock(itf);
#endif

    return rc;
}

/**
 * Write byte to sensor over different interfaces
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    return lis2dh12_writelen(itf, reg, &value, 1);
}

/**
 * Read single register from LIS2DH12 sensor over different interfaces
 *
 * @param interface to use
 * @param register address
 * @param pinter to register data
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_read8(struct sensor_itf *itf, uint8_t addr, uint8_t *reg)
{
    return lis2dh12_readlen(itf, addr, reg, 1);
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
        LIS2DH12_LOG(ERROR, "Invalid full scale value\n");
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
 * @param acc value in mg
 * @param float ptr to return calculated value in ms2
 */
void
lis2dh12_calc_acc_ms2(int16_t acc_mg, float *acc_ms2)
{
    *acc_ms2 = (acc_mg * STANDARD_ACCEL_GRAVITY)/1000;
}

/**
 * Calculates the acceleration in mg from m/s^2
 *
 * @param acc value in m/s^2
 * @param int16 ptr to return calculated value in mg
 */
void
lis2dh12_calc_acc_mg(float acc_ms2, int16_t *acc_mg)
{
    *acc_mg = (acc_ms2 * 1000)/STANDARD_ACCEL_GRAVITY;
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
        LIS2DH12_LOG(ERROR, "Invalid rate value\n");
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
 * Set FIFO mode
 *
 * @param the sensor interface
 * @param mode
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_fifo_mode(struct sensor_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG5, &reg, 1);
    if (rc) {
        goto err;
    }

    reg |= LIS2DH12_CTRL_REG5_FIFO_EN;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG5, &reg, 1);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_FIFO_CTRL_REG, &reg, 1);
    if (rc) {
        goto err;
    }

    reg &= 0x3f;
    reg |= mode << 6;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_FIFO_CTRL_REG, &reg, 1);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_FIFO_SRC_REG, &reg, 1);
    if (rc) {
        goto err;
    }

    if (mode == LIS2DH12_FIFO_M_BYPASS && reg != LIS2DH12_FIFO_SRC_EMPTY) {
        rc = SYS_EINVAL;
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Get Number of Samples in FIFO
 *
 * @param the sensor interface
 * @param Pointer to return number of samples in
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_fifo_samples(struct sensor_itf *itf, uint8_t *samples)
{
    uint8_t reg;
    int rc;

    rc = lis2dh12_read8(itf, LIS2DH12_REG_FIFO_SRC_REG, &reg);
    if (rc == 0) {
        *samples = reg & LIS2DH12_FIFO_SRC_FSS;
    }

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
 * Set high pass filter cfg
 *
 * @param the sensor interface
 * @param filter register settings
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_hpf_cfg(struct sensor_itf *itf, uint8_t reg)
{
    return lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG2, &reg, 1);
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

    /* reset filtering block */
    rc = lis2dh12_readlen(itf, LIS2DH12_REG_REFERENCE, &reg, 1);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG4, &reg, 1);
    if (rc) {
        goto err;
    }

    /* Set HR bit */
    reg &= ~LIS2DH12_CTRL_REG4_HR;
    reg |= (mode & 0x08);

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG4, &reg, 1);
    if (rc) {
        goto err;
    }

    /* Set LP bit */
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



    os_time_delay(OS_TICKS_PER_SEC/1000 + 1);

    return 0;
err:
    return rc;
}

int
lis2dh12_get_fs(struct sensor_itf *itf, uint8_t *fs)
{
    int rc;

    rc = lis2dh12_get_full_scale(itf, fs);
    if (rc) {
        return rc;
    }

    if (*fs == LIS2DH12_FS_2G) {
        *fs = 2;
    } else if (*fs == LIS2DH12_FS_4G) {
        *fs = 4;
    } else if (*fs == LIS2DH12_FS_8G) {
        *fs = 8;
    } else if (*fs == LIS2DH12_FS_16G) {
        *fs = 16;
    } else {
        return SYS_EINVAL;
    }

    return 0;
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
lis2dh12_get_data(struct sensor_itf *itf, uint8_t fs, int16_t *x, int16_t *y, int16_t *z)
{
    int rc;

    uint8_t payload[6] = {0};
    uint8_t status;
    *x = *y = *z = 0;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_STATUS_REG, &status, 1);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_OUT_X_L, payload, 6);
    if (rc) {
        goto err;
    }

    *x = payload[0] | (payload[1] << 8);
    *y = payload[2] | (payload[3] << 8);
    *z = payload[4] | (payload[5] << 8);

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

static int
init_intpin(struct lis2dh12 *lis2dh12, int int_num, hal_gpio_irq_handler_t handler,
            void *arg)
{
    hal_gpio_irq_trig_t trig;
    int pin;
    int rc;

    pin = lis2dh12->sensor.s_itf.si_ints[int_num].host_pin;
    if (pin >= 0) {
        trig = lis2dh12->sensor.s_itf.si_ints[int_num].active;

        rc = hal_gpio_irq_init(pin,
                               handler,
                               arg,
                               trig,
                               HAL_GPIO_PULL_NONE);
    }
    if (pin < 0) {
        LIS2DH12_LOG(ERROR, "Interrupt pin not configured\n");
        return SYS_EINVAL;
    }

    if (rc != 0) {
        LIS2DH12_LOG(ERROR, "Failed to initialize interrupt pin %d\n", pin);
        return rc;
    }

    return 0;
}

static void
init_interrupt(struct lis2dh12_int *interrupt, struct sensor_int *ints)
{
    os_error_t error;

    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints = ints;
}

static void
undo_interrupt(struct lis2dh12_int *interrupt)
{
    OS_ENTER_CRITICAL(interrupt->lock);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(interrupt->lock);
}

static int
wait_interrupt(struct lis2dh12_int *interrupt, uint8_t int_num)
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
        error = os_sem_pend(&interrupt->wait, LIS2DH12_MAX_INT_WAIT);
        if (error == OS_TIMEOUT) {
            return error;
        }
        assert(error == OS_OK);
    }
    return OS_OK;
}

static void
wake_interrupt(struct lis2dh12_int *interrupt)
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

        error = os_sem_release(&interrupt->wait);
        assert(error == OS_OK);
    }
}

/**
 * IRQ handler for interrupts for high/low threshold
 *
 * @param arg
 */
static void
lis2dh12_int_irq_handler(void *arg)
{
    struct sensor *sensor = arg;
    struct lis2dh12 *lis2dh12;

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);

    if (lis2dh12->pdd.interrupt) {
        wake_interrupt(lis2dh12->pdd.interrupt);
    }

    sensor_mgr_put_interrupt_evt(sensor);
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

    sensor = &lis2dh12->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lis2dh12stats),
        STATS_SIZE_INIT_PARMS(g_lis2dh12stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lis2dh12_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lis2dh12stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

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

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
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
#endif

    init_interrupt(&lis2dh12->intr, lis2dh12->sensor.s_itf.si_ints);

    lis2dh12->pdd.notify_ctx.snec_sensor = sensor;
    lis2dh12->pdd.interrupt = NULL;

    rc = init_intpin(lis2dh12, 0, lis2dh12_int_irq_handler, sensor);
    if (rc) {
        return rc;
    }
    rc = init_intpin(lis2dh12, 1, lis2dh12_int_irq_handler, sensor);
    if (rc) {
        return rc;
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

/**
 * Sets the interrupt push-pull/open-drain selection
 *
 * @param The sensor interface
 * @param interrupt setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int_pp_od(struct sensor_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dh12_read8(itf, LIS2DH12_REG_CTRL_REG6, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~LIS2DH12_CTRL_REG6_INT_POLARITY;
    reg |= mode ? LIS2DH12_CTRL_REG6_INT_POLARITY : 0;

    return lis2dh12_write8(itf, LIS2DH12_REG_CTRL_REG6, reg);
}

/**
 * Gets the interrupt push-pull/open-drain selection
 *
 * @param The sensor interface
 * @param ptr to store setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_int_pp_od(struct sensor_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t reg;

    rc = lis2dh12_read8(itf, LIS2DH12_REG_CTRL_REG6, &reg);
    if (rc) {
        return rc;
    }

    *mode = (reg & LIS2DH12_CTRL_REG6_INT_POLARITY) ? 1 : 0;

    return 0;
}

int
lis2dh12_clear_click(struct sensor_itf *itf, uint8_t *src)
{
    return lis2dh12_read8(itf, LIS2DH12_REG_CLICK_SRC, src);
}

int
lis2dh12_set_click_cfg(struct sensor_itf *itf, uint8_t cfg)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_CLICK_CFG, cfg);
}

int
lis2dh12_set_click_threshold(struct sensor_itf *itf, uint8_t cfg)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_CLICK_THS, cfg);
}

int
lis2dh12_set_click_time_limit(struct sensor_itf *itf, uint8_t limit)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_TIME_LIMIT, limit);
}

int
lis2dh12_set_click_time_latency(struct sensor_itf *itf, uint8_t latency)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_TIME_LATENCY, latency);
}

int
lis2dh12_set_click_time_window(struct sensor_itf *itf, uint8_t window)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_TIME_WINDOW, window);
}

int
lis2dh12_set_activity_threshold(struct sensor_itf *itf, uint8_t threshold)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_ACT_THS, threshold);
}

int
lis2dh12_set_activity_duration(struct sensor_itf *itf, uint8_t duration)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_ACT_DUR, duration);
}

static int
lis2dh12_wait_for_data(struct sensor_itf *itf, int timeout_ms)
{
    int rc;
    uint8_t status;
    os_time_t time_limit = os_time_get() + os_time_ms_to_ticks32(timeout_ms);

    while (1) {
        rc = lis2dh12_read8(itf, LIS2DH12_REG_STATUS_REG, &status);
        if (rc != OS_OK || (status & LIS2DH12_STATUS_ZYXDA) != 0) {
            break;
        }
        if (os_time_get() > time_limit) {
            rc = OS_TIMEOUT;
            break;
        }
        os_time_delay(1);
    }
    return rc;
}

int lis2dh12_run_self_test(struct sensor_itf *itf, int *result)
{
    int rc, rc2;
    int i;
    int16_t no_st[3], st[3], data[3];
    int32_t scratch[3] = { 0 };
    uint8_t prev_config[4];
    uint8_t config[4] = { LIS2DH12_CTRL_REG1_XPEN |
                          LIS2DH12_CTRL_REG1_YPEN |
                          LIS2DH12_CTRL_REG1_ZPEN |
                          LIS2DH12_DATA_RATE_50HZ,
                          0, 0,
                          LIS2DH12_ST_MODE_DISABLE |
                          LIS2DH12_FS_2G |
                          LIS2DH12_CTRL_REG4_BDU };
    int16_t diff;
    uint8_t fifo_ctrl = 0;
    const int read_count = 5;

    *result = 0;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG1, prev_config, 4);
    if (rc) {
        return rc;
    }

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG2, &config[1], 3);
    if (rc) {
        goto end;
    }

    rc = lis2dh12_write8(itf, LIS2DH12_REG_CTRL_REG1, config[0]);
    if (rc) {
        goto end;
    }

    /* Turn on bypass mode for fifo */
    rc = lis2dh12_read8(itf, LIS2DH12_REG_FIFO_CTRL_REG, &fifo_ctrl);
    if (rc) {
        goto end;
    }

    if (fifo_ctrl) {
        rc = lis2dh12_write8(itf, LIS2DH12_REG_FIFO_CTRL_REG, 0);
        if (rc) {
            goto end;
        }
    }
    /* Wait 90ms */
    os_time_delay(90 * OS_TICKS_PER_SEC / 1000 + 1);

    /* Wait 1ms for first sample */
    rc = lis2dh12_wait_for_data(itf, 1);
    if (rc) {
        goto end;
    }

    /* Read and discard data */
    rc = lis2dh12_get_data(itf, 2, &data[0], &data[1], &data[2]);
    if (rc) {
        goto end;
    }

    /* Take no st offset reading */
    for (i = 0; i < read_count; i++) {
        /* Wait at least 20 ms, ODR is 50Hz */
        os_time_delay(20 * OS_TICKS_PER_SEC / 1000 + 1);

        rc = lis2dh12_wait_for_data(itf, 1);
        if (rc) {
            goto end;
        }

        rc = lis2dh12_get_data(itf, 2, &data[0], &data[1], &data[2]);
        if (rc) {
            goto end;
        }
        scratch[0] += data[0];
        scratch[1] += data[1];
        scratch[2] += data[2];
    }

    /* Average the stored data on each axis */
    no_st[0] = scratch[0] / read_count;
    no_st[1] = scratch[1] / read_count;
    no_st[2] = scratch[2] / read_count;

    memset(&scratch, 0, sizeof(scratch));

    /* Self test mode 0 */
    rc = lis2dh12_set_self_test_mode(itf, LIS2DH12_ST_MODE_MODE0);
    if (rc) {
        goto end;
    }

    /* Wait 90ms */
    os_time_delay(90 * OS_TICKS_PER_SEC / 1000 + 1);

    /* Wait 1 ms for first sample */
    rc = lis2dh12_wait_for_data(itf, 1);
    if (rc) {
        goto end;
    }

    /* Read and discard data */
    rc = lis2dh12_get_data(itf, 2, &data[0], &data[1], &data[2]);
    if (rc) {
        goto end;
    }

    for (i = 0; i < read_count; i++) {
        /* Wait at least 20 ms, ODR is 50Hz */
        os_time_delay(20 * OS_TICKS_PER_SEC / 1000 + 1);

        rc = lis2dh12_wait_for_data(itf, 1);
        if (rc) {
            goto end;
        }

        rc = lis2dh12_get_data(itf, 2, &data[0], &data[1], &data[2]);
        if (rc) {
            goto end;
        }
        scratch[0] += data[0];
        scratch[1] += data[1];
        scratch[2] += data[2];
    }

    /* Average the stored data on each axis */
    st[0] = scratch[0] / read_count;
    st[1] = scratch[1] / read_count;
    st[2] = scratch[2] / read_count;

    /* |Min(ST_X)| <=|OUTX_AVG_ST - OUTX_AVG_NO_ST| <= |Max(ST_X)| */
    /* Compare values to thresholds */
    for (i = 0; i < LIS2DH12_AXIS_MAX; i++) {
        diff = abs(st[i] - no_st[i]);
        if (diff < LIS2DH12_ST_MIN || diff > LIS2DH12_ST_MAX) {
            *result |= 1 << i;
        }
    }
end:
    /* Restore fifo mode if was set */
    if (fifo_ctrl) {
        rc2 = lis2dh12_write8(itf, LIS2DH12_REG_FIFO_CTRL_REG, fifo_ctrl);
        if (rc == OS_OK && rc2 != OS_OK) {
            rc = rc2;
        }
    }
    /* Disable self test mode, and restore other settings */
    rc2 = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG1, prev_config, 4);
    if (rc == OS_OK && rc2 != OS_OK) {
        rc = rc2;
    }

    return rc;
}

static int
disable_interrupt(struct sensor *sensor, uint8_t int_to_disable, uint8_t int_num)
{
    struct lis2dh12 *lis2dh12;
    struct lis2dh12_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    if (int_to_disable == 0) {
        return SYS_EINVAL;
    }

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dh12->pdd;

    pdd->int_enable[int_num] &= ~(int_to_disable);

    /* disable GPIO int if no longer needed */
    if (!pdd->int_enable[int_num]) {
        hal_gpio_irq_disable(itf->si_ints[int_num].host_pin);
    }

    /* update interrupt setup in device */
    if (int_num == 0) {
        rc = lis2dh12_set_int1_pin_cfg(itf, pdd->int_enable[int_num]);
    } else {
        rc = lis2dh12_set_int2_pin_cfg(itf, pdd->int_enable[int_num]);
    }

    return rc;
}


static int
enable_interrupt(struct sensor *sensor, uint8_t int_to_enable, uint8_t int_num)
{
    struct lis2dh12 *lis2dh12;
    struct lis2dh12_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    if (!int_to_enable) {
        rc = SYS_EINVAL;
        goto err;
    }

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dh12->pdd;

    /* if no interrupts are currently in use enable int pin */
    if (!pdd->int_enable[int_num]) {
        hal_gpio_irq_enable(itf->si_ints[int_num].host_pin);
    }

    if ((pdd->int_enable[int_num] & int_to_enable) == 0) {
        pdd->int_enable[int_num] |= int_to_enable;

        /* enable interrupt in device */
        if (int_num == 0) {
            rc = lis2dh12_set_int1_pin_cfg(itf, pdd->int_enable[int_num]);
        } else {
            rc = lis2dh12_set_int2_pin_cfg(itf, pdd->int_enable[int_num]);
        }

        if (rc) {
            disable_interrupt(sensor, int_to_enable, int_num);
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
lis2dh12_do_read(struct sensor *sensor, sensor_data_func_t data_func,
                 void * data_arg, uint8_t fs)
{
    struct sensor_accel_data sad;
    struct sensor_itf *itf;
    int16_t x, y ,z;
    float fx, fy ,fz;
    int rc;

    itf = SENSOR_GET_ITF(sensor);

    x = y = z = 0;

    rc = lis2dh12_get_data(itf, fs, &x, &y, &z);
    if (rc) {
        goto err;
    }

    /* converting values from mg to ms^2 */
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

/**
 * Do accelerometer polling reads
 *
 * @param The sensor ptr
 * @param The sensor type
 * @param The function pointer to invoke for each accelerometer reading.
 * @param The opaque pointer that will be passed in to the function.
 * @param If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int
lis2dh12_poll_read(struct sensor *sensor, sensor_type_t sensor_type,
                   sensor_data_func_t data_func, void *data_arg,
                   uint32_t timeout)
{
    struct lis2dh12 *lis2dh12;
    struct lis2dh12_cfg *cfg;
    struct sensor_itf *itf;
    uint8_t fs;
    int rc;

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    cfg = &lis2dh12->cfg;

    /* If the read isn't looking for accel data, don't do anything. */
    if (!(sensor_type & SENSOR_TYPE_ACCELEROMETER)) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (cfg->read_mode.mode != LIS2DH12_READ_M_POLL) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dh12_get_fs(itf, &fs);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_do_read(sensor, data_func, data_arg, fs);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
lis2dh12_stream_read(struct sensor *sensor,
                     sensor_type_t sensor_type,
                     sensor_data_func_t read_func,
                     void *read_arg,
                     uint32_t time_ms)
{
    struct lis2dh12_pdd *pdd;
    struct lis2dh12 *lis2dh12;
    struct sensor_itf *itf;
    struct lis2dh12_cfg *cfg;
    os_time_t time_ticks;
    os_time_t stop_ticks = 0;
    uint8_t fifo_samples;
    uint8_t fs;
    int rc, rc2;

    /* If the read isn't looking for accel data, don't do anything. */
    if (!(sensor_type & SENSOR_TYPE_ACCELEROMETER)) {
        return SYS_EINVAL;
    }

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dh12->pdd;
    cfg = &lis2dh12->cfg;

    if (cfg->read_mode.mode != LIS2DH12_READ_M_STREAM) {
        return SYS_EINVAL;
    }

    undo_interrupt(&lis2dh12->intr);

    if (pdd->interrupt) {
        return SYS_EBUSY;
    }

    /* enable interrupt */
    pdd->interrupt = &lis2dh12->intr;

    rc = enable_interrupt(sensor, cfg->read_mode.int_cfg,
                          cfg->read_mode.int_num);
    if (rc) {
        return rc;
    }

    if (time_ms != 0) {
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc) {
            goto err;
        }
        stop_ticks = os_time_get() + time_ticks;
    }

    rc = lis2dh12_get_fs(itf, &fs);
    if (rc) {
        goto err;
    }

    for (;;) {
        /* force at least one read for cases when fifo is disabled */
        rc = wait_interrupt(&lis2dh12->intr, cfg->read_mode.int_num);
        if (rc) {
            goto err;
        }
        fifo_samples = 1;

        while (fifo_samples > 0) {

            /* read all data we believe is currently in fifo */
            while (fifo_samples > 0) {
                rc = lis2dh12_do_read(sensor, read_func, read_arg, fs);
                if (rc) {
                    goto err;
                }
                fifo_samples--;

            }

            /* check if any data is available in fifo */
            rc = lis2dh12_get_fifo_samples(itf, &fifo_samples);
            if (rc) {
                goto err;
            }

        }

        if (time_ms != 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
            break;
        }

    }

err:
    /* disable interrupt */
    pdd->interrupt = NULL;
    rc2 = disable_interrupt(sensor, cfg->read_mode.int_cfg,
                            cfg->read_mode.int_num);
    if (rc) {
        return rc;
    } else {
        return rc2;
    }
}

static int
lis2dh12_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc;
    const struct lis2dh12_cfg *cfg;
    struct lis2dh12 *lis2dh12;
    struct sensor_itf *itf;

    /* If the read isn't looking for accel data, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);
    (void)itf;

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
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
#endif

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);
    cfg = &lis2dh12->cfg;

    if (cfg->read_mode.mode == LIS2DH12_READ_M_POLL) {
        rc = lis2dh12_poll_read(sensor, type, data_func, data_arg, timeout);
    } else {
        rc = lis2dh12_stream_read(sensor, type, data_func, data_arg, timeout);
    }
err:
    if (rc) {
        return SYS_EINVAL; /* XXX */
    } else {
        return SYS_EOK;
    }
}

static struct lis2dh12_notif_cfg *
lis2dh12_find_notif_cfg_by_event(sensor_event_type_t event,
                                 struct lis2dh12_cfg *cfg)
{
    int i;
    struct lis2dh12_notif_cfg *notif_cfg = NULL;

    if (!cfg) {
        goto err;
    }

    for (i = 0; i < cfg->max_num_notif; i++) {
        if (event == cfg->notif_cfg[i].event) {
            notif_cfg = &cfg->notif_cfg[i];
            break;
        }
    }

    if (i == cfg->max_num_notif) {
       /* here if type is set to a non valid event or more than one event
        * we do not currently support registering for more than one event
        * per notification
        */
        goto err;
    }

    return notif_cfg;
err:
    return NULL;
}

static int
lis2dh12_sensor_set_notification(struct sensor *sensor, sensor_event_type_t event)
{
    struct lis2dh12 *lis2dh12;
    struct lis2dh12_pdd *pdd;
    struct lis2dh12_notif_cfg *notif_cfg;
    int rc;

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);
    pdd = &lis2dh12->pdd;

    notif_cfg = lis2dh12_find_notif_cfg_by_event(event, &lis2dh12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = enable_interrupt(sensor, notif_cfg->int_cfg, notif_cfg->int_num);
    if (rc) {
        goto err;
    }

    pdd->notify_ctx.snec_evtype |= event;

    return 0;
err:
    return rc;
}

static int
lis2dh12_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct lis2dh12 *lis2dh12;

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);

    return lis2dh12_config(lis2dh12, (struct lis2dh12_cfg*)cfg);
}

static int
lis2dh12_sensor_unset_notification(struct sensor *sensor, sensor_event_type_t event)
{
    struct lis2dh12_notif_cfg *notif_cfg;
    struct lis2dh12 *lis2dh12;
    int rc;

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);

    notif_cfg = lis2dh12_find_notif_cfg_by_event(event, &lis2dh12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    lis2dh12->pdd.notify_ctx.snec_evtype &= ~event;

    rc = disable_interrupt(sensor, notif_cfg->int_cfg, notif_cfg->int_num);

err:
    return rc;
}

static void
lis2dh12_inc_notif_stats(sensor_event_type_t event)
{

#if MYNEWT_VAL(LIS2DH12_NOTIF_STATS)
    switch (event) {
        case SENSOR_EVENT_TYPE_SLEEP:
            STATS_INC(g_lis2dh12stats, sleep_notify);
            break;
        case SENSOR_EVENT_TYPE_SINGLE_TAP:
            STATS_INC(g_lis2dh12stats, single_tap_notify);
            break;
        case SENSOR_EVENT_TYPE_DOUBLE_TAP:
            STATS_INC(g_lis2dh12stats, double_tap_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_CHANGE:
            STATS_INC(g_lis2dh12stats, orient_chg_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE:
            STATS_INC(g_lis2dh12stats, orient_chg_x_l_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE:
            STATS_INC(g_lis2dh12stats, orient_chg_x_h_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE:
            STATS_INC(g_lis2dh12stats, orient_chg_y_l_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE:
            STATS_INC(g_lis2dh12stats, orient_chg_y_h_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE:
            STATS_INC(g_lis2dh12stats, orient_chg_z_l_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE:
            STATS_INC(g_lis2dh12stats, orient_chg_z_h_notify);
            break;
        case SENSOR_EVENT_TYPE_SLEEP_CHANGE:
            STATS_INC(g_lis2dh12stats, sleep_chg_notify);
            break;
        case SENSOR_EVENT_TYPE_WAKEUP:
            STATS_INC(g_lis2dh12stats, wakeup_notify);
            break;
        case SENSOR_EVENT_TYPE_FREE_FALL:
            STATS_INC(g_lis2dh12stats, free_fall_notify);
            break;
        default:
            break;
    }
#endif

    return;
}

static int
lis2dh12_notify(struct lis2dh12 *lis2dh12, uint16_t src,
                    sensor_event_type_t event_type)
{
    struct lis2dh12_notif_cfg *notif_cfg;

    notif_cfg = lis2dh12_find_notif_cfg_by_event(event_type, &lis2dh12->cfg);
    if (!notif_cfg) {
        return SYS_EINVAL;
    }

    if (src & notif_cfg->notif_src) {
        sensor_mgr_put_notify_evt(&lis2dh12->pdd.notify_ctx, event_type);
        lis2dh12_inc_notif_stats(event_type);
    }

    return 0;
}

static int
lis2dh12_sensor_handle_interrupt(struct sensor *sensor)
{
    struct lis2dh12 *lis2dh12;
    struct sensor_itf *itf;
    uint8_t int_src_bytes[2];
    uint16_t int_src;
    uint8_t click_src;
    uint8_t int2_pin_state;
    struct lis2dh12_notif_cfg *notif_cfg;
    struct lis2dh12_pdd *pdd;
    int rc;

    lis2dh12 = (struct lis2dh12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dh12->pdd;

    rc = lis2dh12_clear_int1(itf, &int_src_bytes[0]);
    if (rc) {
        LIS2DH12_LOG(ERROR, "Could not read INT1_SRC (err=0x%02x)\n", rc);
        goto err;
    }

    /* Read pin state of interrupt 2 before clearing the int */
    int2_pin_state = hal_gpio_read(itf->si_ints[1].host_pin);

    rc = lis2dh12_clear_int2(itf, &int_src_bytes[1]);
    if (rc) {
        LIS2DH12_LOG(ERROR, "Could not read INT1_SRC (err=0x%02x)\n", rc);
        goto err;
    }

    int_src = (int_src_bytes[1] << 8) | int_src_bytes[0];

#if LIS2DH12_PRINT_INTR
    console_printf("INT_SRC = 0x%02X 0x%02X\n", int_src_bytes[0],
                    int_src_bytes[1]);

    if (int_src) {
        int16_t x, y, z;
        lis2dh12_get_data(itf, 16, &x, &y, &z);
        console_printf("X = %-5d Y = %-5d Z = %-5d\n", x, y, z);
    }
#endif

    if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_CHANGE) {
        rc = lis2dh12_notify(lis2dh12, int_src, SENSOR_EVENT_TYPE_ORIENT_CHANGE);
        if (rc) {
            goto err;
        }
    }

    if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE) {
        rc = lis2dh12_notify(lis2dh12, int_src, SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE);
        if (rc) {
            goto err;
        }
    }

    if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE) {
        rc = lis2dh12_notify(lis2dh12, int_src, SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE);
        if (rc) {
            goto err;
        }
    }

    if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE) {
        rc = lis2dh12_notify(lis2dh12, int_src, SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE);
        if (rc) {
            goto err;
        }
    }

    if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE) {
        rc = lis2dh12_notify(lis2dh12, int_src, SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE);
        if (rc) {
            goto err;
        }
    }

    if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE) {
        rc = lis2dh12_notify(lis2dh12, int_src, SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE);
        if (rc) {
            goto err;
        }
    }

    if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE) {
        rc = lis2dh12_notify(lis2dh12, int_src, SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE);
        if (rc) {
            goto err;
        }
    }

    /* Check is single/double detection was requested, if not interrupt
     * did not happened and there is no need to query device about it
     */
    if ((pdd->notify_ctx.snec_evtype &
            (SENSOR_EVENT_TYPE_DOUBLE_TAP | SENSOR_EVENT_TYPE_DOUBLE_TAP)) != 0) {
        /* Read click interrupt state from device */
        rc = lis2dh12_clear_click(itf, &click_src);
        if (rc) {
            LIS2DH12_LOG(ERROR, "Could not read int src err=0x%02x\n", rc);
            return rc;
        }

        if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_SINGLE_TAP) &&
                (click_src & LIS2DH12_CLICK_SRC_SCLICK)) {
            rc = lis2dh12_notify(lis2dh12, click_src, SENSOR_EVENT_TYPE_SINGLE_TAP);
            if (rc) {
                goto err;
            }
            return 0;
        }

        if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_DOUBLE_TAP) &&
                (click_src & LIS2DH12_CLICK_SRC_DCLICK)) {
            rc = lis2dh12_notify(lis2dh12, click_src, SENSOR_EVENT_TYPE_DOUBLE_TAP);
            if (rc) {
                goto err;
            }
            return 0;
        }
    }

    /* Free fall */
    notif_cfg = lis2dh12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_FREE_FALL,
                                                 &lis2dh12->cfg);
    if (NULL != notif_cfg &&
        0 != (int_src & notif_cfg->notif_src)) {
        /* Free-fall is detected */
        sensor_mgr_put_notify_evt(&lis2dh12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_FREE_FALL);
        lis2dh12_inc_notif_stats(SENSOR_EVENT_TYPE_FREE_FALL);
    }

    /* Sleep and wake up */

    /* Sleep/wake up notifications on */
    if (pdd->notify_ctx.snec_evtype &
        (SENSOR_EVENT_TYPE_SLEEP | SENSOR_EVENT_TYPE_SLEEP_CHANGE | SENSOR_EVENT_TYPE_WAKEUP)) {
        /*
         * Sleep state can be routed to INT2 pin, pin is active when device stays active
         * Notification will be sent only on activity pin state change.
         */
        if (int2_pin_state != lis2dh12->pdd.int2_pin_state) {
            lis2dh12->pdd.int2_pin_state = int2_pin_state;

            if ((!int2_pin_state) &&
                0 != (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_WAKEUP)) {
                /* Just become active */
                notif_cfg = lis2dh12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_WAKEUP,
                                                             &lis2dh12->cfg);
                sensor_mgr_put_notify_evt(&lis2dh12->pdd.notify_ctx,
                                          SENSOR_EVENT_TYPE_WAKEUP);
                lis2dh12_inc_notif_stats(SENSOR_EVENT_TYPE_WAKEUP);
            } else if (int2_pin_state &&
                       0 != (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_SLEEP)) {
                /* Just went to sleep */
                notif_cfg = lis2dh12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_SLEEP,
                                                             &lis2dh12->cfg);
                sensor_mgr_put_notify_evt(&lis2dh12->pdd.notify_ctx,
                                           SENSOR_EVENT_TYPE_SLEEP);
                lis2dh12_inc_notif_stats(SENSOR_EVENT_TYPE_SLEEP);
            } else{
                notif_cfg = lis2dh12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_SLEEP_CHANGE,
                                                             &lis2dh12->cfg);
                /* Sleep change interrupt must be configured for int2 */
                if ((notif_cfg && int2_pin_state && itf->si_ints[1].active == HAL_GPIO_TRIG_RISING) ||
                        (notif_cfg && !int2_pin_state && itf->si_ints[1].active == HAL_GPIO_TRIG_FALLING) ||
                        (notif_cfg && itf->si_ints[1].active == HAL_GPIO_TRIG_BOTH))
                {
                    /* Sleep change detected, either wake-up or sleep */
                    sensor_mgr_put_notify_evt(&lis2dh12->pdd.notify_ctx,
                                              SENSOR_EVENT_TYPE_SLEEP_CHANGE);
                    lis2dh12_inc_notif_stats(SENSOR_EVENT_TYPE_SLEEP_CHANGE);
                }
            }
        }
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
 * Set reference threshold
 *
 * @param the sensor interface
 * @param threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_ref_thresh(struct sensor_itf *itf, uint8_t ths)
{
    int rc;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_REFERENCE, &ths, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Set interrupt threshold for int 2
 *
 * @param the sensor interface
 * @param threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_thresh(struct sensor_itf *itf, uint8_t ths)
{
    int rc;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_INT2_THS, &ths, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Set interrupt threshold for int 1
 *
 * @param the sensor interface
 * @param threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_thresh(struct sensor_itf *itf, uint8_t ths)
{

    int rc;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_INT1_THS, &ths, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Clear interrupt 2
 *
 * @param the sensor interface
 */
int
lis2dh12_clear_int2(struct sensor_itf *itf, uint8_t *src)
{
    return lis2dh12_readlen(itf, LIS2DH12_REG_INT2_SRC, src, 1);
}

/**
 * Clear interrupt 1
 *
 * @param the sensor interface
 */
int
lis2dh12_clear_int1(struct sensor_itf *itf, uint8_t *src)
{
    return lis2dh12_readlen(itf, LIS2DH12_REG_INT1_SRC, src, 1);
}

/**
 * Enable interrupt 2
 *
 * @param the sensor interface
 * @param events to enable int for
 */
int
lis2dh12_enable_int2(struct sensor_itf *itf, uint8_t reg)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_INT2_CFG, reg);
}

/**
 * Latch interrupt 1
 *
 * @param the sensor interface
 */
int
lis2dh12_latch_int1(struct sensor_itf *itf)
{
    uint8_t reg;
    int rc;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG5, &reg, 1);
    if (rc) {
        goto err;
    }

    reg |= LIS2DH12_CTRL_REG5_LIR_INT1;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG5, &reg, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Latch interrupt 2
 *
 * @param the sensor interface
 */
int
lis2dh12_latch_int2(struct sensor_itf *itf)
{
    uint8_t reg;
    int rc;

    rc = lis2dh12_readlen(itf, LIS2DH12_REG_CTRL_REG5, &reg, 1);
    if (rc) {
        goto err;
    }

    reg |= LIS2DH12_CTRL_REG5_LIR_INT2;

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG5, &reg, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Set interrupt pin configuration for interrupt 1
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg)
{
    uint8_t reg;

    reg = ~0x08 & cfg;

    return lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG3, &reg, 1);
}

/**
 * Set interrupt 1 duration
 *
 * @param duration in N/ODR units
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_duration(struct sensor_itf *itf, uint8_t dur)
{
    return lis2dh12_writelen(itf, LIS2DH12_REG_INT1_DURATION, &dur, 1);
}

/**
 * Set interrupt 2 duration
 *
 * @param duration in N/ODR units
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_duration(struct sensor_itf *itf, uint8_t dur)
{
    return lis2dh12_writelen(itf, LIS2DH12_REG_INT2_DURATION, &dur, 1);
}

/**
 * Set interrupt pin configuration for interrupt 2
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg)
{
    return lis2dh12_writelen(itf, LIS2DH12_REG_CTRL_REG6, &cfg, 1);
}

/**
 * Disable interrupt 1
 *
 * @param the sensor interface
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_disable_int1(struct sensor_itf *itf)
{
    uint8_t reg;
    int rc;

    rc = lis2dh12_clear_int1(itf, &reg);
    if (rc) {
        goto err;
    }

    reg = 0;

    os_time_delay((OS_TICKS_PER_SEC * 20)/1000 + 1);

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_INT1_CFG, &reg, 1);

err:
    return rc;
}

/**
 * Disable interrupt 2
 *
 * @param the sensor interface
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_disable_int2(struct sensor_itf *itf)
{
    uint8_t reg;
    int rc;

    rc = lis2dh12_clear_int2(itf, &reg);
    if (rc) {
        goto err;
    }

    reg = 0;

    os_time_delay((OS_TICKS_PER_SEC * 20)/1000 + 1);

    rc = lis2dh12_writelen(itf, LIS2DH12_REG_INT2_CFG, &reg, 1);

err:
    return rc;
}

/**
 * Enable interrupt 1
 *
 * @param the sensor interface
 * @param events to enable int for
 */
int
lis2dh12_enable_int1(struct sensor_itf *itf, uint8_t reg)
{
    return lis2dh12_write8(itf, LIS2DH12_REG_INT1_CFG, reg);
}

/**
 * Clear the low threshold values and disable interrupt
 *
 * @param ptr to sensor
 * @param the Sensor type
 * @param Sensor type traits
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dh12_sensor_clear_low_thresh(struct sensor *sensor,
                                 sensor_type_t type)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(sensor);

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dh12_disable_int1(itf);
    if (rc) {
        goto err;
    }

    hal_gpio_irq_release(itf->si_low_pin);

    return 0;
err:
    return rc;
}

/**
 * Clear the high threshold values and disable interrupt
 *
 * @param ptr to sensor
 * @param the Sensor type
 * @param Sensor type traits
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dh12_sensor_clear_high_thresh(struct sensor *sensor,
                                  sensor_type_t type)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(sensor);

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dh12_disable_int2(itf);
    if (rc) {
        goto err;
    }

    hal_gpio_irq_release(itf->si_high_pin);

    return 0;
err:
    return rc;
}

static int
lis2dh12_set_low_thresh(struct sensor_itf *itf,
                        sensor_data_t low_thresh,
                        uint8_t fs,
                        struct sensor_type_traits *stt)
{
    int16_t acc_mg;
    uint8_t reg = 0xFF;
    uint8_t int_src;
    int rc;

    rc = 0;
    if (low_thresh.sad->sad_x_is_valid ||
        low_thresh.sad->sad_y_is_valid ||
        low_thresh.sad->sad_z_is_valid) {

        if (low_thresh.sad->sad_x_is_valid) {
            lis2dh12_calc_acc_mg(low_thresh.sad->sad_x, &acc_mg);
            reg = acc_mg/fs;
        }

        if (low_thresh.sad->sad_y_is_valid) {
            lis2dh12_calc_acc_mg(low_thresh.sad->sad_y, &acc_mg);
            if (reg > acc_mg/fs) {
                reg = acc_mg/fs;
            }
        }

        if (low_thresh.sad->sad_z_is_valid) {
            lis2dh12_calc_acc_mg(low_thresh.sad->sad_z, &acc_mg);
            if (reg > acc_mg/fs) {
                reg = acc_mg/fs;
            }
        }

        rc = lis2dh12_set_int1_thresh(itf, reg);
        if (rc) {
            goto err;
        }

        reg = LIS2DH12_CTRL_REG3_I1_IA1;

        rc = lis2dh12_set_int1_pin_cfg(itf, reg);
        if (rc) {
            goto err;
        }

        rc = lis2dh12_set_int1_duration(itf, 3);
        if (rc) {
            goto err;
        }

        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        hal_gpio_irq_release(itf->si_low_pin);

        rc = hal_gpio_irq_init(itf->si_low_pin, lis2dh12_int_irq_handler, stt,
                               HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_NONE);
        if (rc) {
            goto err;
        }

        reg  = low_thresh.sad->sad_x_is_valid ? LIS2DH12_INT2_CFG_XLIE : 0;
        reg |= low_thresh.sad->sad_y_is_valid ? LIS2DH12_INT2_CFG_YLIE : 0;
        reg |= low_thresh.sad->sad_z_is_valid ? LIS2DH12_INT2_CFG_ZLIE : 0;

        rc = lis2dh12_clear_int1(itf, &int_src);
        if (rc) {
            goto err;
        }

        os_time_delay((OS_TICKS_PER_SEC * 20)/1000 + 1);

        hal_gpio_irq_enable(itf->si_low_pin);

        rc = lis2dh12_enable_int1(itf, reg);
        if (rc) {
            goto err;
        }
    }

err:
    hal_gpio_irq_release(itf->si_low_pin);
    return rc;
}

static int
lis2dh12_set_high_thresh(struct sensor_itf *itf,
                         sensor_data_t high_thresh,
                         uint8_t fs,
                         struct sensor_type_traits *stt)
{
    int16_t acc_mg;
    uint8_t reg = 0;
    uint8_t int_src;
    int rc;

    rc = 0;
    if (high_thresh.sad->sad_x_is_valid ||
        high_thresh.sad->sad_y_is_valid ||
        high_thresh.sad->sad_z_is_valid) {

        if (high_thresh.sad->sad_x_is_valid) {
            lis2dh12_calc_acc_mg(high_thresh.sad->sad_x, &acc_mg);
            reg = acc_mg/fs;
        }

        if (high_thresh.sad->sad_y_is_valid) {
            lis2dh12_calc_acc_mg(high_thresh.sad->sad_y, &acc_mg);
            if (reg < acc_mg/fs) {
                reg = acc_mg/fs;
            }
        }

        if (high_thresh.sad->sad_z_is_valid) {
            lis2dh12_calc_acc_mg(high_thresh.sad->sad_z, &acc_mg);
            if (reg < acc_mg/fs) {
                reg = acc_mg/fs;
            }
        }

        rc = lis2dh12_set_int2_thresh(itf, reg);
        if (rc) {
            goto err;
        }

        reg = LIS2DH12_CTRL_REG6_I2_IA2;

        rc = lis2dh12_set_int2_pin_cfg(itf, reg);
        if (rc) {
            goto err;
        }

        rc = lis2dh12_set_int2_duration(itf, 3);
        if (rc) {
            goto err;
        }

        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        hal_gpio_irq_release(itf->si_high_pin);

        rc = hal_gpio_irq_init(itf->si_high_pin, lis2dh12_int_irq_handler, stt,
                               HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_NONE);
        if (rc) {
            goto err;
        }

        reg  = high_thresh.sad->sad_x_is_valid ? LIS2DH12_INT2_CFG_XHIE : 0;
        reg |= high_thresh.sad->sad_y_is_valid ? LIS2DH12_INT2_CFG_YHIE : 0;
        reg |= high_thresh.sad->sad_z_is_valid ? LIS2DH12_INT2_CFG_ZHIE : 0;

        rc = lis2dh12_clear_int2(itf, &int_src);
        if (rc) {
            goto err;
        }

        hal_gpio_irq_enable(itf->si_high_pin);

        rc = lis2dh12_enable_int2(itf, reg);
        if (rc) {
            goto err;
        }
    }

err:
    hal_gpio_irq_release(itf->si_high_pin);
    return rc;
}


/**
 * Set the trigger threshold values and enable interrupts
 *
 * @param ptr to sensor
 * @param the Sensor type
 * @param Sensor type traits
 *
 * @return 0 on success, non-zero on failure
 */
static int
lis2dh12_sensor_set_trigger_thresh(struct sensor *sensor,
                                   sensor_type_t type,
                                   struct sensor_type_traits *stt)
{
    int rc;
    uint8_t fs;
    struct sensor_itf *itf;
    sensor_data_t low_thresh;
    sensor_data_t high_thresh;

    itf = SENSOR_GET_ITF(sensor);

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    memcpy(&low_thresh, &stt->stt_low_thresh, sizeof(low_thresh));
    memcpy(&high_thresh, &stt->stt_high_thresh, sizeof(high_thresh));

    rc = lis2dh12_get_full_scale(itf, &fs);
    if (rc) {
        goto err;
    }

    if (fs == LIS2DH12_FS_2G) {
        fs = 16;
    } else if (fs == LIS2DH12_FS_4G) {
        fs = 32;
    } else if (fs == LIS2DH12_FS_8G) {
        fs = 62;
    } else if (fs == LIS2DH12_FS_16G) {
        fs = 186;
    } else {
        rc = SYS_EINVAL;
        goto err;
    }

    /* Set low threshold and enable interrupt */
    rc = lis2dh12_set_low_thresh(itf, low_thresh, fs, stt);
    if (rc) {
        goto err;
    }

    /* Set high threshold and enable interrupt */
    rc = lis2dh12_set_high_thresh(itf, high_thresh, fs, stt);
    if (rc) {
        goto err;
    }

    return 0;

err:
    return rc;
}

/**
 * Set tap detection configuration
 *
 * @param the sensor interface
 * @param the tap settings
 * @return 0 on success, non-zero on failure
 */
int lis2dh12_set_tap_cfg(struct sensor_itf *itf, struct lis2dh12_tap_settings *cfg)
{
    int rc;
    uint8_t reg;

    reg = cfg->en_xs ? LIS2DH12_CLICK_CFG_XS : 0;
    reg |= cfg->en_ys ? LIS2DH12_CLICK_CFG_YS : 0;
    reg |= cfg->en_zs ? LIS2DH12_CLICK_CFG_ZS : 0;
    reg |= cfg->en_xd ? LIS2DH12_CLICK_CFG_XD : 0;
    reg |= cfg->en_yd ? LIS2DH12_CLICK_CFG_YD : 0;
    reg |= cfg->en_zd ? LIS2DH12_CLICK_CFG_ZD : 0;
    rc = lis2dh12_set_click_cfg(itf, reg);
    if (rc) {
        return rc;
    }

    lis2dh12_set_click_threshold(itf, cfg->click_ths);
    if (rc) {
        return rc;
    }

    lis2dh12_set_click_time_limit(itf, cfg->time_limit);
    if (rc) {
        return rc;
    }

    lis2dh12_set_click_time_latency(itf, cfg->time_latency);
    if (rc) {
        return rc;
    }

    lis2dh12_set_click_time_window(itf, cfg->time_window);
    if (rc) {
        return rc;
    }

    return 0;
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
    (void)sensor;

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
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
#endif

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

    rc = lis2dh12_set_int_pp_od(itf, cfg->int_pp_od);
    if (rc) {
        goto err;
    }
    lis2dh12->cfg.int_pp_od = cfg->int_pp_od;

    rc = lis2dh12_pull_up_disc(itf, cfg->lc_pull_up_disc);
    if (rc) {
        goto err;
    }

    lis2dh12->cfg.lc_pull_up_disc = cfg->lc_pull_up_disc;

    rc = lis2dh12_hpf_cfg(itf, (cfg->hp_mode << 6) | (cfg->hp_cut_off << 4) |
                               (cfg->hp_fds << 3) | (cfg->hp_click << 2) |
                               (cfg->hp_ia2 << 1) | cfg->hp_ia1);
    if (rc) {
        goto err;
    }

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

    /*sets xen yen and xen */
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

    rc = lis2dh12_set_op_mode(itf, cfg->power_mode);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_set_fifo_mode(itf, cfg->fifo_mode);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_set_ref_thresh(itf, cfg->reference);
    if (rc)
    {
        goto err;
    }

    if (cfg->int_cfg[0].ths) {
        rc = lis2dh12_set_int1_thresh(itf, cfg->int_cfg[0].ths);
        if (rc) {
            goto err;
        }
        rc = lis2dh12_enable_int1(itf, cfg->int_cfg[0].cfg);
        if (rc) {
            goto err;
        }
        rc = lis2dh12_set_int1_duration(itf, cfg->int_cfg[0].dur);
        if (rc) {
            goto err;
        }
        lis2dh12->cfg.int_cfg[0] = cfg->int_cfg[0];
    }

    if (cfg->int_cfg[1].ths) {
        rc = lis2dh12_set_int2_thresh(itf, cfg->int_cfg[1].ths);
        if (rc) {
            goto err;
        }
        rc = lis2dh12_enable_int2(itf, cfg->int_cfg[1].cfg);
        if (rc) {
            goto err;
        }
        rc = lis2dh12_set_int2_duration(itf, cfg->int_cfg[1].dur);
        if (rc) {
            goto err;
        }
        lis2dh12->cfg.int_cfg[0] = cfg->int_cfg[0];
    }

    if (cfg->latch_int1) {
        rc = lis2dh12_latch_int1(itf);
    }
    if (rc) {
        goto err;
    }

    if (cfg->latch_int2) {
        rc = lis2dh12_latch_int2(itf);
    }
    if (rc) {
        goto err;
    }

    rc = lis2dh12_set_activity_threshold(itf, cfg->act_ths);
    if (rc) {
        goto err;
    }

    rc = lis2dh12_set_activity_duration(itf, cfg->act_dur);
    if (rc) {
        goto err;
    }

    rc = sensor_set_type_mask(&(lis2dh12->sensor), cfg->lc_s_mask);
    if (rc) {
        goto err;
    }
//    lis2dh12_shell_init();
    lis2dh12->cfg.read_mode.int_cfg = cfg->read_mode.int_cfg;
    lis2dh12->cfg.read_mode.int_num = cfg->read_mode.int_num;
    lis2dh12->cfg.read_mode.mode = cfg->read_mode.mode;

    if (!cfg->notif_cfg) {
        lis2dh12->cfg.notif_cfg = (struct lis2dh12_notif_cfg *)dflt_notif_cfg;
        lis2dh12->cfg.max_num_notif = sizeof(dflt_notif_cfg)/sizeof(dflt_notif_cfg[0]);
    } else {
        lis2dh12->cfg.notif_cfg = cfg->notif_cfg;
        lis2dh12->cfg.max_num_notif = cfg->max_num_notif;
    }

    lis2dh12->cfg.read_mode.int_cfg = cfg->read_mode.int_cfg;
    lis2dh12->cfg.read_mode.int_num = cfg->read_mode.int_num;
    lis2dh12->cfg.read_mode.mode = cfg->read_mode.mode;

    if (!cfg->notif_cfg) {
        lis2dh12->cfg.notif_cfg = (struct lis2dh12_notif_cfg *)dflt_notif_cfg;
        lis2dh12->cfg.max_num_notif = sizeof(dflt_notif_cfg)/sizeof(dflt_notif_cfg[0]);
    } else {
        lis2dh12->cfg.notif_cfg = cfg->notif_cfg;
        lis2dh12->cfg.max_num_notif = cfg->max_num_notif;
    }

    lis2dh12->cfg.lc_s_mask = cfg->lc_s_mask;

    rc = lis2dh12_set_tap_cfg(itf, &cfg->tap);
    if (rc) {
        goto err;
    }
    lis2dh12->cfg.tap = cfg->tap;

    return 0;
err:

    return rc;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    struct sensor_itf *itf = arg;

    lis2dh12_init((struct os_dev *)bnode, itf);
}

int
lis2dh12_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                               const struct bus_i2c_node_cfg *i2c_cfg,
                               struct sensor_itf *sensor_itf)
{
    struct lis2dh12 *dev = (struct lis2dh12 *)node;
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
lis2dh12_create_spi_sensor_dev(struct bus_spi_node *node, const char *name,
                               const struct bus_spi_node_cfg *spi_cfg,
                               struct sensor_itf *sensor_itf)
{
    struct lis2dh12 *dev = (struct lis2dh12 *)node;
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
#endif
