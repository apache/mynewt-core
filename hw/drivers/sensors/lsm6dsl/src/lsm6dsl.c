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

/**
 * Driver for 6 axis IMU LSM6DSL
 * For more details please refers to www.st.com AN5040
 */
#include <assert.h>
#include <string.h>

#include <os/mynewt.h>
#include <hal/hal_gpio.h>
#include <modlog/modlog.h>
#include <stats/stats.h>
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/i2c_common.h>
#include <bus/drivers/spi_common.h>
#else /* BUS_DRIVER_PRESENT */
#include <hal/hal_spi.h>
#include <hal/hal_i2c.h>
#include <i2cn/i2cn.h>
#endif /* BUS_DRIVER_PRESENT */
#include <sensor/sensor.h>
#include <sensor/accel.h>
#include <sensor/gyro.h>
#include <sensor/temperature.h>
#include <lsm6dsl/lsm6dsl.h>
#include "lsm6dsl_priv.h"

/* Default event notification */
static const struct lsm6dsl_notif_cfg default_notif_cfg[] = {
    {
        .event = SENSOR_EVENT_TYPE_TILT_POS,
        .int_num = 0,
        .int_mask = LSM6DSL_A_WRIST_TILT_ZPOS_MASK,
        .int_en = LSM6DSL_INT1_TILT_MASK,
    },
    {
        .event = SENSOR_EVENT_TYPE_TILT_NEG,
        .int_num = 0,
        .int_mask = LSM6DSL_A_WRIST_TILT_ZNEG_MASK,
        .int_en = LSM6DSL_INT1_TILT_MASK,
    },
    {
        .event = SENSOR_EVENT_TYPE_TILT_CHANGE,
        .int_num = 0,
        .int_mask = LSM6DSL_TILT_IA_MASK,
        .int_en = LSM6DSL_INT1_TILT_MASK,
    },
    {
        .event = SENSOR_EVENT_TYPE_SINGLE_TAP,
        .int_num = 0,
        .int_mask = LSM6DSL_SINGLE_TAP_MASK,
        .int_en = LSM6DSL_INT1_SINGLE_TAP_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_DOUBLE_TAP,
        .int_num = 0,
        .int_mask = LSM6DSL_DOUBLE_TAP_MASK,
        .int_en = LSM6DSL_INT1_DOUBLE_TAP_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_FREE_FALL,
        .int_num = 0,
        .int_mask = LSM6DSL_FF_IA_MASK,
        .int_en = LSM6DSL_INT1_FF_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_WAKEUP,
        .int_num = 0,
        .int_mask = LSM6DSL_WU_IA_MASK,
        .int_en = LSM6DSL_INT1_WU_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_SLEEP,
        .int_num = 0,
        .int_mask = LSM6DSL_SLEEP_STATE_IA_MASK,
        .int_en = LSM6DSL_INT1_INACT_STATE_MASK
    },
    {
        .event = SENSOR_EVENT_TYPE_ORIENT_CHANGE,
        .int_num = 0,
        .int_mask = LSM6DSL_D6D_IA_MASK,
        .int_en = LSM6DSL_INT1_6D_MASK
    }
};

/* Define the stats section and records */
STATS_SECT_START(lsm6dsl_stat_section)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(read_errors)
#if MYNEWT_VAL(LSM6DSL_NOTIF_STATS)
    STATS_SECT_ENTRY(single_tap_notify)
    STATS_SECT_ENTRY(double_tap_notify)
    STATS_SECT_ENTRY(free_fall_notify)
    STATS_SECT_ENTRY(rel_tilt_notify)
    STATS_SECT_ENTRY(abs_tilt_pos_notify)
    STATS_SECT_ENTRY(abs_tilt_neg_notify)
    STATS_SECT_ENTRY(sleep_notify)
    STATS_SECT_ENTRY(orientation_notify)
    STATS_SECT_ENTRY(wakeup_notify)
#endif /* LSM6DSL_NOTIF_STATS */
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lsm6dsl_stat_section)
    STATS_NAME(lsm6dsl_stat_section, write_errors)
    STATS_NAME(lsm6dsl_stat_section, read_errors)
#if MYNEWT_VAL(LSM6DSL_NOTIF_STATS)
    STATS_NAME(lsm6dsl_stat_section, single_tap_notify)
    STATS_NAME(lsm6dsl_stat_section, double_tap_notify)
    STATS_NAME(lsm6dsl_stat_section, rel_tilt_notify)
    STATS_NAME(lsm6dsl_stat_section, abs_tilt_pos_notify)
    STATS_NAME(lsm6dsl_stat_section, abs_tilt_neg_notify)
    STATS_NAME(lsm6dsl_stat_section, free_fall_notify)
    STATS_NAME(lsm6dsl_stat_section, sleep_notify)
    STATS_NAME(lsm6dsl_stat_section, orientation_notify)
    STATS_NAME(lsm6dsl_stat_section, wakeup_notify)
#endif /* LSM6DSL_NOTIFY_STATS */
STATS_NAME_END(lsm6dsl_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lsm6dsl_stat_section) g_lsm6dsl_stats;

static int lsm6dsl_sensor_read(struct sensor *sensor, sensor_type_t type,
                               sensor_data_func_t data_func, void *data_arg,
                               uint32_t timeout);
static int lsm6dsl_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                                     struct sensor_cfg *cfg);
static int lsm6dsl_sensor_set_notification(struct sensor *sensor,
                                           sensor_event_type_t event);
static int lsm6dsl_sensor_unset_notification(struct sensor *sensor,
                                             sensor_event_type_t event);
static int lsm6dsl_sensor_handle_interrupt(struct sensor *sensor);
static int lsm6dsl_sensor_set_config(struct sensor *sensor, void *cfg);
static int lsm6dsl_sensor_reset(struct sensor *sensor);

static const struct sensor_driver g_lsm6dsl_sensor_driver = {
    .sd_read               = lsm6dsl_sensor_read,
    .sd_get_config         = lsm6dsl_sensor_get_config,
    .sd_set_config         = lsm6dsl_sensor_set_config,
    .sd_set_notification   = lsm6dsl_sensor_set_notification,
    .sd_unset_notification = lsm6dsl_sensor_unset_notification,
    .sd_handle_interrupt   = lsm6dsl_sensor_handle_interrupt,
    .sd_reset              = lsm6dsl_sensor_reset,
};

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Read number of data byte from LSM6DSL sensor over I2C
 *
 * @param lsm6dsl The device
 * @param reg    register address
 * @param buffer data buffer
 * @param len    number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_i2c_read(struct lsm6dsl *lsm6dsl, uint8_t reg,
                 uint8_t *buffer, uint8_t len)
{
    int rc;
    struct hal_i2c_master_data data_struct = {
        .address = lsm6dsl->sensor.s_itf.si_addr,
        .len = 1,
        .buffer = &reg
    };

    /* First byte is register address */
    rc = i2cn_master_write(lsm6dsl->sensor.s_itf.si_num,
                           &data_struct, MYNEWT_VAL(LSM6DSL_I2C_TIMEOUT_TICKS),
                           1,
                           MYNEWT_VAL(LSM6DSL_I2C_RETRIES));
    if (rc) {
        LSM6DSL_LOG_ERROR("I2C access failed at address 0x%02X\n",
                     data_struct.address);
        STATS_INC(g_lsm6dsl_stats, read_errors);
        goto end;
    }

    data_struct.buffer = buffer;
    data_struct.len = len;

    /* Read data from register(s) */
    rc = i2cn_master_read(lsm6dsl->sensor.s_itf.si_num, &data_struct,
                          MYNEWT_VAL(LSM6DSL_I2C_TIMEOUT_TICKS), len,
                          MYNEWT_VAL(LSM6DSL_I2C_RETRIES));
    if (rc) {
        LSM6DSL_LOG_ERROR("Failed to read from 0x%02X:0x%02X\n",
                     data_struct.address, reg);
        STATS_INC(g_lsm6dsl_stats, read_errors);
    }

end:

    return rc;
}

/**
 * Read number of data bytes from LSM6DSL sensor over SPI
 *
 * @param lsm6dsl The device
 * @param reg    register address
 * @param buffer buffer for register data
 * @param len    number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_spi_read(struct lsm6dsl *lsm6dsl, uint8_t reg,
                 uint8_t *buffer, uint8_t len)
{
    int i;
    uint16_t retval;
    int rc = 0;

    /* Select the device */
    hal_gpio_write(lsm6dsl->sensor.s_itf.si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(lsm6dsl->sensor.s_itf.si_num, LSM6DSL_SPI_READ_CMD_BIT(reg));
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        LSM6DSL_LOG_ERROR("SPI_%u register write failed addr:0x%02X\n",
                          lsm6dsl->sensor.s_itf.si_num, reg);
        STATS_INC(g_lsm6dsl_stats, read_errors);
        goto end;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(lsm6dsl->sensor.s_itf.si_num, 0xFF);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            LSM6DSL_LOG_ERROR("SPI_%u read failed addr:0x%02X\n",
                              lsm6dsl->sensor.s_itf.si_num, reg);
            STATS_INC(g_lsm6dsl_stats, read_errors);
            goto end;
        }
        buffer[i] = retval;
    }

end:
    /* De-select the device */
    hal_gpio_write(lsm6dsl->sensor.s_itf.si_cs_pin, 1);

    return rc;
}

/**
 * Write number of bytes to LSM6DSL sensor over I2C
 *
 * @param lsm6dsl The device
 * @param reg    register address
 * @param buffer data buffer
 * @param len    number of bytes to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_i2c_write(struct lsm6dsl *lsm6dsl, uint8_t reg,
                  const uint8_t *buffer, uint8_t len)
{
    int rc;
    uint8_t payload[20] = { reg };
    struct hal_i2c_master_data data_struct = {
        .address = lsm6dsl->sensor.s_itf.si_addr,
        .len = len + 1,
        .buffer = (uint8_t *)payload
    };

    /* Max tx payload can be sizeof(payload) less register address */
    if (len >= sizeof(payload)) {
        return OS_EINVAL;
    }

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = i2cn_master_write(lsm6dsl->sensor.s_itf.si_num, &data_struct,
                           MYNEWT_VAL(LSM6DSL_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(LSM6DSL_I2C_RETRIES));
    if (rc) {
        LSM6DSL_LOG_ERROR("I2C access failed at address 0x%02X\n",
                     data_struct.address);
        STATS_INC(g_lsm6dsl_stats, write_errors);
    }

    return rc;
}

/**
 * Write number of bytes to LSM6DSL sensor over SPI
 *
 * @param lsm6dsl The device
 * @param reg    register address
 * @param buffer data buffer
 * @param len    number of bytes to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_spi_write(struct lsm6dsl *lsm6dsl, uint8_t reg,
                  const uint8_t *buffer, uint8_t len)
{
    int i;
    int rc = 0;

    /* Select the device */
    hal_gpio_write(lsm6dsl->sensor.s_itf.si_cs_pin, 0);

    /* Send the address */
    rc = hal_spi_tx_val(lsm6dsl->sensor.s_itf.si_num, reg);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        LSM6DSL_LOG_ERROR("SPI_%u register write failed addr:0x%02X\n",
                          lsm6dsl->sensor.s_itf.si_num, reg);
        STATS_INC(g_lsm6dsl_stats, write_errors);
        goto end;
    }

    for (i = 0; i < len; i++) {
        /* Read register data */
        rc = hal_spi_tx_val(lsm6dsl->sensor.s_itf.si_num, buffer[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            LSM6DSL_LOG_ERROR("SPI_%u write failed addr:0x%02X\n",
                              lsm6dsl->sensor.s_itf.si_num, reg);
            STATS_INC(g_lsm6dsl_stats, write_errors);
            goto end;
        }
    }

end:
    /* De-select the device */
    hal_gpio_write(lsm6dsl->sensor.s_itf.si_cs_pin, 1);

    return rc;
}
#endif /* BUS_DRIVER_PRESENT */

int
lsm6dsl_write(struct lsm6dsl *lsm6dsl, uint8_t reg,
              const uint8_t *buffer, uint8_t len)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    uint8_t write_data[len + 1];

    write_data[0] = reg;
    memcpy(write_data + 1, buffer, len);

    rc = bus_node_simple_write((struct os_dev *)lsm6dsl, &write_data, len + 1);
#else /* BUS_DRIVER_PRESENT */
    rc = sensor_itf_lock(&lsm6dsl->sensor.s_itf, MYNEWT_VAL(LSM6DSL_ITF_LOCK_TMO));
    if (rc) {
        goto end;
    }

    if (lsm6dsl->sensor.s_itf.si_type == SENSOR_ITF_I2C) {
        rc = lsm6dsl_i2c_write(lsm6dsl, reg, buffer, len);
    } else {
        rc = lsm6dsl_spi_write(lsm6dsl, reg, buffer, len);
    }

    sensor_itf_unlock(&lsm6dsl->sensor.s_itf);
end:
#endif /* BUS_DRIVER_PRESENT */

    return rc;
}

int
lsm6dsl_write_reg(struct lsm6dsl *lsm6dsl, uint8_t reg, uint8_t val)
{
    uint8_t *shadow_reg = NULL;
    bool write_through = true;
    int rc = 0;

    if (reg >= LSM6DSL_FUNC_CFG_ACCESS_REG && reg <= LSM6DSL_D6D_SRC_REG) {
        shadow_reg = &lsm6dsl->cfg_regs1.regs[reg - LSM6DSL_FUNC_CFG_ACCESS_REG];
    } else if (reg >= LSM6DSL_TAP_CFG_REG && reg <= LSM6DSL_MD2_CFG_REG) {
        shadow_reg = &lsm6dsl->cfg_regs2.regs[reg - LSM6DSL_TAP_CFG_REG];
    }

    if (shadow_reg) {
        write_through = *shadow_reg != val;
        *shadow_reg = val;
    }
    if (write_through) {
        rc = lsm6dsl_write(lsm6dsl, reg, &val, 1);
    }

    return rc;
}

int
lsm6dsl_read(struct lsm6dsl *lsm6dsl, uint8_t reg,
             uint8_t *buffer, uint8_t len)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

    if (lsm6dsl->node_is_spi) {
        LSM6DSL_SPI_READ_CMD_BIT(reg);
    }

    rc = bus_node_simple_write_read_transact((struct os_dev *)lsm6dsl, &reg, 1,
                                             buffer, len);
#else /* BUS_DRIVER_PRESENT */
    rc = sensor_itf_lock(&lsm6dsl->sensor.s_itf, MYNEWT_VAL(LSM6DSL_ITF_LOCK_TMO));
    if (rc) {
        goto end;
    }

    if (lsm6dsl->sensor.s_itf.si_type == SENSOR_ITF_I2C) {
        rc = lsm6dsl_i2c_read(lsm6dsl, reg, buffer, len);
    } else {
        rc = lsm6dsl_spi_read(lsm6dsl, reg, buffer, len);
    }

    sensor_itf_unlock(&lsm6dsl->sensor.s_itf);
end:
#endif /* BUS_DRIVER_PRESENT */

    return rc;
}

/**
 * Return cached register variable address for give register address
 *
 * @param lsm6dsl the device
 * @param reg     register number
 *
 * @return address of local copy of register or NULL if register is not cached.
 */
static uint8_t *
lsm6dsl_shadow_reg(struct lsm6dsl *lsm6dsl, uint8_t reg)
{
    uint8_t *shadow_reg;

    if (reg >= LSM6DSL_FUNC_CFG_ACCESS_REG && reg <= LSM6DSL_D6D_SRC_REG) {
        shadow_reg = &lsm6dsl->cfg_regs1.regs[reg - LSM6DSL_FUNC_CFG_ACCESS_REG];
    } else if (reg >= LSM6DSL_TAP_CFG_REG && reg <= LSM6DSL_MD2_CFG_REG) {
        shadow_reg = &lsm6dsl->cfg_regs2.regs[reg - LSM6DSL_TAP_CFG_REG];
    } else {
        shadow_reg = NULL;
    }

    return shadow_reg;
}

int
lsm6dsl_read_reg(struct lsm6dsl *lsm6dsl, uint8_t reg, uint8_t *val)
{
    int rc;
    uint8_t *shadow_reg = lsm6dsl_shadow_reg(lsm6dsl, reg);

    if (shadow_reg) {
        *val = *shadow_reg;
        rc = 0;
    } else {
        rc = lsm6dsl_read(lsm6dsl, reg, val, 1);
    }

    return rc;
}

/**
 * Modify register bit field
 *
 * @param lsm6dsl The device
 * @param reg  register address
 * @param mask register mask
 * @param data new bit filed value
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_write_reg_bit_field(struct lsm6dsl *lsm6dsl, uint8_t reg,
                            uint8_t mask, uint8_t data)
{
    int rc;

    uint8_t new_data;
    uint8_t old_data;

    rc = lsm6dsl_read_reg(lsm6dsl, reg, &old_data);
    if (rc) {
        goto end;
    }

    new_data = ((old_data & (~mask)) | LSM6DSL_SHIFT_DATA_MASK(data, mask));

    /* Try to limit bus access if possible */
    if (new_data != old_data) {
        rc = lsm6dsl_write_reg(lsm6dsl, reg, new_data);
    }
end:
    return rc;
}

/**
 * Reset lsm6dsl
 *
 * @param lsm6dsl The device
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_reset(struct lsm6dsl *lsm6dsl)
{
    int rc;

    rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_CTRL3_C_REG,
                           LSM6DSL_SW_RESET_MASK | LSM6DSL_IF_INC_MASK | LSM6DSL_BDU_MASK);

    if (rc == 0) {
        os_time_delay((OS_TICKS_PER_SEC * 10 / 1000) + 1);

        rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_CTRL3_C_REG,
                               LSM6DSL_IF_INC_MASK | LSM6DSL_BDU_MASK);
    }

    return rc;
}

/**
 * Get chip ID
 *
 * @param lsm6dsl The device
 * @param ptr to chip id to be filled up
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_get_chip_id(struct lsm6dsl *lsm6dsl, uint8_t *chip_id)
{
    return lsm6dsl_read(lsm6dsl, LSM6DSL_WHO_AM_I_REG, chip_id, 1);
}

int
lsm6dsl_set_gyro_full_scale(struct lsm6dsl *lsm6dsl, uint8_t fs)
{
    switch (fs) {
    case LSM6DSL_GYRO_FS_125DPS:
        lsm6dsl->gyro_mult = 125.0f / 32768;
        break;
    case LSM6DSL_GYRO_FS_250DPS:
        lsm6dsl->gyro_mult = 250.0f / 32768;
        break;
    case LSM6DSL_GYRO_FS_500DPS:
        lsm6dsl->gyro_mult = 500.0f / 32768;
        break;
    case LSM6DSL_GYRO_FS_1000DPS:
        lsm6dsl->gyro_mult = 1000.0f / 32768;
        break;
    case LSM6DSL_GYRO_FS_2000DPS:
        lsm6dsl->gyro_mult = 2000.0f / 32768;
        break;
    }
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL2_G_REG,
                                       LSM6DSL_FS_G_MASK, fs);
}

int
lsm6dsl_set_acc_full_scale(struct lsm6dsl *lsm6dsl, uint8_t fs)
{
    switch (fs) {
    case LSM6DSL_ACCEL_FS_2G:
        lsm6dsl->acc_mult = 2 * STANDARD_ACCEL_GRAVITY / 32768;
        break;
    case LSM6DSL_ACCEL_FS_4G:
        lsm6dsl->acc_mult = 4 * STANDARD_ACCEL_GRAVITY / 32768;
        break;
    case LSM6DSL_ACCEL_FS_8G:
        lsm6dsl->acc_mult = 8 * STANDARD_ACCEL_GRAVITY / 32768;
        break;
    case LSM6DSL_ACCEL_FS_16G:
        lsm6dsl->acc_mult = 16 * STANDARD_ACCEL_GRAVITY / 32768;
        break;
    }
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL1_XL_REG,
                                       LSM6DSL_FS_XL_MASK, fs);
}

int
lsm6dsl_set_acc_rate(struct lsm6dsl *lsm6dsl, acc_data_rate_t rate)
{
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL1_XL_REG,
                                       LSM6DSL_ODR_XL_MASK, rate);
}

int
lsm6dsl_set_gyro_rate(struct lsm6dsl *lsm6dsl, gyro_data_rate_t rate)
{
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL2_G_REG,
                                       LSM6DSL_ODR_G_MASK, rate);
}

int
lsm6dsl_set_fifo_mode(struct lsm6dsl *lsm6dsl, lsm6dsl_fifo_mode_t mode)
{
    int rc;
    uint8_t fifo_odr = max(lsm6dsl->cfg.acc_rate, lsm6dsl->cfg.gyro_rate);

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_FIFO_CTRL5_REG,
                                     LSM6DSL_ODR_FIFO_MASK, fifo_odr);
    if (rc == 0) {
        rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_FIFO_CTRL5_REG,
                                         LSM6DSL_FIFO_MODE_MASK, mode);
    }
    return rc;
}

int
lsm6dsl_set_fifo_watermark(struct lsm6dsl *lsm6dsl, uint16_t wtm)
{
    int rc = 0;
    uint16_t old_wtm;

    if (wtm > LSM6DSL_MAX_FIFO_DEPTH) {
        return SYS_EINVAL;
    }

    old_wtm = (lsm6dsl->cfg_regs1.fifo_ctrl1 | (lsm6dsl->cfg_regs1.fifo_ctrl2 << 8u)) & (LSM6DSL_MAX_FIFO_DEPTH - 1);
    if (old_wtm != wtm) {
        lsm6dsl->cfg_regs1.fifo_ctrl1 = (uint8_t)wtm;
        lsm6dsl->cfg_regs1.fifo_ctrl2 &= ~LSM6DSL_FTH_8_10_MASK;
        lsm6dsl->cfg_regs1.fifo_ctrl2 |= wtm >> 8;
        rc = lsm6dsl_write(lsm6dsl, LSM6DSL_FIFO_CTRL1_REG,
                           &lsm6dsl->cfg_regs1.fifo_ctrl1, 2);
    }

    return rc;
}

int
lsm6dsl_get_fifo_samples(struct lsm6dsl *lsm6dsl, uint16_t *samples)
{
    uint8_t fifo_status[2];
    int rc;

    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_FIFO_STATUS1_REG,
                      (uint8_t *)&fifo_status, sizeof(fifo_status));
    if (rc) {
        return rc;
    }

    if (fifo_status[1] & LSM6DSL_OVER_RUN_MASK) {
        *samples = 2048;
    } else {
        *samples = fifo_status[0] | ((LSM6DSL_DIFF_FIFO_MASK & fifo_status[1]) << 8u);
    }

    return 0;
}

int
lsm6dsl_get_fifo_pattern(struct lsm6dsl *lsm6dsl, uint16_t *pattern)
{
    uint8_t fifo_status[2];
    int rc;

    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_FIFO_STATUS3_REG,
                      fifo_status, sizeof(fifo_status));
    if (rc) {
        return rc;
    }

    *pattern = (fifo_status[1] << 8u) | fifo_status[0];

    return 0;
}

/**
 * Set block data update
 *
 * @param lsm6dsl The device
 * @param enable bdu
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_set_bdu(struct lsm6dsl *lsm6dsl, bool en)
{
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL3_C_REG,
                                       LSM6DSL_BDU_MASK, en);
}

int
lsm6dsl_set_offsets(struct lsm6dsl *lsm6dsl, int8_t offset_x, int8_t offset_y,
                    int8_t offset_z, user_off_weight_t weight)
{
    int rc;
    int8_t offset[] = { offset_x, offset_y, offset_z };

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL6_C_REG, LSM6DSL_USR_OFF_W_MASK, weight);
    if (rc == 0) {
        rc = lsm6dsl_write(lsm6dsl, LSM6DSL_X_OFS_USR_REG,
                           (uint8_t *)offset, sizeof(offset));
    }

    return rc;
}

int
lsm6dsl_get_offsets(struct lsm6dsl *lsm6dsl, int8_t *offset_x,
                    int8_t *offset_y, int8_t *offset_z, user_off_weight_t *weight)
{
    uint8_t ctrl6_c;
    int8_t offset[3];
    int rc;

    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_X_OFS_USR_REG,
                      (uint8_t *)offset, sizeof(offset));
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_read_reg(lsm6dsl, LSM6DSL_CTRL6_C_REG, &ctrl6_c);
    if (rc) {
        goto end;
    }

    *weight = (ctrl6_c & LSM6DSL_USR_OFF_W_MASK) ? LSM6DSL_USER_WEIGHT_HI : LSM6DSL_USER_WEIGHT_LO;
    *offset_x = offset[0];
    *offset_y = offset[1];
    *offset_z = offset[2];
end:
    return 0;
}

int
lsm6dsl_set_int_pp_od(struct lsm6dsl *lsm6dsl, bool open_drain)
{
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL3_C_REG,
                                       LSM6DSL_PP_OD_MASK, open_drain);
}

int
lsm6dsl_get_int_pp_od(struct lsm6dsl *lsm6dsl, bool *open_drain)
{
    *open_drain = (lsm6dsl->cfg_regs1.ctrl3_c & LSM6DSL_PP_OD_MASK) != 0;

    return 0;
}

int
lsm6dsl_set_latched_int(struct lsm6dsl *lsm6dsl, bool en)
{
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_TAP_CFG_REG,
                                       LSM6DSL_LIR_MASK, en);
}

int
lsm6dsl_get_latched_int(struct lsm6dsl *lsm6dsl, uint8_t *en)
{
    *en = (lsm6dsl->cfg_regs2.tap_cfg & LSM6DSL_LIR_MASK) != 0;

    return 0;
}

int
lsm6dsl_set_map_int2_to_int1(struct lsm6dsl *lsm6dsl, bool en)
{
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL4_C_REG,
                                       LSM6DSL_INT2_ON_INT1_MASK, en);
}

int
lsm6dsl_get_map_int2_to_int1(struct lsm6dsl *lsm6dsl, uint8_t *en)
{
    *en = (lsm6dsl->cfg_regs1.ctrl4_c & LSM6DSL_INT2_ON_INT1_MASK) != 0;

    return 0;
}

int
lsm6dsl_set_int_level(struct lsm6dsl *lsm6dsl, uint8_t level)
{
    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_CTRL3_C_REG,
                                       LSM6DSL_H_LACTIVE_MASK, level ? 0 : 1);
}

int
lsm6dsl_get_int_level(struct lsm6dsl *lsm6dsl, uint8_t *level)
{
    *level = (lsm6dsl->cfg_regs2.tap_cfg & LSM6DSL_H_LACTIVE_MASK) == 0;

    return 0;
}

int
lsm6dsl_clear_int_pin_cfg(struct lsm6dsl *lsm6dsl, uint8_t int_pin,
                          uint8_t int_mask)
{
    uint8_t reg;

    switch (int_pin) {
    case 0:
        reg = LSM6DSL_MD1_CFG_REG;
        break;
    case 1:
        reg = LSM6DSL_MD2_CFG_REG;
        break;
    default:
        LSM6DSL_LOG_ERROR("Invalid int pin %d\n", int_pin);

        return SYS_EINVAL;
    }

    return lsm6dsl_write_reg_bit_field(lsm6dsl, reg, int_mask, LSM6DSL_DIS_BIT);
}

int
lsm6dsl_clear_int(struct lsm6dsl *lsm6dsl, struct int_src_regs *int_src)
{
    int rc;
    /*
     * Interrupt status could have been read in single 4 byte I2C transaction, but
     * if wake_up_src is read first D6D_IA bit from D6D_SRC is cleared
     * and information about orientation change is lost.
     * 3 byte read of FUNC_SRC1 will get FUNC_SRC1, FUNC_SRC2, and WRIST_TILT_IA.
     */
    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_D6D_SRC_REG, (uint8_t *)&int_src->d6d_src, 2);
    if (rc == 0) {
        rc = lsm6dsl_read(lsm6dsl, LSM6DSL_FUNC_SRC1_REG, (uint8_t *)&int_src->func_src1, 3);
    }
    if (rc == 0) {
        rc = lsm6dsl_read(lsm6dsl, LSM6DSL_WAKE_UP_SRC_REG, (uint8_t *)&int_src->wake_up_src, 2);
    }
    return rc;
}

int
lsm6dsl_set_int_pin_cfg(struct lsm6dsl *lsm6dsl, uint8_t int_pin,
                        uint8_t int_mask)
{
    uint8_t reg;

    switch (int_pin) {
    case 0:
        reg = LSM6DSL_MD1_CFG_REG;
        break;
    case 1:
        reg = LSM6DSL_MD2_CFG_REG;
        break;
    default:
        LSM6DSL_LOG_ERROR("Invalid int pin %d\n", int_pin);

        return SYS_EINVAL;
    }

    return lsm6dsl_write_reg_bit_field(lsm6dsl, reg, int_mask, LSM6DSL_EN_BIT);
}

int
lsm6dsl_set_orientation(struct lsm6dsl *lsm6dsl,
                        const struct lsm6dsl_orientation_settings *cfg)
{
    uint8_t val = cfg->en_4d ? 0x4 : 0;
    val |= (uint8_t)cfg->ths_6d;

    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_TAP_THS_6D_REG,
                                       LSM6DSL_D4D_EN_MASK | LSM6DSL_SIXD_THS_MASK, val);
}

int
lsm6dsl_get_orientation_cfg(struct lsm6dsl *lsm6dsl,
                            struct lsm6dsl_orientation_settings *cfg)
{
    cfg->en_4d = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.tap_cfg, LSM6DSL_D4D_EN_MASK);
    cfg->ths_6d = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.tap_cfg, LSM6DSL_SIXD_THS_MASK);

    return 0;
}

int
lsm6dsl_set_tap_cfg(struct lsm6dsl *lsm6dsl,
                    const struct lsm6dsl_tap_settings *cfg)
{
    int rc;
    uint8_t val;

    val = LSM6DSL_SHIFT_DATA_MASK(cfg->tap_ths, LSM6DSL_TAP_THS_MASK);
    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_TAP_THS_6D_REG,
                                     LSM6DSL_TAP_THS_MASK, val);
    if (rc) {
        goto end;
    }

    val = LSM6DSL_SHIFT_DATA_MASK(cfg->dur, LSM6DSL_DUR_MASK) |
          LSM6DSL_SHIFT_DATA_MASK(cfg->quiet, LSM6DSL_QUIET_MASK) |
          LSM6DSL_SHIFT_DATA_MASK(cfg->shock, LSM6DSL_SHOCK_MASK);

    rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_INT_DUR2_REG, val);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_WAKE_UP_THS_REG,
                                     LSM6DSL_SINGLE_DOUBLE_TAP_MASK,
                                     cfg->en_dtap);
    if (rc) {
        goto end;
    }

    val = cfg->en_x ? (LSM6DSL_TAP_X_EN_MASK >> 1) : 0;
    val |= cfg->en_y ? (LSM6DSL_TAP_Y_EN_MASK >> 1) : 0;
    val |= cfg->en_z ? (LSM6DSL_TAP_Z_EN_MASK >> 1) : 0;
    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_TAP_CFG_REG,
                                     LSM6DSL_TAP_XYZ_EN_MASK, val);
end:
    return rc;
}

int
lsm6dsl_get_tap_cfg(struct lsm6dsl *lsm6dsl,
                    struct lsm6dsl_tap_settings *cfg)
{
    cfg->en_x = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.tap_cfg, LSM6DSL_TAP_X_EN_MASK);
    cfg->en_y = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.tap_cfg, LSM6DSL_TAP_Y_EN_MASK);
    cfg->en_z = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.tap_cfg, LSM6DSL_TAP_Z_EN_MASK);

    cfg->tap_ths = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.tap_ths_6d, LSM6DSL_TAP_THS_MASK);

    cfg->dur = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.int_dur2, LSM6DSL_DUR_MASK);
    cfg->quiet = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.int_dur2, LSM6DSL_QUIET_MASK);
    cfg->shock = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.int_dur2, LSM6DSL_SHOCK_MASK);

    cfg->en_dtap = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.wake_up_ths, LSM6DSL_SINGLE_DOUBLE_TAP_MASK);

    return 0;
}

int
lsm6dsl_set_free_fall(struct lsm6dsl *lsm6dsl, struct lsm6dsl_ff_settings *ff)
{
    int rc;
    uint8_t val;

    val = LSM6DSL_SHIFT_DATA_MASK(ff->free_fall_dur, LSM6DSL_FF_DUR_MASK);
    val |= LSM6DSL_SHIFT_DATA_MASK(ff->free_fall_ths, LSM6DSL_FF_THS_MASK);

    rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_FREE_FALL_REG, val);
    if (rc == 0) {
        rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_WAKE_UP_DUR_REG,
                                         LSM6DSL_FF_DUR5_MASK,
                                         ff->free_fall_dur >> 5u);
    }
    return rc;
}

int
lsm6dsl_get_free_fall(struct lsm6dsl *lsm6dsl, struct lsm6dsl_ff_settings *ff)
{
    ff->free_fall_dur = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.free_fall, LSM6DSL_FF_DUR_MASK) |
                        (LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.wake_up_dur, LSM6DSL_FF_DUR5_MASK) << 5);
    ff->free_fall_ths = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.free_fall, LSM6DSL_FF_THS_MASK);

    return 0;
}

int
lsm6dsl_set_wake_up(struct lsm6dsl *lsm6dsl, struct lsm6dsl_wk_settings *wk)
{
    int rc;

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_WAKE_UP_THS_REG,
                                     LSM6DSL_WK_THS_MASK, wk->wake_up_ths);
    if (rc) {
        return rc;
    }

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_WAKE_UP_DUR_REG,
                                     LSM6DSL_WAKE_DUR_MASK, wk->wake_up_dur);
    if (rc) {
        return rc;
    }

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_WAKE_UP_DUR_REG,
                                     LSM6DSL_SLEEP_DUR_MASK, wk->sleep_duration);
    if (rc) {
        return rc;
    }

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_TAP_CFG_REG,
                                     LSM6DSL_INACT_EN_MASK, wk->inactivity);
    if (rc) {
        return rc;
    }

    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_TAP_CFG_REG,
                                       LSM6DSL_SLOPE_FDS_MASK, wk->hpf_slope);
}

int
lsm6dsl_get_wake_up(struct lsm6dsl *lsm6dsl, struct lsm6dsl_wk_settings *wk)
{
    wk->wake_up_ths = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.wake_up_ths, LSM6DSL_WK_THS_MASK);
    wk->wake_up_dur = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.wake_up_dur, LSM6DSL_WAKE_DUR_MASK);
    wk->sleep_duration = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.wake_up_dur, LSM6DSL_SLEEP_DUR_MASK);
    wk->hpf_slope = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.tap_cfg, LSM6DSL_SLOPE_FDS_MASK);
    wk->inactivity = LSM6DSL_DESHIFT_DATA_MASK(lsm6dsl->cfg_regs2.tap_cfg, LSM6DSL_INACT_EN_MASK);

    return 0;
}

int
lsm6dsl_set_tilt(struct lsm6dsl *lsm6dsl,
                 const struct lsm6dsl_tilt_settings *cfg)
{
    int rc;
    uint8_t en_mask;

    en_mask = (cfg->en_rel_tilt * LSM6DSL_TILT_EN_MASK) | (cfg->en_wrist_tilt * LSM6DSL_WRIST_TILT_EN_MASK);

    if (en_mask) {
        rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_CTRL10_C_REG, LSM6DSL_FUNC_EN_MASK | en_mask);
        if (rc) {
            return rc;
        }
        if (cfg->en_wrist_tilt) {
            rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_FUNC_CFG_ACCESS_REG,
                                   LSM6DSL_FUNC_CFG_ACCESS_MASK | LSM6DSL_SHUB_REG_ACCESS_MASK);
            if (rc) {
                return rc;
            }

            rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_A_WRIST_TILT_LAT_REG, cfg->tilt_lat);
            if (rc) {
                goto err;
            }

            rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_A_WRIST_TILT_THS_REG, cfg->tilt_ths);
            if (rc) {
                goto err;
            }

            rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_A_WRIST_TILT_MASK_REG, cfg->tilt_axis_mask);

err:
            rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_FUNC_CFG_ACCESS_REG, 0x00);
        }
    } else {
        rc = lsm6dsl_write_reg(lsm6dsl, LSM6DSL_CTRL10_C_REG, en_mask);
        if (rc) {
            return rc;
        }
    }

    return rc;
}

static void
init_interrupt(struct lsm6dsl_int *interrupt, int pin, int active_level)
{
    os_error_t error;

    /* Init semaphore for task to wait on when irq asleep */
    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints[0].host_pin = pin;
    interrupt->ints[0].active = active_level;
    interrupt->ints[0].device_pin = 0;
}

static void
undo_interrupt(struct lsm6dsl_int *interrupt)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(sr);
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
static int
wait_interrupt(struct lsm6dsl_int *interrupt, uint8_t int_num)
{
    os_sr_t sr;
    bool wait;
    os_error_t error;

    OS_ENTER_CRITICAL(sr);

    /* Check if we did not miss interrupt */
    if (hal_gpio_read(interrupt->ints[int_num].host_pin) ==
                      interrupt->ints[int_num].active) {
        OS_EXIT_CRITICAL(sr);
        return OS_OK;
    }

    if (interrupt->active) {
        interrupt->active = false;
        wait = false;
    } else {
        interrupt->asleep = true;
        wait = true;
    }
    OS_EXIT_CRITICAL(sr);

    if (wait) {
        error = os_sem_pend(&interrupt->wait, LSM6DSL_MAX_INT_WAIT);
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
static void
wake_interrupt(struct lsm6dsl_int *interrupt)
{
    os_sr_t sr;
    os_error_t error;
    bool wake;

    OS_ENTER_CRITICAL(sr);
    if (interrupt->asleep) {
        interrupt->asleep = false;
        wake = true;
    } else {
        interrupt->active = true;
        wake = false;
    }
    OS_EXIT_CRITICAL(sr);

    if (wake) {
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
static void
lsm6dsl_int_irq_handler(void *arg)
{
    struct lsm6dsl *lsm6dsl = arg;

    if (lsm6dsl->pdd.interrupt) {
        wake_interrupt(lsm6dsl->pdd.interrupt);
    }

    sensor_mgr_put_interrupt_evt(&lsm6dsl->sensor);
}

/**
 * Register irq pin handler
 *
 * @param the lsm6dsl device
 *
 * @return 0 on success, non-zero on failure
 */
static int
init_intpin(struct lsm6dsl *lsm6dsl)
{
    hal_gpio_irq_trig_t trig;
    int rc;

    if (lsm6dsl->intr.ints[0].host_pin < 0) {
        LSM6DSL_LOG_ERROR("Interrupt pin not configured\n");

        return SYS_EINVAL;
    }

    if (lsm6dsl->intr.ints[0].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }

    rc = hal_gpio_irq_init(lsm6dsl->intr.ints[0].host_pin, lsm6dsl_int_irq_handler, lsm6dsl,
                           trig, HAL_GPIO_PULL_NONE);
    if (rc) {
        LSM6DSL_LOG_ERROR("Failed to initialise interrupt pin %d\n", lsm6dsl->intr.ints[0].host_pin);
    }

    lsm6dsl_set_int_level(lsm6dsl, lsm6dsl->intr.ints[0].active);

    return rc;
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
static int
lsm6dsl_disable_interrupt(struct sensor *sensor, uint8_t int_mask,
                          uint8_t int_num)
{
    struct lsm6dsl *lsm6dsl;
    struct lsm6dsl_pdd *pdd;

    if (int_mask == 0) {
        return SYS_EINVAL;
    }

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);
    pdd = &lsm6dsl->pdd;

    pdd->int_enable &= ~(int_mask << (int_num * 8));

    /* disable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_disable(lsm6dsl->intr.ints[0].host_pin);
    }

    /* update interrupt setup in device */
    return lsm6dsl_clear_int_pin_cfg(lsm6dsl, int_num, int_mask);
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
static int
lsm6dsl_enable_interrupt(struct sensor *sensor, uint8_t int_mask, uint8_t int_num)
{
    struct lsm6dsl *lsm6dsl;
    struct lsm6dsl_pdd *pdd;
    struct int_src_regs int_src;
    int rc;

    if (!int_mask) {
        return SYS_EINVAL;
    }

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);
    pdd = &lsm6dsl->pdd;

    rc = lsm6dsl_clear_int(lsm6dsl, &int_src);
    if (rc) {
        return rc;
    }

    /* if no interrupts are currently in use disable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_enable(lsm6dsl->intr.ints[0].host_pin);
    }

    /* Save bitmask of enabled events interrupt */
    pdd->int_enable |= (int_mask << (int_num * 8));

    /* enable interrupt in device */
    rc = lsm6dsl_set_int_pin_cfg(lsm6dsl, int_num, int_mask);
    if (rc) {
        lsm6dsl_disable_interrupt(sensor, int_mask, int_num);
    }

    return rc;
}

/**
 * Disable sensor fifo interrupt
 *
 * @param sensor device
 * @param lsm6dsl_cfg configuration
 *
 * @return 0 on success, non-zero on failure
 */
static int
disable_fifo_interrupt(struct sensor *sensor, sensor_type_t type,
                       struct lsm6dsl_cfg *cfg)
{
    struct lsm6dsl *lsm6dsl;
    struct lsm6dsl_pdd *pdd;
    uint8_t reg;
    uint8_t int_pin;
    int rc;

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);
    pdd = &lsm6dsl->pdd;
    int_pin = cfg->read.int_num;

    /* Clear it in interrupt bitmask */
    pdd->int_enable &= ~(LSM6DSL_INT_FIFO_TH_MASK << (int_pin * 8));

    /* The last one closes the door */
    if (!pdd->int_enable) {
        hal_gpio_irq_disable(lsm6dsl->intr.ints[0].host_pin);
    }

    switch (int_pin) {
    case 0:
        reg = LSM6DSL_INT1_CTRL;
        break;
    case 1:
        reg = LSM6DSL_INT2_CTRL;
        break;
    default:
        LSM6DSL_LOG_ERROR("Invalid int pin %d\n", int_pin);
        rc = SYS_EINVAL;
        goto end;
    }

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, reg, LSM6DSL_INT_FIFO_TH_MASK,
                                     LSM6DSL_DIS_BIT);
    if (rc) {
        goto end;
    }

    if (type & SENSOR_TYPE_GYROSCOPE) {
        rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_FIFO_CTRL3_REG,
                                         LSM6DSL_DEC_FIFO_GYRO_MASK, 0);
        if (rc) {
            goto end;
        }
    }

    if (type & SENSOR_TYPE_ACCELEROMETER) {
        rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_FIFO_CTRL3_REG,
                                         LSM6DSL_DEC_FIFO_XL_MASK, 0);
    }

end:
    return rc;
}

static int
lsm6dsl_enable_fifo_interrupt(struct sensor *sensor, sensor_type_t type,
                              struct lsm6dsl_cfg *cfg)
{
    struct lsm6dsl *lsm6dsl;
    struct lsm6dsl_pdd *pdd;
    uint8_t reg;
    uint8_t int_pin;
    int rc;

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);
    pdd = &lsm6dsl->pdd;
    int_pin = cfg->read.int_num;

    /* if no interrupts are currently in use enable int pin */
    if (!pdd->int_enable) {
        hal_gpio_irq_enable(lsm6dsl->intr.ints[0].host_pin);
    }

    switch (int_pin) {
    case 0:
        reg = LSM6DSL_INT1_CTRL;
        break;
    case 1:
        reg = LSM6DSL_INT2_CTRL;
        break;
    default:
        rc = SYS_EINVAL;
        LSM6DSL_LOG_ERROR("Invalid int pin %d\n", int_pin);
        goto err;
    }

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, reg, LSM6DSL_INT_FIFO_TH_MASK,
                                     LSM6DSL_EN_BIT);
    if (rc) {
        goto err;
    }

    /* Update enabled interrupt bitmask */
    pdd->int_enable |= (LSM6DSL_INT_FIFO_TH_MASK << (int_pin * 8));

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_FIFO_CTRL3_REG,
                                     LSM6DSL_DEC_FIFO_GYRO_MASK,
                                     type & SENSOR_TYPE_GYROSCOPE ? 1 : 0);
    if (rc) {
        goto err;
    }

    rc = lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_FIFO_CTRL3_REG,
                                     LSM6DSL_DEC_FIFO_XL_MASK,
                                     type & SENSOR_TYPE_ACCELEROMETER ? 1 : 0);
    if (rc) {
        goto err;
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
static int
enable_embedded_interrupt(struct sensor *sensor, bool en)
{
    struct lsm6dsl *lsm6dsl;

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);

    return lsm6dsl_write_reg_bit_field(lsm6dsl, LSM6DSL_TAP_CFG_REG,
                                       LSM6DSL_INTERRUPTS_ENABLE_MASK, en);
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
 * @param lsm6dsl The device
 * @param pointer to the temperature data structure
 *
 * @return 0 on success, non-zero on error
 */
static int
lsm6dsl_get_temp_data(struct lsm6dsl *lsm6dsl,
                      struct sensor_temp_data *std)
{
    int rc;
    uint8_t temp[2];

    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_OUT_TEMP_L_REG,
                      temp, sizeof(temp));
    if (rc) {
        return rc;
    }

    std->std_temp = (int16_t)((temp[1] << 8) | temp[0]) / 256.0f + 25.0f;
    std->std_temp_is_valid = 1;

    return 0;
}

/**
 * Gets a raw sensor data from the acc/gyro/temp sensor.
 *
 * @param lsm6dsl The device
 * @param Sensor type
 * @param axis data
 *
 * @return 0 on success, non-zero on failure
 */
static inline int
lsm6dsl_get_ag_raw_data(struct lsm6dsl *lsm6dsl,
                        sensor_type_t type, int16_t *data)
{
    int rc;
    uint8_t reg;
    uint8_t payload[6];

    /* Set data out base register for sensor */
    reg = LSM6DSL_GET_OUT_REG(type);
    rc = lsm6dsl_read(lsm6dsl, reg, payload, 6);
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

int
lsm6dsl_run_self_test(struct lsm6dsl *lsm6dsl, int *result)
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
    uint8_t val;

    *result = 0;

    /* Configure xl as for AN5040 */
    rc = lsm6dsl_write(lsm6dsl, LSM6DSL_CTRL1_XL_REG,
                       st_xl_config, sizeof(st_xl_config));
    if (rc) {
        goto end;
    }

    /* Wait 100ms for stable output data */
    os_time_delay(OS_TICKS_PER_SEC / 10);

    /* Read and discard first data sample */
    rc = lsm6dsl_get_ag_raw_data(lsm6dsl, SENSOR_TYPE_ACCELEROMETER, data);
    if (rc) {
        goto end;
    }

    /* Take 5 samples */
    for (i = 0; i < 5; i++) {
        rc = lsm6dsl_get_ag_raw_data(lsm6dsl, SENSOR_TYPE_ACCELEROMETER, data);
        if (rc) {
            goto end;
        }
        diff[0] += data[0];
        diff[1] += data[1];
        diff[2] += data[2];

        /* Wait at least 1/52 s ~ 20 ms */
        os_time_delay(OS_TICKS_PER_SEC / 52);
    }

    /* Enable positive sign self-test mode */
    val = LSM6DSL_XL_SELF_TEST_POS_SIGN;
    rc = lsm6dsl_write(lsm6dsl, LSM6DSL_CTRL5_C_REG, &val, 1);
    if (rc) {
        goto end;
    }

    /* Wait 100ms for stable output data */
    os_time_delay(OS_TICKS_PER_SEC / 10);

    /* Read and discard first data sample */
    rc = lsm6dsl_get_ag_raw_data(lsm6dsl, SENSOR_TYPE_ACCELEROMETER, data);
    if (rc) {
        goto end;
    }

    /* Take 5 samples */
    for (i = 0; i < 5; i++) {
        rc = lsm6dsl_get_ag_raw_data(lsm6dsl, SENSOR_TYPE_ACCELEROMETER, data);
        if (rc) {
            goto end;
        }

        diff[0] -= data[0];
        diff[1] -= data[1];
        diff[2] -= data[2];

        /* Wait at least 1/52 s ~ 20 ms */
        os_time_delay(OS_TICKS_PER_SEC / 52);
    }

    /* Restore register configuration */
    val = 0;
    rc = lsm6dsl_write(lsm6dsl, LSM6DSL_CTRL1_XL_REG, &val, 1);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_write(lsm6dsl, LSM6DSL_CTRL5_C_REG, &val, 1);
    if (rc) {
        goto end;
    }

    /* Compare values to thresholds */
    min = LSM6DSL_XL_ST_MIN * 5 * 2;
    max = LSM6DSL_XL_ST_MAX * 5 * 2;
    for (i = 0; i < 3; i++) {
        /* ABS */
        if (diff[i] < 0)
            diff[i] = -diff[i];

        if ((diff[i] < min) || (diff[i] > max)) {
            *result |= 1;
        }
    }

    /* Configure gyro as for AN5040 */
    rc = lsm6dsl_write(lsm6dsl, LSM6DSL_CTRL1_XL_REG,
                       st_g_config, sizeof(st_g_config));
    if (rc) {
        goto end;
    }

    /* Wait 150ms for stable output data */
    os_time_delay(OS_TICKS_PER_SEC * 150 / 1000);

    /* Read and discard first gyro data sample */
    rc = lsm6dsl_get_ag_raw_data(lsm6dsl, SENSOR_TYPE_GYROSCOPE, data);
    if (rc) {
        goto end;
    }

    memset(diff, 0, sizeof(diff));

    /* Take 5 samples */
    for (i = 0; i < 5; i++) {
        rc = lsm6dsl_get_ag_raw_data(lsm6dsl, SENSOR_TYPE_GYROSCOPE, data);
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
    val = 0x04;
    rc = lsm6dsl_write(lsm6dsl, LSM6DSL_CTRL5_C_REG, &val, 1);
    if (rc) {
        goto end;
    }

    /* Wait 50ms for stable output data */
    os_time_delay(OS_TICKS_PER_SEC * 50 / 1000);

    /* Read and discard first data sample */
    rc = lsm6dsl_get_ag_raw_data(lsm6dsl, SENSOR_TYPE_GYROSCOPE, data);
    if (rc) {
        goto end;
    }

    /* Take 5 samples */
    for (i = 0; i < 5; i++) {
        rc = lsm6dsl_get_ag_raw_data(lsm6dsl, SENSOR_TYPE_GYROSCOPE, data);
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
    rc = lsm6dsl_write(lsm6dsl, LSM6DSL_CTRL1_XL_REG,
                       lsm6dsl_shadow_reg(lsm6dsl, LSM6DSL_CTRL1_XL_REG),
                       sizeof(prev_config));
    if (rc) {
        goto end;
    }

    /* Compare values to thresholds */
    min = LSM6DSL_G_ST_MIN * 5 * 2;
    max = LSM6DSL_G_ST_MAX * 5 * 2;
    for (i = 0; i < 3; i++) {
        /* ABS */
        if (diff[i] < 0)
            diff[i] = -diff[i];
        if ((diff[i] < min) || (diff[i] > max)) {
            *result |= 2;
        }
    }
end:
    return rc;
}

int
lsm6dsl_get_ag_data(struct lsm6dsl *lsm6dsl, sensor_type_t type, void *data)
{
    int rc;
    int16_t x_y_z[3];
    struct sensor_accel_data *sad = data;
    float mult;

    rc = lsm6dsl_get_ag_raw_data(lsm6dsl, type, (void *)x_y_z);
    if (rc) {
        return rc;
    }

    switch (type) {
    case SENSOR_TYPE_GYROSCOPE:
        mult = lsm6dsl->gyro_mult;
        break;
    case SENSOR_TYPE_ACCELEROMETER:
        mult = lsm6dsl->acc_mult;
       break;
    default:
        LSM6DSL_LOG_ERROR("Invalid sensor type: %d\n", (int)type);
        rc = SYS_EINVAL;
        goto end;
    }

    sad->sad_x = (float)x_y_z[0] * mult;
    sad->sad_y = (float)x_y_z[1] * mult;
    sad->sad_z = (float)x_y_z[2] * mult;

    sad->sad_x_is_valid = 1;
    sad->sad_y_is_valid = 1;
    sad->sad_z_is_valid = 1;
end:
    return rc;
}

static int
lsm6dsl_drop_fifo_samples(struct lsm6dsl *lsm6dsl, uint16_t samples_to_drop)
{
    int rc = 0;
    uint8_t sample_buffer[2];

    while (rc == 0 && samples_to_drop != 0) {
        rc = lsm6dsl_read(lsm6dsl, LSM6DSL_FIFO_DATA_OUT_L_REG, sample_buffer, 2);
        --samples_to_drop;
    }

    return rc;
}

static int
lsm6dsl_read_3_samples_from_fifo(struct lsm6dsl *lsm6dsl, uint8_t buf[6])
{
    int rc;

    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_FIFO_DATA_OUT_L_REG, buf, 2);
    if (rc) {
        goto end;
    }
    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_FIFO_DATA_OUT_L_REG, buf + 2, 2);
    if (rc) {
        goto end;
    }
    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_FIFO_DATA_OUT_L_REG, buf + 4, 2);
end:
    return rc;
}

/**
 * Gets a data sample of acc/gyro/temp sensor from FIFO.
 *
 * @param lsm6dsl The device
 * @param axis data
 *
 * @return 0 on success, non-zero on failure
 */
static int
lsm6dsl_read_fifo(struct lsm6dsl *lsm6dsl,
                  sensor_type_t sensor_type,
                  struct sensor_accel_data *sample_data, sensor_type_t *sample_type,
                  uint16_t *pattern)
{
    int rc;
    uint8_t sample_buf[6];
    int16_t x, y, z;
    float mult;

    /*
     * Only gyroscope and accelerometer supported
     * If pattern == 3 - next sample is from accelerometer
     * If pattern == 0 - next sample is from gyro is it is enabled,
     *                   otherwise from accelerometer
     */
    assert(*pattern == 0 || *pattern == 3);

    if (sensor_type & SENSOR_TYPE_GYROSCOPE || *pattern == 0) {
        *sample_type = SENSOR_TYPE_GYROSCOPE;
        mult = lsm6dsl->gyro_mult;
        if (sensor_type & SENSOR_TYPE_ACCELEROMETER) {
            /* Next sample is form accelerometer */
            *pattern = 3;
        }
    } else {
        mult = lsm6dsl->acc_mult;
        *sample_type = SENSOR_TYPE_ACCELEROMETER;
        *pattern = 0;
    }

    rc = lsm6dsl_read_3_samples_from_fifo(lsm6dsl, sample_buf);
    if (rc) {
        return rc;
    }

    /*
     * Both acc and gyro data are represented as 16-bit word in
     * two’s complement
     */
    x = (int16_t)(sample_buf[0] | (sample_buf[1] << 8));
    y = (int16_t)(sample_buf[2] | (sample_buf[3] << 8));
    z = (int16_t)(sample_buf[4] | (sample_buf[5] << 8));


    sample_data->sad_x = (float)x * mult;
    sample_data->sad_y = (float)y * mult;
    sample_data->sad_z = (float)z * mult;

    sample_data->sad_x_is_valid = 1;
    sample_data->sad_y_is_valid = 1;
    sample_data->sad_z_is_valid = 1;

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
int
lsm6dsl_init(struct os_dev *dev, void *arg)
{
    struct lsm6dsl *lsm6dsl;
    struct lsm6dsl_create_dev_cfg *cfg = arg;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto end;
    }

    lsm6dsl = (struct lsm6dsl *)dev;
    lsm6dsl->cfg.lc_s_mask = SENSOR_TYPE_ALL;
    sensor = &lsm6dsl->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lsm6dsl_stats),
        STATS_SIZE_INIT_PARMS(g_lsm6dsl_stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lsm6dsl_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lsm6dsl_stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc) {
        goto end;
    }

    /* Add the IMU driver plus temperature sensor */
    rc = sensor_set_driver(sensor,
                           SENSOR_TYPE_ACCELEROMETER |
                           SENSOR_TYPE_GYROSCOPE |
                           SENSOR_TYPE_TEMPERATURE,
                           (struct sensor_driver *)&g_lsm6dsl_sensor_driver);
    if (rc) {
        goto end;
    }

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    /* Set the interface */
    rc = sensor_set_interface(sensor, &cfg->itf);
    if (rc) {
        goto end;
    }
#endif

    rc = sensor_mgr_register(sensor);
    if (rc) {
        goto end;
    }

    init_interrupt(&lsm6dsl->intr, cfg->int_pin, cfg->int_active_level);

    lsm6dsl->pdd.notify_ctx.snec_sensor = sensor;
    lsm6dsl->pdd.interrupt = NULL;

    rc = init_intpin(lsm6dsl);

end:
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
static int
lsm6dsl_stream_read(struct sensor *sensor,
                    sensor_type_t sensor_type,
                    sensor_data_func_t read_func,
                    void *data_arg,
                    uint32_t time_ms)
{
    struct lsm6dsl_pdd *pdd;
    struct lsm6dsl *lsm6dsl;
    struct lsm6dsl_cfg *cfg;
    os_time_t time_ticks;
    os_time_t stop_ticks = 0;
    uint16_t fifo_samples;
    int rc;
    struct sensor_accel_data sad;
    sensor_type_t r_type;
    /* combined FIFO_STATUS3 and FIFO_STATUS4 */
    uint16_t pattern;

    /* Temperature reading not supported in FIFO */
    if (!(sensor_type & SENSOR_TYPE_ACCELEROMETER) &&
        !(sensor_type & SENSOR_TYPE_GYROSCOPE)) {
        return SYS_EINVAL;
    }

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);
    pdd = &lsm6dsl->pdd;
    cfg = &lsm6dsl->cfg;

    if (cfg->read.mode != LSM6DSL_READ_STREAM) {
        return SYS_EINVAL;
    }

    undo_interrupt(&lsm6dsl->intr);

   if (pdd->interrupt) {
        return SYS_EBUSY;
    }

    /* Enable interrupt */
    pdd->interrupt = &lsm6dsl->intr;

    /* Set FIFO to configuration value */
    rc = lsm6dsl_set_fifo_mode(lsm6dsl, cfg->fifo.mode);
    if (rc) {
        goto err;
    }

    rc = lsm6dsl_enable_fifo_interrupt(sensor, sensor_type, cfg);
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
        rc = wait_interrupt(&lsm6dsl->intr, cfg->read.int_num);
        if (rc) {
            goto err;
        }

        rc = lsm6dsl_get_fifo_samples(lsm6dsl, &fifo_samples);
        if (rc) {
            goto err;
        }

        if (fifo_samples) {
            rc = lsm6dsl_get_fifo_pattern(lsm6dsl, &pattern);
            if (rc) {
                goto err;
            }

            rc = lsm6dsl_drop_fifo_samples(lsm6dsl, (6 - pattern) % 3);
            if (rc) {
                goto err;
            }
        }

        while (fifo_samples >= 3) {
            /* Read all fifo samples */
            rc = lsm6dsl_read_fifo(lsm6dsl, sensor_type, &sad, &r_type, &pattern);
            if (rc) {
                goto err;
            }

            if ((sensor_type & r_type) == r_type) {
                rc = read_func(sensor, data_arg, &sad, r_type);
                if (rc) {
                    goto err;
                }

            }
            fifo_samples -= 3;
        }

        if (time_ms > 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
           break;
        }
    }

err:
    /* Disable FIFO */
    rc |= lsm6dsl_set_fifo_mode(lsm6dsl, LSM6DSL_FIFO_MODE_BYPASS_VAL);

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
static int
lsm6dsl_poll_read(struct sensor *sensor, sensor_type_t type,
                  sensor_data_func_t data_func, void *data_arg,
                  uint32_t timeout)
{
    int rc = 0;
    struct lsm6dsl *lsm6dsl;
    struct sensor_accel_data sad;
    struct sensor_temp_data std;

    /* Check if requested sensor type is supported */
    if (!(type & SENSOR_TYPE_ACCELEROMETER) &&
        !(type & SENSOR_TYPE_GYROSCOPE) &&
        !(type & SENSOR_TYPE_TEMPERATURE)) {
        rc = SYS_EINVAL;
        goto end;
    }

    /* Retrieve configuration from device for sensitivity */
    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);

    if (type & (SENSOR_TYPE_ACCELEROMETER | SENSOR_TYPE_GYROSCOPE)) {

        /* Acc and Gyro data can share the same data structure */
        rc = lsm6dsl_get_ag_data(lsm6dsl, type, &sad);
        if (rc) {
            goto end;
        }

        rc = data_func(sensor, data_arg, &sad, type);
        if (rc) {
            goto end;
        }
    }

    if (type & SENSOR_TYPE_TEMPERATURE) {

        rc = lsm6dsl_get_temp_data(lsm6dsl, &std);
        if (rc) {
            goto end;
        }

        /* Call data function */
        rc = data_func(sensor, data_arg, &std, SENSOR_TYPE_TEMPERATURE);
    }

end:
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
static int
lsm6dsl_sensor_read(struct sensor *sensor, sensor_type_t type,
                    sensor_data_func_t data_func, void *data_arg,
                    uint32_t timeout)
{
    struct lsm6dsl *lsm6dsl;
    struct lsm6dsl_cfg *cfg;

    /* Check if requested sensor type is supported */
    if (!(type & SENSOR_TYPE_ACCELEROMETER) &&
        !(type & SENSOR_TYPE_GYROSCOPE) &&
        !(type & SENSOR_TYPE_TEMPERATURE)) {
        return SYS_EINVAL;
    }

    /* Retrieve configuration from device for sensitivity */
    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);
    cfg = &lsm6dsl->cfg;

    if (cfg->read.mode == LSM6DSL_READ_POLL) {
        return lsm6dsl_poll_read(sensor, type, data_func, data_arg, timeout);
    }

    return lsm6dsl_stream_read(sensor, type, data_func, data_arg, timeout);
}

static struct lsm6dsl_notif_cfg *
lsm6dsl_find_notify_cfg(struct lsm6dsl *lsm6dsl, sensor_event_type_t event)
{
    int i;
    struct lsm6dsl_notif_cfg *notify_cfg = NULL;
    struct lsm6dsl_cfg *cfg = &lsm6dsl->cfg;

    for (i = 0; i < cfg->notify_cfg_count; i++) {
        if (event == cfg->notify_cfg[i].event) {
            notify_cfg = &cfg->notify_cfg[i];
            break;
        }
    }

    return notify_cfg;
}

static int
lsm6dsl_notify(struct lsm6dsl *lsm6dsl, uint8_t src,
               sensor_event_type_t event_type)
{
    struct lsm6dsl_notif_cfg *notify_cfg;

    notify_cfg = lsm6dsl_find_notify_cfg(lsm6dsl, event_type);
    if (!notify_cfg) {
        return SYS_EINVAL;
    }

    if (src & notify_cfg->int_mask) {
        sensor_mgr_put_notify_evt(&lsm6dsl->pdd.notify_ctx, event_type);
        return 0;
    }

    return -1;
}

static void
lsm6dsl_inc_notif_stats(sensor_event_type_t event)
{
#if MYNEWT_VAL(LSM6DSL_NOTIF_STATS)
    switch (event) {
    case SENSOR_EVENT_TYPE_SINGLE_TAP:
        STATS_INC(g_lsm6dsl_stats, single_tap_notify);
        break;
    case SENSOR_EVENT_TYPE_DOUBLE_TAP:
        STATS_INC(g_lsm6dsl_stats, double_tap_notify);
        break;
    case SENSOR_EVENT_TYPE_ORIENT_CHANGE:
        STATS_INC(g_lsm6dsl_stats, orientation_notify);
        break;
    case SENSOR_EVENT_TYPE_SLEEP:
        STATS_INC(g_lsm6dsl_stats, sleep_notify);
        break;
    case SENSOR_EVENT_TYPE_WAKEUP:
        STATS_INC(g_lsm6dsl_stats, wakeup_notify);
        break;
    case SENSOR_EVENT_TYPE_FREE_FALL:
        STATS_INC(g_lsm6dsl_stats, free_fall_notify);
        break;
    case SENSOR_EVENT_TYPE_TILT_CHANGE:
        STATS_INC(g_lsm6dsl_stats, rel_tilt_notify);
        break;
    case SENSOR_EVENT_TYPE_TILT_POS:
        STATS_INC(g_lsm6dsl_stats, abs_tilt_pos_notify);
        break;
    case SENSOR_EVENT_TYPE_TILT_NEG:
        STATS_INC(g_lsm6dsl_stats, abs_tilt_neg_notify);
        break;
    default:
        break;
    }
#endif /* LSM6DSL_NOTIF_STATS */
}

/**
 * Manage events from sensor
 *
 * @param sensor The sensor object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lsm6dsl_sensor_handle_interrupt(struct sensor *sensor)
{
    struct lsm6dsl *lsm6dsl;
    struct int_src_regs int_src;
    int rc;

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);

    rc = lsm6dsl_clear_int(lsm6dsl, &int_src);
    if (rc) {
        LSM6DSL_LOG_ERROR("Could not read int src err=0x%02x\n", rc);
        return rc;
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.tap_src, SENSOR_EVENT_TYPE_SINGLE_TAP);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_SINGLE_TAP);
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.tap_src, SENSOR_EVENT_TYPE_DOUBLE_TAP);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_DOUBLE_TAP);
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.wake_up_src, SENSOR_EVENT_TYPE_FREE_FALL);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_FREE_FALL);
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.wake_up_src, SENSOR_EVENT_TYPE_WAKEUP);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_WAKEUP);
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.wake_up_src, SENSOR_EVENT_TYPE_SLEEP);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_SLEEP);
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.d6d_src, SENSOR_EVENT_TYPE_ORIENT_CHANGE);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_ORIENT_CHANGE);
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.func_src1, SENSOR_EVENT_TYPE_TILT_CHANGE);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_TILT_CHANGE);
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.wrist_tilt_ia, SENSOR_EVENT_TYPE_TILT_POS);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_TILT_POS);
    }

    rc = lsm6dsl_notify(lsm6dsl, int_src.wrist_tilt_ia, SENSOR_EVENT_TYPE_TILT_NEG);
    if (!rc) {
        lsm6dsl_inc_notif_stats(SENSOR_EVENT_TYPE_TILT_NEG);
    }
    return rc;
}

/**
 * Find registered events in available event list
 *
 * @param event   event type to find
 * @param int_cfg interrupt bit configuration
 * @param int_reg interrupt register
 * @param int_num interrupt number
 * @param cfg lsm6dsl_cfg configuration structure
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lsm6dsl_find_int_by_event(sensor_event_type_t event, uint8_t *int_en,
                          uint8_t *int_num, struct lsm6dsl_cfg *cfg)
{
    int i;

    *int_num = 0;
    *int_en = 0;

    if (!cfg) {
        return SYS_EINVAL;
    }

    /* Search in enabled event list */
    for (i = 0; i < cfg->notify_cfg_count; i++) {
        if (event == cfg->notify_cfg[i].event) {
            *int_en = cfg->notify_cfg[i].int_en;
            *int_num = cfg->notify_cfg[i].int_num;
            break;
        }
    }

    if (i == cfg->notify_cfg_count) {
        return SYS_EINVAL;
    }

    return 0;
}

/**
 * Reset sensor
 *
 * @param sensor sensor object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lsm6dsl_sensor_reset(struct sensor *sensor)
{
    struct lsm6dsl *lsm6dsl;

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);

    return lsm6dsl_reset(lsm6dsl);
}

/**
 * Enable notification event
 *
 * @param sensor sensor object
 * @param event  event mask
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lsm6dsl_sensor_set_notification(struct sensor *sensor,
                                sensor_event_type_t event)
{
    struct lsm6dsl *lsm6dsl;
    struct lsm6dsl_pdd *pdd;
    uint8_t int_num;
    uint8_t int_mask;
    int rc;

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);
    pdd = &lsm6dsl->pdd;

    rc = lsm6dsl_find_int_by_event(event, &int_mask, &int_num, &lsm6dsl->cfg);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_enable_interrupt(sensor, int_mask, int_num);
    if (rc) {
        goto end;
    }

    pdd->notify_ctx.snec_evtype |= event;

    if (pdd->notify_ctx.snec_evtype) {
        rc = enable_embedded_interrupt(sensor, 1);
    }

end:
    return rc;
}

/**
 * Disable notification event
 *
 * @param sensor sensor object
 * @param event  event mask
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lsm6dsl_sensor_unset_notification(struct sensor *sensor, sensor_event_type_t event)
{
    struct lsm6dsl *lsm6dsl;
    uint8_t int_num;
    uint8_t int_mask;
    int rc;

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);
    lsm6dsl->pdd.notify_ctx.snec_evtype &= ~event;

    rc = lsm6dsl_find_int_by_event(event, &int_mask, &int_num, &lsm6dsl->cfg);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_disable_interrupt(sensor, int_mask, int_num);
    if (rc) {
        goto end;
    }

    if (lsm6dsl->pdd.notify_ctx.snec_evtype) {
        rc = enable_embedded_interrupt(sensor, 0);
    }

end:
    return rc;
}

/**
 * Sensor driver get_config implementation
 *
 * @param sensor sensor object
 * @param type type of sensor
 * @param cfg  sensor_cfg object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lsm6dsl_sensor_get_config(struct sensor *sensor, sensor_type_t type,
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
 * @param cfg lsm6dsl_cfg object
 *
 * @return 0 on success, non-zero error on failure.
 */
static int
lsm6dsl_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct lsm6dsl *lsm6dsl;

    lsm6dsl = (struct lsm6dsl *)SENSOR_GET_DEVICE(sensor);

    return lsm6dsl_config(lsm6dsl, (struct lsm6dsl_cfg*)cfg);
}

/**
 * Configure the sensor
 *
 * @param lsm6dsl The device
 * @param cfg     device config
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lsm6dsl_config(struct lsm6dsl *lsm6dsl, struct lsm6dsl_cfg *cfg)
{
    int rc;
    uint8_t chip_id;
    struct sensor *sensor;

    sensor = &(lsm6dsl->sensor);

    rc = lsm6dsl_get_chip_id(lsm6dsl, &chip_id);
    if (rc) {
        goto end;
    }

    if (chip_id != LSM6DSL_WHO_AM_I) {
        rc = SYS_EINVAL;
        goto end;
    }

    rc = lsm6dsl_reset(lsm6dsl);
    if (rc) {
        goto end;
    }

    /* Cache all registers */
    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_FUNC_CFG_ACCESS_REG,
                      &lsm6dsl->cfg_regs1.func_cfg_access, sizeof(lsm6dsl->cfg_regs1));
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_read(lsm6dsl, LSM6DSL_TAP_CFG_REG,
                      &lsm6dsl->cfg_regs2.tap_cfg, sizeof(lsm6dsl->cfg_regs2));
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_bdu(lsm6dsl, LSM6DSL_EN_BIT);
    if (rc) {
        goto end;
    }

    lsm6dsl->cfg = *cfg;

    assert(cfg->gyro_fs >= LSM6DSL_GYRO_FS_250DPS && cfg->gyro_fs < LSM6DSL_GYRO_FS_2000DPS);
    assert(cfg->acc_fs >= LSM6DSL_ACCEL_FS_2G && cfg->acc_fs <= LSM6DSL_ACCEL_FS_8G);
    rc = lsm6dsl_set_gyro_full_scale(lsm6dsl, cfg->gyro_fs);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_acc_full_scale(lsm6dsl, cfg->acc_fs);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_gyro_rate(lsm6dsl, cfg->gyro_rate);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_acc_rate(lsm6dsl, cfg->acc_rate);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_offsets(lsm6dsl, 0, 0, 0, LSM6DSL_USER_WEIGHT_LO);
    if (rc) {
        goto end;
    }

    /*
     * Disable FIFO by default
     * Save FIFO default configuration value to be used later on
     */
    rc = lsm6dsl_set_fifo_mode(lsm6dsl, LSM6DSL_FIFO_MODE_BYPASS_VAL);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_fifo_watermark(lsm6dsl, cfg->fifo.wtm);
    if (rc) {
        goto end;
    }

    /* Add embedded gesture configuration */
    rc = lsm6dsl_set_wake_up(lsm6dsl, &cfg->wk);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_free_fall(lsm6dsl, &cfg->ff);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_tap_cfg(lsm6dsl, &cfg->tap);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_orientation(lsm6dsl, &cfg->orientation);
    if (rc) {
        goto end;
    }

    rc = lsm6dsl_set_tilt(lsm6dsl, &cfg->tilt);
    if (rc) {
        goto end;
    }

    lsm6dsl_set_latched_int(lsm6dsl, cfg->latched_int);
    lsm6dsl_set_map_int2_to_int1(lsm6dsl, cfg->map_int2_to_int1);

    if (!cfg->notify_cfg) {
        lsm6dsl->cfg.notify_cfg = (struct lsm6dsl_notif_cfg *)default_notif_cfg;
        lsm6dsl->cfg.notify_cfg_count = ARRAY_SIZE(default_notif_cfg);
    } else {
        lsm6dsl->cfg.notify_cfg = cfg->notify_cfg;
        lsm6dsl->cfg.notify_cfg_count = cfg->notify_cfg_count;
    }

    rc = sensor_set_type_mask(sensor, cfg->lc_s_mask);

end:
    return rc;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    lsm6dsl_init((struct os_dev *)bnode, arg);
}

static int
lsm6dsl_create_i2c_sensor_dev(struct lsm6dsl *lsm6dsl, const char *name,
                              const struct lsm6dsl_create_dev_cfg *cfg)
{
    const struct bus_i2c_node_cfg *i2c_cfg = &cfg->i2c_cfg;
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    lsm6dsl->node_is_spi = false;

    lsm6dsl->sensor.s_itf.si_dev = &lsm6dsl->i2c_node.bnode.odev;
    bus_node_set_callbacks((struct os_dev *)lsm6dsl, &cbs);

    rc = bus_i2c_node_create(name, &lsm6dsl->i2c_node, i2c_cfg, (void *)cfg);

    return rc;
}

static int
lsm6dsl_create_spi_sensor_dev(struct lsm6dsl *lsm6dsl, const char *name,
                              const struct lsm6dsl_create_dev_cfg *cfg)
{
    const struct bus_spi_node_cfg *spi_cfg = &cfg->spi_cfg;
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    lsm6dsl->node_is_spi = true;

    lsm6dsl->sensor.s_itf.si_dev = &lsm6dsl->spi_node.bnode.odev;
    bus_node_set_callbacks((struct os_dev *)lsm6dsl, &cbs);

    rc = bus_spi_node_create(name, &lsm6dsl->spi_node, spi_cfg, (void *)cfg);

    return rc;
}

int
lsm6dsl_create_dev(struct lsm6dsl *lsm6dsl, const char *name,
                   const struct lsm6dsl_create_dev_cfg *cfg)
{
    if (MYNEWT_VAL(LSM6DSL_SPI_SUPPORT) && cfg->node_is_spi) {
        return lsm6dsl_create_spi_sensor_dev(lsm6dsl, name, cfg);
    } else if (MYNEWT_VAL(LSM6DSL_I2C_SUPPORT) && !cfg->node_is_spi) {
        return lsm6dsl_create_i2c_sensor_dev(lsm6dsl, name, cfg);
    }
    return 0;
}

#else
int
lsm6dsl_create_dev(struct lsm6dsl *lsm6dsl, const char *name,
                   const struct lsm6dsl_create_dev_cfg *cfg)
{
    return os_dev_create((struct os_dev *)lsm6dsl, name,
                         OS_DEV_INIT_PRIMARY, 0, lsm6dsl_init, (void *)cfg);
}

#endif /* BUS_DRIVER_PRESENT */
