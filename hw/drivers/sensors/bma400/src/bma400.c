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
#include <string.h>

#include <os/mynewt.h>
#include <hal/hal_gpio.h>
#include <modlog/modlog.h>
#include <stats/stats.h>
#include "defs/error.h"
#include "bma400/bma400.h"
#include "bma400_priv.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/i2c_common.h>
#include <bus/drivers/spi_common.h>
#else /* BUS_DRIVER_PRESENT */
#include <hal/hal_spi.h>
#include <hal/hal_i2c.h>
#include <i2cn/i2cn.h>
#endif /* BUS_DRIVER_PRESENT */

#define BMA400_NOTIFY_MASK  0x01
#define BMA400_READ_MASK    0x02

#define GET_FIELD(reg_val, field_mask) \
        (((reg_val) & (field_mask)) >> __builtin_ctz(field_mask))
#define SET_FIELD(field_val, field_mask)   \
        (((field_val) << __builtin_ctz(field_mask)) & (field_mask))

const uint8_t BMA400_STEP_COUNTER_WRIST_CONFIG[] = {
    1, 45, 123, 212, 68, 1, 59, 122, 219, 123, 63, 108, 205, 39, 25, 150, 160, 195, 14, 12, 60, 240, 0, 247
};

const uint8_t BMA400_STEP_COUNTER_NON_WRIST_CONFIG[] = {
    1, 50, 120, 230, 135, 0, 132, 108, 156, 117, 100, 126, 170, 12, 12, 74, 160, 0, 0, 12, 60, 240, 1, 0
};

static void
delay_msec(uint32_t delay)
{
    delay = (delay * OS_TICKS_PER_SEC) / 1000 + 1;
    os_time_delay(delay);
}

#if MYNEWT_VAL(BMA400_INT_ENABLE)
static void
init_interrupt(struct bma400_int *interrupt)
{
    os_error_t error;

    interrupt->ints[0].host_pin = -1;
    interrupt->ints[1].host_pin = -1;

    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);
}

static void
undo_interrupt(struct bma400_int *interrupt)
{
    (void)os_sem_pend(&interrupt->wait, 0);
}

static void
wait_interrupt(struct bma400_int *interrupt)
{
    os_error_t error;

    error = os_sem_pend(&interrupt->wait, OS_WAIT_FOREVER);
    assert(error == OS_OK);
}

static void
wake_interrupt(struct bma400_int *interrupt)
{
    os_error_t error;

    if (os_sem_get_count(&interrupt->wait) == 0) {
        error = os_sem_release(&interrupt->wait);
        assert(error == OS_OK);
    }
}

static void
bma400_interrupt_handler(void *arg)
{
    struct bma400 *bma400 = arg;

    if (bma400->pdd.interrupt) {
        wake_interrupt(bma400->pdd.interrupt);
    }

    sensor_mgr_put_interrupt_evt(&bma400->sensor);
}
#endif

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Read number of data byte from BMA400 sensor over I2C
 *
 * @param bma400 The device
 * @param reg    register address
 * @param buffer data buffer
 * @param len    number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
bma400_i2c_read(struct bma400 *bma400, uint8_t reg,
                uint8_t *buffer, uint8_t len)
{
    int rc;
    struct hal_i2c_master_data data_struct = {
        .address = bma400->sensor.s_itf.si_addr,
        .len = 1,
        .buffer = &reg
    };

    /* First byte is register address */
    rc = i2cn_master_write(bma400->sensor.s_itf.si_num,
                           &data_struct, MYNEWT_VAL(BMA400_I2C_TIMEOUT_TICKS),
                           1,
                           MYNEWT_VAL(BMA400_I2C_TIMEOUT_TICKS));
    if (rc) {
        BMA400_LOG_ERROR("I2C access failed at address 0x%02X\n",
                         data_struct.address);
        STATS_INC(g_bma400_stats, read_errors);
        goto end;
    }

    data_struct.buffer = buffer;
    data_struct.len = len;

    /* Read data from register(s) */
    rc = i2cn_master_read(bma400->sensor.s_itf.si_num, &data_struct,
                          MYNEWT_VAL(BMA400_I2C_TIMEOUT_TICKS), len,
                          MYNEWT_VAL(BMA400_I2C_RETRIES));
    if (rc) {
        BMA400_LOG_ERROR("Failed to read from 0x%02X:0x%02X\n",
                         data_struct.address, reg);
        STATS_INC(g_bma400_stats, read_errors);
    }

end:

    return rc;
}

/**
 * Read number of data bytes from BMA400 sensor over SPI
 *
 * @param bma400 The device
 * @param reg    register address
 * @param buffer buffer for register data
 * @param len    number of bytes to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
bma400_spi_read(struct bma400 *bma400, uint8_t reg,
                uint8_t *buffer, uint8_t len)
{
    int i;
    uint16_t retval;
    int rc = 0;

    /* Select the device */
    hal_gpio_write(bma400->sensor.s_itf.si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(bma400->sensor.s_itf.si_num, BMA400_SPI_READ_CMD_BIT(reg));
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        BMA400_LOG_ERROR("SPI_%u register write failed addr:0x%02X\n",
                         bma400->sensor.s_itf.si_num, reg);
        STATS_INC(g_bma400_stats, read_errors);
        goto end;
    }
    /* Dummy byte */
    retval = hal_spi_tx_val(bma400->sensor.s_itf.si_num, 0xFF);

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(bma400->sensor.s_itf.si_num, 0xFF);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            BMA400_LOG_ERROR("SPI_%u read failed addr:0x%02X\n",
                             bma400->sensor.s_itf.si_num, reg);
            STATS_INC(g_bma400_stats, read_errors);
            goto end;
        }
        buffer[i] = retval;
    }

end:
    /* De-select the device */
    hal_gpio_write(bma400->sensor.s_itf.si_cs_pin, 1);

    return rc;
}

/**
 * Write number of bytes to BMA400 sensor over I2C
 *
 * @param bma400 The device
 * @param buffer data buffer (reg1,val1,reg2,val2...)
 * @param len    number of bytes to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
bma400_i2c_write(struct bma400 *bma400, const uint8_t *buffer, uint8_t len)
{
    int i, j;
    int rc = 0;
    struct hal_i2c_master_data data_struct = {
        .address = bma400->sensor.s_itf.si_addr,
        .len = len,
        .buffer = (uint8_t)buffer
    };
    rc = i2cn_master_write(bma400->sensor.s_itf.si_num, &data_struct,
                           MYNEWT_VAL(BMA400_I2C_TIMEOUT_TICKS), 1,
                           MYNEWT_VAL(BMA400_I2C_RETRIES));
    if (rc) {
        BMA400_LOG_ERROR("I2C access failed at address 0x%02X\n",
                         data_struct.address);
        STATS_INC(g_bma400_stats, write_errors);
        break;
    }

    return rc;
}

/**
 * Write number of bytes to BMA400 sensor over SPI
 *
 * @param bma400 The device
 * @param buffer data buffer (reg1,val1,reg2,val2...)
 * @param len    number of bytes to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
bma400_spi_write(struct bma400 *bma400, uint8_t reg,
                 const uint8_t *buffer, uint8_t len)
{
    int i;
    int rc = 0;

    /* Select the device */
    hal_gpio_write(bma400->sensor.s_itf.si_cs_pin, 0);

    for (i = 0; i < len; i++) {
        rc = hal_spi_tx_val(bma400->sensor.s_itf.si_num, buffer[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            BMA400_LOG_ERROR("SPI_%u write failed addr:0x%02X\n",
                             bma400->sensor.s_itf.si_num, reg);
            STATS_INC(g_bma400_stats, write_errors);
            goto end;
        }
    }

end:
    /* De-select the device */
    hal_gpio_write(bma400->sensor.s_itf.si_cs_pin, 1);

    return rc;
}
#endif /* BUS_DRIVER_PRESENT */

int
bma400_write(struct bma400 *bma400, uint8_t reg,
             const uint8_t *buffer, uint8_t len)
{
    int rc = 0;
    int i, j;
    uint8_t write_data[32];

    for (i = 0, j = 0; rc == 0 && i < len; ++reg) {
        write_data[j++] = reg;
        write_data[j++] = buffer[i++];
        if (j >= sizeof(write_data) || i >= len) {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
            rc = bus_node_simple_write((struct os_dev *)bma400, &write_data, j);
#else /* BUS_DRIVER_PRESENT */
            rc = sensor_itf_lock(&bma400->sensor.s_itf, MYNEWT_VAL(BMA400_ITF_LOCK_TMO));
            if (rc) {
                break;
            }

            if (bma400->sensor.s_itf.si_type == SENSOR_ITF_I2C) {
                rc = bma400_i2c_write(bma400, reg, write_data, j);
            } else {
                rc = bma400_spi_write(bma400, reg, write_data, j);
            }

            sensor_itf_unlock(&bma400->sensor.s_itf);
#endif /* BUS_DRIVER_PRESENT */
            j = 0;
        }
    }
    return rc;
}

int
bma400_read(struct bma400 *bma400, uint8_t reg,
            uint8_t *buffer, uint8_t len)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    uint8_t reg_and_dummy[2] = { reg, 0 };

    if (bma400->node_is_spi) {
        BMA400_SPI_READ_CMD_BIT(reg);
    }

    rc = bus_node_simple_write_read_transact((struct os_dev *)bma400, reg_and_dummy, bma400->node_is_spi ? 2 : 1,
                                             buffer, len);
#else /* BUS_DRIVER_PRESENT */
    rc = sensor_itf_lock(&bma400->sensor.s_itf, MYNEWT_VAL(BMA400_ITF_LOCK_TMO));
    if (rc) {
        goto end;
    }

    if (bma400->sensor.s_itf.si_type == SENSOR_ITF_I2C) {
        rc = bma400_i2c_read(bma400, reg, buffer, len);
    } else {
        rc = bma400_spi_read(bma400, reg, buffer, len);
    }

    sensor_itf_unlock(&bma400->sensor.s_itf);
end:
#endif /* BUS_DRIVER_PRESENT */

    return rc;
}

int
bma400_get_register(struct bma400 *bma400, uint8_t reg, uint8_t *data)
{
    int rc;
    uint8_t *cache_ptr = NULL;

    if (reg >= BMA400_REG_ACC_CONFIG0 && reg <= BMA400_REG_TAP_CONFIG1) {
        cache_ptr = &bma400->pdd.cache.regs[reg - BMA400_REG_ACC_CONFIG0];
    }

    if (cache_ptr && !bma400->pdd.cache.always_read) {
        *data = *cache_ptr;
        rc = 0;
    } else {
        rc = bma400_read(bma400, reg, data, 1);
        if (rc == 0 && cache_ptr) {
            *cache_ptr = *data;
        }
    }

    return rc;
}

int
bma400_set_register(struct bma400 *bma400, uint8_t reg, uint8_t data)
{
    int rc = 0;
    int ix;
    bool write_back;

    if (reg >= BMA400_REG_ACC_CONFIG0 && reg <= BMA400_REG_TAP_CONFIG1) {
        ix = reg - BMA400_REG_ACC_CONFIG0;
    } else {
        ix = -1;
        write_back = true;
    }

    if (ix >= 0) {
        write_back = bma400->pdd.cache.regs[ix] != data;
        bma400->pdd.cache.regs[ix] = data;
        if (write_back && bma400->pdd.transact) {
            bma400->pdd.cache.dirty |= 1ULL << ix;
            write_back = false;
        }
    }

    if (write_back) {
        rc = bma400_write(bma400, reg, &data, 1);
        if (rc == 0 && ix >= 0) {
            bma400->pdd.cache.dirty &= ~(1ULL << ix);
        }
    }

    return rc;
}

static void
bma400_begin_transact(struct bma400 *bma400)
{
    bma400->pdd.transact++;
}

static int
bma400_commit(struct bma400 *bma400)
{
    int rc = 0;
    int first_dirty = 0;
    int clean_count;
    int dirty_count;
    uint64_t dirty_mask;

    dirty_mask = bma400->pdd.cache.dirty;
    if (--bma400->pdd.transact == 0) {
        while (rc == 0 && dirty_mask) {
            clean_count = __builtin_ctzll(dirty_mask);
            dirty_mask >>= clean_count;
            dirty_mask ^= ~0;
            dirty_count = __builtin_ctzll(dirty_mask);
            dirty_mask ^= ~0;
            dirty_mask >>= dirty_count;
            first_dirty += clean_count;
            rc = bma400_write(bma400, first_dirty + BMA400_REG_ACC_CONFIG0,
                              bma400->pdd.cache.regs + first_dirty, dirty_count);
            if (rc == 0) {
                bma400->pdd.cache.dirty = dirty_mask << (first_dirty + dirty_count);
            }
            first_dirty += dirty_count;
        }
    }

    return rc;
}

int
bma400_set_register_field(struct bma400 *bma400, uint8_t reg,
                          uint8_t field_mask, uint8_t field_val)
{
    int rc;

    uint8_t new_data;
    uint8_t old_data;

    rc = bma400_get_register(bma400, reg, &old_data);
    if (rc) {
        goto end;
    }

    new_data = ((old_data & (~field_mask)) | SET_FIELD(field_val, field_mask));

    /* Try to limit bus access if possible */
    if (new_data != old_data) {
        rc = bma400_set_register(bma400, reg, new_data);
    }
end:
    return rc;
}

static int
bma400_get_register_field(struct bma400 *bma400, uint8_t reg,
                          uint8_t field_mask, uint8_t *field_val)
{
    int rc;
    uint8_t reg_val;

    rc = bma400_get_register(bma400, reg, &reg_val);
    if (rc) {
        goto end;
    }

    *field_val = GET_FIELD(reg_val, field_mask);

end:
    return rc;
}

int
bma400_get_chip_id(struct bma400 *bma400, uint8_t *chip_id)
{
    return bma400_get_register(bma400, BMA400_REG_CHIPID, chip_id);
}

/**
 * Convert acceleration data from 16 bits signed value to floating point number
 *
 * @param int32g gravity (7FFF = +32G, 0001 = -32G)
 * @return gravity in m/s^2
 */
static float
convert_int32g_to_f(int16_t int32g)
{
    return int32g * (STANDARD_ACCEL_GRAVITY / 1024);
}

/**
 * Convert data from ACC_x_LSB, ACC_x_MSB to signed value in range (7FFF = +32G, 0001 = -32G)
 * 12 bits data is not signed extended when read from registers.
 *
 * @param data  bytes read from ACC_x_LSB, ACC_x_MSB registers
 * @param range range configured in ACC_CONFIG1 register
 * @return gravity value sign extended
 */
static int16_t
convert_raw_to_int32g(const uint8_t *data, bma400_g_range_t range)
{
    return ((int16_t)((data[1] << 12) | (data[0] << 4))) >> (4 - range);
}

/**
 * Convert 2 bytes data read from FIFO to signed value in range (7FFF = +32G, 0001 = -32G)
 * @param data  two bytes read from FIFO
 * @param range range configured in ACC_CONFIG1 register
 * @return gravity value sign extended
 */
static int16_t
convert_fifo_to_int32g(uint8_t lo, uint8_t hi, bma400_g_range_t range)
{
    return ((int16_t)((hi << 8) | lo)) >> (4 - range);
}

bma400_g_range_t
bma400_get_cached_range(struct bma400 *bma400)
{
    return (bma400->pdd.cache.acc_config1 & BMA400_ACC_CONFIG1_ACC_RANGE) >> __builtin_ctz(BMA400_ACC_CONFIG1_ACC_RANGE);
}

int
bma400_get_axis_accel(struct bma400 *bma400, bma400_axis_t axis, float *accel_data)
{
    uint8_t base_addr;
    uint8_t data[2];
    int rc = 0;

    switch (axis) {
    case AXIS_X:
        base_addr = BMA400_REG_ACC_X_LSB;
        break;
    case AXIS_Y:
        base_addr = BMA400_REG_ACC_Y_LSB;
        break;
    case AXIS_Z:
        base_addr = BMA400_REG_ACC_Z_LSB;
        break;
    default:
        rc = SYS_EINVAL;
    }

    if (rc == 0) {
        rc = bma400_read(bma400, base_addr, data, sizeof(data));
    }
    if (rc == 0) {
        *accel_data = convert_int32g_to_f(convert_raw_to_int32g(data, bma400_get_cached_range(bma400)));
    }

    return rc;
}

/**
 * Reads acc data
 *
 * @param bma400     the device
 * @param accel_data 3 axis integer data
 * @return 0 on success, non-zero on failure.
 */
int
bma400_get_accel_int_32g(struct bma400 *bma400, int16_t accel_data[3])
{
    uint8_t data[6];
    int rc;
    uint8_t range;

    rc = bma400_get_register_field(bma400, BMA400_REG_ACC_CONFIG0, BMA400_ACC_CONFIG1_ACC_RANGE, &range);
    if (rc) {
        goto end;
    }

    rc = bma400_read(bma400, BMA400_REG_ACC_X_LSB, data, sizeof(data));
    if (rc) {
        goto end;
    }

    /* Shift left to remove unused bits, then shift right to saturate with range 32g */
    accel_data[0] = convert_raw_to_int32g(&data[0], range);
    accel_data[1] = convert_raw_to_int32g(&data[2], range);
    accel_data[2] = convert_raw_to_int32g(&data[4], range);

end:
    return rc;
}

int
bma400_get_accel(struct bma400 *bma400, float accel_data[3])
{
    int16_t data[3];
    int rc;

    rc = bma400_get_accel_int_32g(bma400, data);
    if (rc == 0) {
        accel_data[0] = convert_int32g_to_f(data[0]);
        accel_data[1] = convert_int32g_to_f(data[1]);
        accel_data[2] = convert_int32g_to_f(data[2]);
    }

    return rc;
}

int
bma400_get_temp(struct bma400 *bma400, float *temp_c)
{
    int8_t data;
    int rc;

    rc = bma400_get_register(bma400, BMA400_REG_TEMP_DATA, (uint8_t *)&data);
    if (rc == 0) {
        *temp_c = data * 0.5f + 23.0f;
    }

    return rc;
}

int
bma400_get_int_status(struct bma400 *bma400,
                      struct bma400_int_status *int_status)
{
    int rc;

    rc = bma400_read(bma400, BMA400_REG_INT_STAT0, &int_status->int_stat0, sizeof(*int_status));

    return rc;
}

int
bma400_get_fifo_count(struct bma400 *bma400, uint16_t *fifo_bytes)
{
    uint8_t data[2];
    int rc;

    rc = bma400_read(bma400, BMA400_REG_FIFO_LENGTH0, data, 2);
    if (rc == 0) {
        *fifo_bytes = data[0] | ((data[1] & 0x07) << 8);
    }
    return rc;
}

int
bma400_get_g_range(struct bma400 *bma400,
                   bma400_g_range_t *g_range)
{
    uint8_t field_val;
    int rc;

    rc = bma400_get_register_field(bma400, BMA400_REG_ACC_CONFIG1, BMA400_ACC_CONFIG1_ACC_RANGE, &field_val);
    if (rc != 0) {
        *g_range = (bma400_g_range_t)field_val;
    }

    return rc;
}

int
bma400_set_g_range(struct bma400 *bma400,
                   bma400_g_range_t g_range)
{
    return bma400_set_register_field(bma400, BMA400_REG_ACC_CONFIG1, BMA400_ACC_CONFIG1_ACC_RANGE, (uint8_t)g_range);
}

int
bma400_set_filt1_bandwidth(struct bma400 *bma400, bma400_filt1_bandwidth_t bandwidth)
{
    return bma400_set_register_field(bma400, BMA400_REG_ACC_CONFIG0, BMA400_ACC_CONFIG0_FILT1_BW, (uint8_t)bandwidth);
}

int
bma400_set_power_mode(struct bma400 *bma400, bma400_power_mode_t power_mode)
{
    return bma400_set_register_field(bma400, BMA400_REG_ACC_CONFIG0, BMA400_ACC_CONFIG0_POWER_MODE_CONF,
                                     (uint8_t)power_mode);
}

int
bma400_get_power_mode(struct bma400 *bma400, bma400_power_mode_t *power_mode)
{
    return bma400_get_register_field(bma400, BMA400_REG_ACC_CONFIG0, BMA400_ACC_CONFIG0_POWER_MODE_CONF,
                                     (uint8_t *)power_mode);
}

int
bma400_set_odr(struct bma400 *bma400, bma400_odr_t odr)
{
    return bma400_set_register_field(bma400, BMA400_REG_ACC_CONFIG1, BMA400_ACC_CONFIG1_ACC_ODR, (uint8_t)odr);
}

int
bma400_set_data_src(struct bma400 *bma400, bma400_data_src_t src)
{
    return bma400_set_register_field(bma400, BMA400_REG_ACC_CONFIG2, BMA400_ACC_CONFIG2_DATA_SRC_REG, (uint8_t)src);
}

int
bma400_set_acc_cfg(struct bma400 *bma400, struct bma400_acc_cfg *cfg)
{
    bma400_begin_transact(bma400);

    bma400_set_filt1_bandwidth(bma400, cfg->filt1_bw);
    bma400_set_register_field(bma400, BMA400_REG_ACC_CONFIG0, BMA400_ACC_CONFIG0_OSR_LP, cfg->osr_lp);
    bma400_set_power_mode(bma400, cfg->power_mode_conf);

    bma400_set_g_range(bma400, cfg->acc_range);
    bma400_set_odr(bma400, cfg->acc_odr);

    bma400_set_data_src(bma400, cfg->data_src_reg);

    return bma400_commit(bma400);
}

static void
bma400_arm_interrupt(struct bma400 *bma400, bma400_int_num_t int_num, hal_gpio_irq_trig_t trig)
{
    struct bma400_private_driver_data *pdd;
    pdd = &bma400->pdd;
    int host_pin;
    int int_ix = (int)int_num - 1;

    if (int_ix >= 0 && pdd->intr.armed_trigger[int_ix] != trig) {
        host_pin = pdd->intr.ints[int_ix].host_pin;
        if (pdd->intr.armed_trigger[int_ix] != HAL_GPIO_TRIG_NONE) {
            hal_gpio_irq_release(host_pin);
        }

        pdd->intr.armed_trigger[int_ix] = trig;
        if (trig != HAL_GPIO_TRIG_NONE) {
            hal_gpio_irq_init(host_pin,
                              bma400_interrupt_handler, bma400,
                              trig,
                              HAL_GPIO_PULL_NONE);
            hal_gpio_irq_enable(host_pin);
        }
    }
}

int
bma400_set_int12_cfg(struct bma400 *bma400, struct bma400_int_pin_cfg *cfg)
{
    hal_gpio_irq_trig_t trig;

    if (cfg->int1_host_pin != bma400->pdd.intr.ints[0].host_pin) {
        if (bma400->pdd.intr.ints[0].host_pin >= 0) {
            hal_gpio_irq_release(bma400->pdd.intr.ints[0].host_pin);
        }
        if (cfg->int1_host_pin >= 0) {
            trig = cfg->int1_level ? HAL_GPIO_TRIG_RISING : HAL_GPIO_TRIG_FALLING;
            bma400->pdd.intr.ints[0].host_pin = cfg->int1_host_pin;
            bma400->pdd.intr.ints[0].active = cfg->int1_level;
            bma400_arm_interrupt(bma400, BMA400_INT1_PIN, trig);
        }
    }
    if (cfg->int2_host_pin != bma400->pdd.intr.ints[1].host_pin) {
        if (bma400->pdd.intr.ints[1].host_pin >= 0) {
            hal_gpio_irq_release(bma400->pdd.intr.ints[1].host_pin);
        }
        if (cfg->int2_host_pin >= 0) {
            trig = cfg->int1_level ? HAL_GPIO_TRIG_RISING : HAL_GPIO_TRIG_FALLING;
            bma400->pdd.intr.ints[1].host_pin = cfg->int2_host_pin;
            bma400->pdd.intr.ints[1].active = cfg->int2_level;
            bma400_arm_interrupt(bma400, BMA400_INT2_PIN, trig);
        }
    }

    bma400_begin_transact(bma400);

    bma400_set_register_field(bma400, BMA400_REG_INT12_IO_CTRL, BMA400_INT12_IO_CTRL_INT1_LVL, cfg->int1_level);
    bma400_set_register_field(bma400, BMA400_REG_INT12_IO_CTRL, BMA400_INT12_IO_CTRL_INT2_LVL, cfg->int2_level);
    bma400_set_register_field(bma400, BMA400_REG_INT12_IO_CTRL, BMA400_INT12_IO_CTRL_INT1_OD, cfg->int1_od);
    bma400_set_register_field(bma400, BMA400_REG_INT12_IO_CTRL, BMA400_INT12_IO_CTRL_INT2_OD, cfg->int2_od);
    bma400_set_register_field(bma400, BMA400_REG_INT_CONFIG1, BMA400_INT_CONFIG1_LATCH_INT, cfg->latch_int);

    return bma400_commit(bma400);
}

int
bma400_set_activity_cfg(struct bma400 *bma400, struct bma400_activity_cfg *cfg)
{
    bma400_begin_transact(bma400);

    bma400_set_register(bma400, BMA400_REG_ACTCH_CONFIG0, cfg->actch_thres);
    bma400_set_register_field(bma400, BMA400_REG_ACTCH_CONFIG1, BMA400_ACTCH_CONFIG1_ACTCH_X_EN, cfg->actch_x_en);
    bma400_set_register_field(bma400, BMA400_REG_ACTCH_CONFIG1, BMA400_ACTCH_CONFIG1_ACTCH_Y_EN, cfg->actch_y_en);
    bma400_set_register_field(bma400, BMA400_REG_ACTCH_CONFIG1, BMA400_ACTCH_CONFIG1_ACTCH_Z_EN, cfg->actch_z_en);
    bma400_set_register_field(bma400, BMA400_REG_ACTCH_CONFIG1, BMA400_ACTCH_CONFIG1_ACTCH_NPTS, cfg->actch_npts);

    bma400_set_register_field(bma400, BMA400_REG_INT12_MAP, BMA400_INT12_MAP_ACTCH_INT1,
                              cfg->int_num == BMA400_INT1_PIN);
    bma400_set_register_field(bma400, BMA400_REG_INT12_MAP, BMA400_INT12_MAP_ACTCH_INT2,
                              cfg->int_num == BMA400_INT2_PIN);

    if (cfg->int_num != BMA400_NO_INT_PIN) {
        bma400->pdd.allowed_events |= cfg->event_type;
    } else {
        bma400->pdd.allowed_events &= ~cfg->event_type;
    }

    return bma400_commit(bma400);
}

int
bma400_set_step_counter_cfg(struct bma400 *bma400, struct bma400_step_cfg *cfg)
{
    int rc = 0;

    if (cfg->step_counter_config) {
        rc = bma400_write(bma400, BMA400_REG_STEP_COUNTER_CONFIG0, cfg->step_counter_config, 24);
    }

    if (rc == 0) {
        bma400_begin_transact(bma400);

        bma400_set_register_field(bma400, BMA400_REG_INT12_MAP, BMA400_INT12_MAP_STEP_INT1,
                                  cfg->int_num == BMA400_INT1_PIN);
        bma400_set_register_field(bma400, BMA400_REG_INT12_MAP, BMA400_INT12_MAP_STEP_INT2,
                                  cfg->int_num == BMA400_INT2_PIN);

        rc = bma400_commit(bma400);
    }
    return rc;
}

int
bma400_get_step_counter(struct bma400 *bma400, uint32_t *counter)
{
    int rc;
    uint8_t data[3];

    rc = bma400_read(bma400, BMA400_REG_STEP_CNT_0, data, 3);
    if (rc == 0) {
        *counter = data[0] + (data[1] << 8) + (data[2] << 16);
    }

    return rc;
}

int
bma400_set_autolowpow_mode(struct bma400 *bma400, struct bma400_autolowpow_cfg *cfg)
{
    bma400_begin_transact(bma400);

    bma400_set_register(bma400, BMA400_REG_AUTOLOWPOW_0, cfg->timeout_threshold >> 4);
    bma400_set_register_field(bma400, BMA400_REG_AUTOLOWPOW_1, BMA400_AUTOLOWPOW_1_AUTO_LP_TIMEOUT_THRES, cfg->timeout_threshold & 15);
    bma400_set_register_field(bma400, BMA400_REG_AUTOLOWPOW_1, BMA400_AUTOLOWPOW_1_AUTO_LP_TIMEOUT, cfg->timeout);
    bma400_set_register_field(bma400, BMA400_REG_AUTOLOWPOW_1, BMA400_AUTOLOWPOW_1_GEN1_INT, cfg->trig_gen1);
    bma400_set_register_field(bma400, BMA400_REG_AUTOLOWPOW_1, BMA400_AUTOLOWPOW_1_DRDY_LOWPOW_TRIG, cfg->drdy_lowpow_trig);
    /* If Gen2int is selected source of resetting low power timeout enabled it in INT_CONFIG0 register */
    if (cfg->timeout == BMA400_AUTOLOWPOW_TIMEOUT_2) {
        bma400_set_register_field(bma400, BMA400_REG_INT_CONFIG0, BMA400_INT_CONFIG0_GEN2_INT_EN, 1);
    }

    return bma400_commit(bma400);
}

int
bma400_set_autowakeup(struct bma400 *bma400, struct bma400_autowakeup_cfg *cfg)
{
    bma400_begin_transact(bma400);

    bma400_set_register(bma400, BMA400_REG_AUTOWAKEUP_0, cfg->timeout_threshold >> 4);
    bma400_set_register_field(bma400, BMA400_REG_AUTOWAKEUP_1, BMA400_AUTOWAKEUP_1_WAKEUP_TIMEOUT_THRES, cfg->timeout_threshold & 15);
    bma400_set_register_field(bma400, BMA400_REG_AUTOWAKEUP_1, BMA400_AUTOWAKEUP_1_WKUP_TIMEOUT, cfg->wkup_timeout);
    bma400_set_register_field(bma400, BMA400_REG_AUTOWAKEUP_1, BMA400_AUTOWAKEUP_1_WKUP_INT, cfg->wkup_int);

    return bma400_commit(bma400);
}

int
bma400_set_wakeup(struct bma400 *bma400, struct bma400_wakeup_cfg *cfg)
{
    bma400_begin_transact(bma400);

    bma400_set_register_field(bma400, BMA400_REG_WKUP_INT_CONFIG0, BMA400_WKUP_INT_CONFIG0_WKUP_Z_EN, cfg->wkup_z_en);
    bma400_set_register_field(bma400, BMA400_REG_WKUP_INT_CONFIG0, BMA400_WKUP_INT_CONFIG0_WKUP_Y_EN, cfg->wkup_y_en);
    bma400_set_register_field(bma400, BMA400_REG_WKUP_INT_CONFIG0, BMA400_WKUP_INT_CONFIG0_WKUP_X_EN, cfg->wkup_x_en);
    bma400_set_register_field(bma400, BMA400_REG_WKUP_INT_CONFIG0, BMA400_WKUP_INT_CONFIG0_NUM_OF_SAMPLES, cfg->num_of_samples);
    bma400_set_register_field(bma400, BMA400_REG_WKUP_INT_CONFIG0, BMA400_WKUP_INT_CONFIG0_WKUP_REFU, cfg->wkup_refu);

    bma400_set_register(bma400, BMA400_REG_WKUP_INT_CONFIG1, cfg->int_wkup_thres);
    bma400_set_register(bma400, BMA400_REG_WKUP_INT_CONFIG2, cfg->int_wkup_refx);
    bma400_set_register(bma400, BMA400_REG_WKUP_INT_CONFIG3, cfg->int_wkup_refy);
    bma400_set_register(bma400, BMA400_REG_WKUP_INT_CONFIG4, cfg->int_wkup_refz);

    bma400_set_register_field(bma400, BMA400_REG_INT1_MAP, BMA400_INT1_MAP_WKUP_INT1, cfg->int_num == BMA400_INT1_PIN);
    bma400_set_register_field(bma400, BMA400_REG_INT2_MAP, BMA400_INT2_MAP_WKUP_INT2, cfg->int_num == BMA400_INT2_PIN);

    if (cfg->int_num != BMA400_NO_INT_PIN) {
        bma400->pdd.allowed_events |= SENSOR_EVENT_TYPE_SLEEP | SENSOR_EVENT_TYPE_WAKEUP;
    }

    return bma400_commit(bma400);
}

int
bma400_set_gen_int(struct bma400 *bma400, bma400_get_int_t gen_int, struct bma400_gen_int_cfg *cfg)
{
    uint8_t gen_int_off = gen_int * (BMA400_REG_GEN2INT_CONFIG0 - BMA400_REG_GEN1INT_CONFIG0);
    bma400_begin_transact(bma400);

    bma400_set_register_field(bma400, BMA400_REG_GEN1INT_CONFIG0 + gen_int_off, BMA400_GEN1INT_CONFIG0_GEN1_ACT_Z_EN, cfg->gen_act_z_en);
    bma400_set_register_field(bma400, BMA400_REG_GEN1INT_CONFIG0 + gen_int_off, BMA400_GEN1INT_CONFIG0_GEN1_ACT_Y_EN, cfg->gen_act_y_en);
    bma400_set_register_field(bma400, BMA400_REG_GEN1INT_CONFIG0 + gen_int_off, BMA400_GEN1INT_CONFIG0_GEN1_ACT_X_EN, cfg->gen_act_x_en);
    bma400_set_register_field(bma400, BMA400_REG_GEN1INT_CONFIG0 + gen_int_off, BMA400_GEN1INT_CONFIG0_GEN1_DATA_SRC, cfg->gen_data_src);
    bma400_set_register_field(bma400, BMA400_REG_GEN1INT_CONFIG0 + gen_int_off, BMA400_GEN1INT_CONFIG0_GEN1_ACT_REFU, cfg->gen_act_refu);
    bma400_set_register_field(bma400, BMA400_REG_GEN1INT_CONFIG0 + gen_int_off, BMA400_GEN1INT_CONFIG0_GEN1_ACT_HYST, cfg->gen_act_hyst);
    bma400_set_register_field(bma400, BMA400_REG_GEN1INT_CONFIG1 + gen_int_off, BMA400_GEN1INT_CONFIG1_GEN1_CRITERION_SEL, cfg->gen_criterion_sel);
    bma400_set_register_field(bma400, BMA400_REG_GEN1INT_CONFIG1 + gen_int_off, BMA400_GEN1INT_CONFIG1_GEN1_COMB_SEL, cfg->gen_comb_sel);

    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG2 + gen_int_off, cfg->get_int_thres);
    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG3 + gen_int_off, cfg->get_int_dur >> 8);
    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG31 + gen_int_off, (uint8_t)cfg->get_int_dur);
    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG4 + gen_int_off, (uint8_t)cfg->get_int_th_refx);
    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG5 + gen_int_off, cfg->get_int_th_refx >> 8);
    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG6 + gen_int_off, (uint8_t)cfg->get_int_th_refy);
    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG7 + gen_int_off, cfg->get_int_th_refy >> 8);
    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG8 + gen_int_off, (uint8_t)cfg->get_int_th_refz);
    bma400_set_register(bma400, BMA400_REG_GEN1INT_CONFIG9 + gen_int_off, cfg->get_int_th_refz >> 8);

    if (gen_int == BMA400_GEN_INT_1) {
        bma400_set_register_field(bma400, BMA400_REG_INT1_MAP, BMA400_INT1_MAP_GEN1_INT1,
                                  cfg->int_num == BMA400_INT1_PIN);
        bma400_set_register_field(bma400, BMA400_REG_INT2_MAP, BMA400_INT2_MAP_GEN1_INT2,
                                  cfg->int_num == BMA400_INT2_PIN);
    } else {
        bma400_set_register_field(bma400, BMA400_REG_INT1_MAP, BMA400_INT1_MAP_GEN2_INT1,
                                  cfg->int_num == BMA400_INT1_PIN);
        bma400_set_register_field(bma400, BMA400_REG_INT2_MAP, BMA400_INT2_MAP_GEN2_INT2,
                                  cfg->int_num == BMA400_INT2_PIN);
    }
    if (cfg->event_type) {
        bma400->pdd.allowed_events |= cfg->event_type;
    } else {
        bma400->pdd.allowed_events &= ~cfg->event_type;
    }

    return bma400_commit(bma400);
}

int
bma400_soft_reset(struct bma400 *bma400)
{
    int rc = 0;
    uint8_t ready = 0;

    while (rc == 0 && !ready) {
        rc = bma400_get_register_field(bma400, BMA400_REG_STATUS, BMA400_STATUS_CMD_RDY, &ready);
    }
    if (rc == 0) {
        rc = bma400_set_register(bma400, BMA400_REG_CMD, BMA400_CMD_SOFT_RESET);
        ready = rc != 0;
    }
    while (rc == 0 && !ready) {
        rc = bma400_get_register_field(bma400, BMA400_REG_STATUS, BMA400_STATUS_CMD_RDY, &ready);
    }

    bma400->pdd.cache.dirty = 0;
    if (rc == 0) {
        rc = bma400_read(bma400, BMA400_REG_ACC_CONFIG0, bma400->pdd.cache.regs, sizeof(bma400->pdd.cache.regs));
    }

    return rc;
}

int
bma400_set_orient_cfg(struct bma400 *bma400,
                      struct bma400_orient_cfg *cfg)
{
    bma400_begin_transact(bma400);

    bma400_set_register_field(bma400, BMA400_REG_ORIENTCH_CONFIG0, BMA400_ORIENTCH_CONFIG0_ORIENT_Z_EN, cfg->orient_z_en);
    bma400_set_register_field(bma400, BMA400_REG_ORIENTCH_CONFIG0, BMA400_ORIENTCH_CONFIG0_ORIENT_Y_EN, cfg->orient_y_en);
    bma400_set_register_field(bma400, BMA400_REG_ORIENTCH_CONFIG0, BMA400_ORIENTCH_CONFIG0_ORIENT_X_EN, cfg->orient_x_en);
    bma400_set_register_field(bma400, BMA400_REG_ORIENTCH_CONFIG0, BMA400_ORIENTCH_CONFIG0_ORIENT_DATA_SRC, cfg->orient_data_src);
    bma400_set_register_field(bma400, BMA400_REG_ORIENTCH_CONFIG0, BMA400_ORIENTCH_CONFIG0_ORIENT_REFU, cfg->orient_refu);
    bma400_set_register(bma400, BMA400_REG_ORIENTCH_CONFIG1, cfg->orient_thres);
    bma400_set_register(bma400, BMA400_REG_ORIENTCH_CONFIG3, cfg->orient_dur);
    bma400_set_register(bma400, BMA400_REG_ORIENTCH_CONFIG4, (uint8_t)cfg->int_orient_refx);
    bma400_set_register(bma400, BMA400_REG_ORIENTCH_CONFIG5, cfg->int_orient_refx >> 8);
    bma400_set_register(bma400, BMA400_REG_ORIENTCH_CONFIG6, (uint8_t)cfg->int_orient_refy);
    bma400_set_register(bma400, BMA400_REG_ORIENTCH_CONFIG7, cfg->int_orient_refy >> 8);
    bma400_set_register(bma400, BMA400_REG_ORIENTCH_CONFIG8, (uint8_t)cfg->int_orient_refz);
    bma400_set_register(bma400, BMA400_REG_ORIENTCH_CONFIG9, cfg->int_orient_refz >> 8);

    bma400_set_register_field(bma400, BMA400_REG_INT1_MAP, BMA400_INT1_MAP_ORIENTCH_INT1, cfg->int_num == BMA400_INT1_PIN);
    bma400_set_register_field(bma400, BMA400_REG_INT2_MAP, BMA400_INT2_MAP_ORIENTCH_INT2, cfg->int_num == BMA400_INT2_PIN);

    if (cfg->int_num != BMA400_NO_INT_PIN) {
        bma400->pdd.allowed_events |= SENSOR_EVENT_TYPE_ORIENT_CHANGE;
    } else {
        bma400->pdd.allowed_events &= ~SENSOR_EVENT_TYPE_ORIENT_CHANGE;
    }

    return bma400_commit(bma400);
}

int
bma400_set_tap_cfg(struct bma400 *bma400, struct bma400_tap_cfg *cfg)
{
    int rc;

    bma400_begin_transact(bma400);

    bma400_set_register_field(bma400, BMA400_REG_TAP_CONFIG0, BMA400_TAP_CONFIG_SEL_AXIS, cfg->sel_axis);
    bma400_set_register_field(bma400, BMA400_REG_TAP_CONFIG0, BMA400_TAP_CONFIG_TAP_SENSITIVITY, cfg->tap_sensitivity);
    bma400_set_register_field(bma400, BMA400_REG_TAP_CONFIG1, BMA400_TAP_CONFIG1_TICS_TH, cfg->tics_th);
    bma400_set_register_field(bma400, BMA400_REG_TAP_CONFIG1, BMA400_TAP_CONFIG1_QUIET, cfg->quite);
    bma400_set_register_field(bma400, BMA400_REG_TAP_CONFIG1, BMA400_TAP_CONFIG1_QUIET_DT, cfg->quite_dt);

    bma400_set_register_field(bma400, BMA400_REG_INT12_MAP, BMA400_INT12_MAP_TAP_INT1, cfg->int_num == BMA400_INT1_PIN);
    bma400_set_register_field(bma400, BMA400_REG_INT12_MAP, BMA400_INT12_MAP_TAP_INT2, cfg->int_num == BMA400_INT2_PIN);

    if (cfg->int_num != BMA400_NO_INT_PIN) {
        bma400->pdd.allowed_events |= SENSOR_EVENT_TYPE_SINGLE_TAP | SENSOR_EVENT_TYPE_DOUBLE_TAP;
    } else {
        bma400->pdd.allowed_events &= ~(SENSOR_EVENT_TYPE_SINGLE_TAP | SENSOR_EVENT_TYPE_DOUBLE_TAP);
    }

    rc = bma400_commit(bma400);

    return rc;
}

int
bma400_get_fifo_watermark(struct bma400 *bma400, uint16_t *watermark)
{
    uint8_t data[2];
    int rc;

    rc = bma400_get_register(bma400, BMA400_REG_FIFO_CONFIG1, &data[0]);

    if (rc == 0) {
        rc = bma400_get_register((struct bma400 *)bma400, BMA400_REG_FIFO_CONFIG2, &data[1]);
    }
    if (rc == 0) {
        *watermark = data[0] | ((data[1] & 7) << 8);
    }

    return rc;
}

int
bma400_set_fifo_watermark(struct bma400 *bma400, uint16_t watermark)
{
    bma400_begin_transact(bma400);

    bma400_set_register(bma400, BMA400_REG_FIFO_CONFIG1, (uint8_t)watermark);
    bma400_set_register(bma400, BMA400_REG_FIFO_CONFIG2, watermark >> 8);

    return bma400_commit(bma400);
}

int
bma400_set_fifo_cfg(struct bma400 *bma400,
                    struct bma400_fifo_cfg *cfg)
{
    bma400_begin_transact(bma400);

    bma400_set_register_field(bma400, BMA400_REG_FIFO_CONFIG0, BMA400_FIFO_CONFIG0_FIFO_Z_EN, cfg->fifo_z_en);
    bma400_set_register_field(bma400, BMA400_REG_FIFO_CONFIG0, BMA400_FIFO_CONFIG0_FIFO_Y_EN, cfg->fifo_y_en);
    bma400_set_register_field(bma400, BMA400_REG_FIFO_CONFIG0, BMA400_FIFO_CONFIG0_FIFO_X_EN, cfg->fifo_x_en);
    bma400_set_register_field(bma400, BMA400_REG_FIFO_CONFIG0, BMA400_FIFO_CONFIG0_FIFO_8BIT_EN, cfg->fifo_8bit_en);
    bma400_set_register_field(bma400, BMA400_REG_FIFO_CONFIG0, BMA400_FIFO_CONFIG0_FIFO_DATA_SRC, cfg->fifo_data_src);
    bma400_set_register_field(bma400, BMA400_REG_FIFO_CONFIG0, BMA400_FIFO_CONFIG0_FIFO_TIME_EN, cfg->fifo_time_en);
    bma400_set_register_field(bma400, BMA400_REG_FIFO_CONFIG0, BMA400_FIFO_CONFIG0_FIFO_STOP_ON_FULL, cfg->fifo_stop_on_full);
    bma400_set_register_field(bma400, BMA400_REG_FIFO_CONFIG0, BMA400_FIFO_CONFIG0_AUTO_FLUSH, cfg->auto_flush);
    bma400_set_fifo_watermark(bma400, cfg->watermark);
    bma400_set_register_field(bma400, BMA400_REG_FIFO_PWR_CONFIG, BMA400_FIFO_PWR_CONFIG_FIFO_READ_DISABLE, cfg->fifo_read_disable);

    bma400_set_register_field(bma400, BMA400_REG_INT1_MAP, BMA400_INT1_MAP_FWM_INT1, cfg->int_num == BMA400_INT1_PIN);
    bma400_set_register_field(bma400, BMA400_REG_INT2_MAP, BMA400_INT2_MAP_FWM_INT2, cfg->int_num == BMA400_INT2_PIN);

    return bma400_commit(bma400);
}

int
bma400_read_fifo(struct bma400 *bma400, uint16_t *fifo_count, struct sensor_accel_data *sad)
{
    uint8_t data[1 + 6];
    int16_t acc_32g;
    int rc;
    struct bma400_fifo_cfg *cfg = &bma400->cfg.fifo_cfg;
    int data_record_size = 1 + (cfg->fifo_x_en + cfg->fifo_y_en + cfg->fifo_z_en) * (2 - cfg->fifo_8bit_en);
    bma400_g_range_t range = bma400_get_cached_range(bma400);

    for (;;) {
        if (*fifo_count < sizeof(data)) {
            rc = bma400_get_fifo_count(bma400, fifo_count);
            if (rc != 0 || *fifo_count < sizeof(data)) {
                return rc;
            }
        }

        rc = bma400_read(bma400, BMA400_REG_FIFO_DATA, data, sizeof(data));
        if (rc == 0) {
            switch (data[0] & 0xE0) {
            case 0x80: /* Data frame */
                *fifo_count -= data_record_size;
                if (cfg->fifo_x_en) {
                    sad->sad_x_is_valid = 1;
                    if (cfg->fifo_8bit_en) {
                        acc_32g = convert_fifo_to_int32g(0, data[1], range);
                    } else {
                        acc_32g = convert_fifo_to_int32g(data[1], data[2], range);
                    }
                    sad->sad_x = convert_int32g_to_f(acc_32g);
                }
                if (cfg->fifo_y_en) {
                    sad->sad_y_is_valid = 1;
                    if (cfg->fifo_8bit_en) {
                        acc_32g = convert_fifo_to_int32g(0, data[2], range);
                    } else {
                        acc_32g = convert_fifo_to_int32g(data[3], data[4], range);
                    }
                    sad->sad_y = convert_int32g_to_f(acc_32g);
                }
                if (cfg->fifo_z_en) {
                    sad->sad_z_is_valid = 1;
                    if (cfg->fifo_8bit_en) {
                        acc_32g = convert_fifo_to_int32g(0, data[3], range);
                    } else {
                        acc_32g = convert_fifo_to_int32g(data[5], data[6], range);
                    }
                    sad->sad_z = convert_int32g_to_f(acc_32g);
                }
                return 0;
                break;
            case 0xA0: /* Time frame */
                *fifo_count -= 4;
                break;
            case 0x40: /* Control frame */
                *fifo_count -= 2;
                break;
            }
        }
    }

    return 0;
}

static int
reset_and_recfg(struct bma400 *bma400)
{
    struct bma400_cfg *cfg;
    int rc;

    cfg = &bma400->cfg;

    rc = bma400_soft_reset(bma400);
    if (rc != 0) {
        return rc;
    }

    bma400_begin_transact(bma400);

    bma400_set_acc_cfg(bma400, &cfg->acc_cfg);
    bma400_set_int12_cfg(bma400, &cfg->int_pin_cfg);
    bma400_set_fifo_cfg(bma400, &bma400->cfg.fifo_cfg);
    bma400_set_autolowpow_mode(bma400, &bma400->cfg.autolowpow_cfg);
    bma400_set_autowakeup(bma400, &bma400->cfg.autowakeup_cfg);
    bma400_set_wakeup(bma400, &bma400->cfg.wakeup_cfg);
    bma400_set_orient_cfg(bma400, &bma400->cfg.orient_cfg);
    bma400_set_tap_cfg(bma400, &bma400->cfg.tap_cfg);
    bma400_set_activity_cfg(bma400, &bma400->cfg.activity_cfg);
    bma400_set_step_counter_cfg(bma400, &bma400->cfg.step_cfg);
    bma400_set_gen_int(bma400, BMA400_GEN_INT_1, &bma400->cfg.gen_int_cfg[0]);
    bma400_set_gen_int(bma400, BMA400_GEN_INT_2, &bma400->cfg.gen_int_cfg[1]);

    rc = bma400_commit(bma400);

    return rc;
}

#if MYNEWT_VAL(BMA400_INT_ENABLE)

static void
enable_intpin(struct bma400 *bma400)
{
    struct bma400_private_driver_data *pdd = &bma400->pdd;
    pdd->int_ref_cnt++;

    if (pdd->int_ref_cnt == 1) {
        if (pdd->intr.ints[0].host_pin >= 0) {
            hal_gpio_irq_enable(pdd->intr.ints[0].host_pin);
        }
        if (pdd->intr.ints[1].host_pin >= 0) {
            hal_gpio_irq_enable(pdd->intr.ints[1].host_pin);
        }
    }
}

static void
disable_intpin(struct bma400 *bma400)
{
    struct bma400_private_driver_data *pdd = &bma400->pdd;

    if (pdd->int_ref_cnt == 0) {
        return;
    }

    if (--pdd->int_ref_cnt == 0) {
        if (pdd->interrupt->ints[0].host_pin >= 0) {
            hal_gpio_irq_enable(pdd->intr.ints[0].host_pin);
        }
        if (pdd->interrupt->ints[1].host_pin >= 0) {
            hal_gpio_irq_enable(pdd->intr.ints[1].host_pin);
        }
    }
}
#endif

int
bma400_self_test(struct bma400 *bma400, bool *self_test_fail)
{
    float positive_vals[3];
    float negative_vals[3];
    int rc;

    bma400_begin_transact(bma400);

    /* Disable all interrupts */
    bma400_set_register(bma400, BMA400_REG_INT_CONFIG0, 0);
    bma400_set_register(bma400, BMA400_REG_INT_CONFIG1, 0);
    /* Normal mode */
    bma400_set_register(bma400, BMA400_REG_ACC_CONFIG0, 2);
    /* 4G, 100Hz */
    bma400_set_register(bma400, BMA400_REG_ACC_CONFIG1, 0x48);

    rc = bma400_commit(bma400);
    if (rc) {
        goto end;
    }

    delay_msec(2);

    /* Positive self-test excitation */
    rc = bma400_set_register(bma400, BMA400_REG_SELF_TEST, 0x07);
    if (rc) {
        goto end;
    }
    delay_msec(50);

    rc = bma400_get_accel(bma400, positive_vals);
    if (rc) {
        goto end;
    }

    /* Negative self-test excitation */
    rc = bma400_set_register(bma400, BMA400_REG_SELF_TEST, 0x0F);
    if (rc) {
        goto end;
    }

    delay_msec(50);

    rc = bma400_get_accel(bma400, negative_vals);
    if (rc) {
        goto end;
    }

    rc = bma400_set_register(bma400, BMA400_REG_SELF_TEST, 0);
    if (rc) {
        goto end;
    }

    /*
     * Self-test minimum difference for positive - negative excitation acceleration are:
     * x-axis: 1500mg, y-axis: 1200mg, z-axis: 250mg
     */
    *self_test_fail = (positive_vals[0] - negative_vals[0]) < (1.5f * STANDARD_ACCEL_GRAVITY) ||
                      (positive_vals[1] - negative_vals[1]) < (1.2f * STANDARD_ACCEL_GRAVITY) ||
                      (positive_vals[2] - negative_vals[2]) < (0.25f * STANDARD_ACCEL_GRAVITY);

end:
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
bma400_poll_read(struct sensor *sensor, sensor_type_t sensor_type,
                 sensor_data_func_t data_func, void *data_arg,
                 uint32_t timeout)
{
    int rc = SYS_EINVAL;
    struct bma400 *bma400 = (struct bma400 *)SENSOR_GET_DEVICE(sensor);
    float accel_data[3];
    struct sensor_accel_data sad;
    struct sensor_temp_data std;
    bma400_power_mode_t power_mode = BMA400_POWER_MODE_NORMAL;

    rc = bma400_get_power_mode(bma400, &power_mode);
    if (rc) {
        goto end;
    }
    if (power_mode == BMA400_POWER_MODE_SLEEP) {
        rc = bma400_set_power_mode(bma400, BMA400_POWER_MODE_NORMAL);
        if (rc) {
            goto end;
        }
    }

    if ((sensor_type & SENSOR_TYPE_ACCELEROMETER) != 0) {
        rc = bma400_get_accel(bma400, accel_data);
        if (rc != 0) {
            goto end;
        }
        sad.sad_x = accel_data[0];
        sad.sad_y = accel_data[1];
        sad.sad_z = accel_data[2];
        sad.sad_x_is_valid = 1;
        sad.sad_y_is_valid = 1;
        sad.sad_z_is_valid = 1;

        rc = data_func(sensor,
                       data_arg,
                       &sad,
                       SENSOR_TYPE_ACCELEROMETER);
        if (rc != 0) {
            goto end;
        }
    }

    if ((sensor_type & SENSOR_TYPE_TEMPERATURE) != 0) {
        rc = bma400_get_temp(bma400, &std.std_temp);
        if (rc != 0) {
            goto end;
        }

        std.std_temp_is_valid = 1;

        rc = data_func(sensor,
                       data_arg,
                       &std,
                       SENSOR_TYPE_TEMPERATURE);
    }
end:
    if (power_mode == BMA400_POWER_MODE_SLEEP) {
        rc = bma400_set_power_mode(bma400, BMA400_POWER_MODE_SLEEP);
    }

    return rc;
}

int
bma400_stream_read(struct sensor *sensor,
                   sensor_type_t sensor_type,
                   sensor_data_func_t data_func,
                   void *read_arg,
                   uint32_t time_ms)
{
    int rc;
    int i;
    os_time_t time_ticks;
    os_time_t stop_ticks;
    struct sensor_accel_data sad;
    struct bma400_private_driver_data *pdd;
    struct bma400 *bma400 = (struct bma400 *)SENSOR_GET_DEVICE(sensor);
    uint16_t fifo_count = 0;
    uint8_t old_config[17];
    struct bma400_int_status int_status;

    pdd = &bma400->pdd;

    memcpy(old_config, pdd->cache.regs, sizeof(old_config));

    stop_ticks = 0;

    bma400_begin_transact(bma400);
    /* Clear int */
    bma400_get_int_status(bma400, &int_status);
    bma400_set_power_mode(bma400, BMA400_POWER_MODE_NORMAL);
    bma400_set_register(bma400, BMA400_REG_AUTOLOWPOW_1, 0);
    bma400_set_register(bma400, BMA400_REG_INT1_MAP, BMA400_INT1_MAP_DRDY_INT1);
    bma400_set_register(bma400, BMA400_REG_INT_CONFIG0, BMA400_INT_CONFIG0_DRDY_INT_EN);
    bma400_set_register(bma400, BMA400_REG_INT_CONFIG1, 0);

    rc = bma400_commit(bma400);

#if MYNEWT_VAL(BMA400_INT_ENABLE)
    undo_interrupt(&bma400->pdd.intr);

    if (pdd->interrupt) {
        return SYS_EBUSY;
    }
    pdd->interrupt = &bma400->pdd.intr;
    enable_intpin(bma400);
#endif

    if (time_ms != 0) {
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc != 0) {
            goto done;
        }
        stop_ticks = os_time_get() + time_ticks;
    }

    for (;;) {
        wait_interrupt(&bma400->pdd.intr);

        rc = bma400_read_fifo(bma400, &fifo_count, &sad);
        if (rc != 0) {
            goto done;
        }

        if (data_func(sensor, read_arg, &sad, SENSOR_TYPE_ACCELEROMETER)) {
            break;
        }

        if (time_ms != 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
            break;
        }
    }

done:
#if MYNEWT_VAL(BMA400_INT_ENABLE)
    pdd->interrupt = NULL;
    disable_intpin(bma400);
#endif
    bma400_begin_transact(bma400);
    for (i = 0; i < sizeof(old_config); ++i) {
        (void)bma400_set_register(bma400, BMA400_REG_ACC_CONFIG0 + i, old_config[i]);
    }
    (void)bma400_commit(bma400);

    return rc;
}

static int
bma400_sensor_read(struct sensor *sensor,
                   sensor_type_t sensor_type,
                   sensor_data_func_t data_func,
                   void *data_arg,
                   uint32_t timeout)
{
    int rc;
    struct bma400 *bma400;

    if ((sensor_type & ~(SENSOR_TYPE_ACCELEROMETER |
                         SENSOR_TYPE_TEMPERATURE)) != 0) {
        rc = SYS_EINVAL;
        goto end;
    }

    bma400 = (struct bma400 *)SENSOR_GET_DEVICE(sensor);

    if (bma400->cfg.stream_read_mode) {
        rc = bma400_stream_read(sensor, sensor_type, data_func, data_arg, timeout);
    } else {
        rc = bma400_poll_read(sensor, sensor_type, data_func, data_arg, timeout);
    }

end:

    return rc;
}

static int
bma400_sensor_get_config(struct sensor *sensor,
                         sensor_type_t sensor_type,
                         struct sensor_cfg *cfg)
{
    int rc = 0;

    /* Only one bit should be set in sensor_type mask */
    if ((sensor_type & (sensor_type - 1)) != 0) {
        return SYS_EINVAL;
    }

    if (sensor_type & SENSOR_TYPE_ACCELEROMETER) {
        cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;
    } else if (sensor_type & SENSOR_TYPE_TEMPERATURE) {
        cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;
    } else {
        rc = SYS_EINVAL;
    }

    return rc;
}

static int
bma400_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct bma400 *bma400;

    bma400 = (struct bma400 *)SENSOR_GET_DEVICE(sensor);

    return bma400_config(bma400, (struct bma400_cfg *)cfg);
}

static void
bma400_set_event_int(struct bma400 *bma400, sensor_event_type_t sensor_event_type, int on)
{
    if (sensor_event_type == SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        bma400_set_register_field(bma400, BMA400_REG_INT_CONFIG1, BMA400_INT_CONFIG1_D_TAP_INT_EN, on);
    }
    if (sensor_event_type == SENSOR_EVENT_TYPE_SINGLE_TAP) {
        bma400_set_register_field(bma400, BMA400_REG_INT_CONFIG1, BMA400_INT_CONFIG1_S_TAP_INT_EN, on);
    }
    if (sensor_event_type == bma400->cfg.gen_int_cfg[0].event_type) {
        bma400_set_register_field(bma400, BMA400_REG_INT_CONFIG0, BMA400_INT_CONFIG0_GEN1_INT_EN, on);
    }
    if (sensor_event_type == bma400->cfg.gen_int_cfg[1].event_type) {
        bma400_set_register_field(bma400, BMA400_REG_INT_CONFIG0, BMA400_INT_CONFIG0_GEN2_INT_EN, on);
    }
    if (sensor_event_type == bma400->cfg.activity_cfg.event_type) {
        bma400_set_register_field(bma400, BMA400_REG_INT_CONFIG1, BMA400_INT_CONFIG1_ACTCH_INT_EN, on);
    }
    if (sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_X_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_Y_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_Z_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE ||
        sensor_event_type == SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE) {
        bma400_set_register_field(bma400, BMA400_REG_INT_CONFIG0, BMA400_INT_CONFIG0_ORIENTCH_INT_EN, on);
    }
}

static int
bma400_sensor_unset_notification(struct sensor *sensor,
                                 sensor_event_type_t registered_event)
{
#if MYNEWT_VAL(BMA400_INT_ENABLE)
    struct bma400 *bma400 = (struct bma400 *)SENSOR_GET_DEVICE(sensor);
    struct bma400_private_driver_data *pdd = &bma400->pdd;
    int rc;

    /* Supported event check */
    if ((registered_event & pdd->notify_ctx.snec_evtype) == 0) {
        return SYS_EINVAL;
    }

    pdd = &bma400->pdd;

    pdd->notify_ctx.snec_evtype &= ~registered_event;

    bma400_begin_transact(bma400);

    bma400_set_event_int(bma400, registered_event, 0);

    rc = bma400_commit(bma400);

    if (pdd->notify_ctx.snec_evtype == 0) {
        disable_intpin(bma400);
    }

    return rc;
#else
    return SYS_ENODEV;
#endif
}

static int
bma400_sensor_set_notification(struct sensor *sensor,
                               sensor_event_type_t requested_event)
{
#if MYNEWT_VAL(BMA400_INT_ENABLE)
    int rc;
    struct bma400 *bma400 = (struct bma400 *)SENSOR_GET_DEVICE(sensor);
    struct bma400_private_driver_data *pdd;

    pdd = &bma400->pdd;

    /* Supported event check */
    if ((requested_event & ~(pdd->allowed_events)) != 0) {
        return SYS_EINVAL;
    }

    bma400_begin_transact(bma400);

    bma400_set_event_int(bma400, requested_event, 1);

    rc = bma400_commit(bma400);

    if (rc == 0) {
        if (pdd->notify_ctx.snec_evtype == 0) {
            enable_intpin(bma400);
        }
        pdd->notify_ctx.snec_evtype |= requested_event;
    }

    return rc;
#else
    return SYS_ENODEV;
#endif
}

static int
bma400_process_orientation_change(struct bma400 *bma400)
{
    struct bma400_private_driver_data *pdd = &bma400->pdd;
    int16_t accel[3];
    uint8_t refu[6] = {0};
    int max_accel_axis = 0;
    int max_accel;
    int rc;

    rc = bma400_get_accel_int_32g(bma400, accel);
    if (rc == 0) {
        max_accel = abs(accel[0]);
        if (max_accel < abs(accel[1])) {
            max_accel = abs(accel[1]);
            max_accel_axis = 1;
        }
        if (max_accel < abs(accel[2])) {
            max_accel = abs(accel[2]);
            max_accel_axis = 2;
        }
        if (accel[max_accel_axis] < 0) {
            max_accel = -max_accel;
        }
        /* When manual update is selected write refu */
        if (0 == (bma400->pdd.cache.orientch_config0 & BMA400_ORIENTCH_CONFIG0_ORIENT_REFU)) {
            refu[max_accel_axis * 2] = (uint8_t)max_accel;
            refu[max_accel_axis * 2 + 1] = (uint8_t)(max_accel >> 8);
            bma400_write(bma400, BMA400_REG_ORIENTCH_CONFIG4, refu, 6);
        }

        if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE) &&
            max_accel_axis == 0 && accel[0] > 0) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE);
        } else if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE) &&
            max_accel_axis == 0 && accel[0] < 0) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE);
        }
        if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE) &&
            max_accel_axis == 1 && accel[1] > 0) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE);
        } else if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE) &&
            max_accel_axis == 1 && accel[1] < 0) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE);
        }
        if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE) &&
            max_accel_axis == 2 && accel[2] > 0) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE);
        } else if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE) &&
            max_accel_axis == 2 && accel[2] < 0) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE);
        }
        if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_ORIENT_CHANGE)) {
            sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_ORIENT_CHANGE);
        }
    }
    return rc;
}

static int
bma400_sensor_handle_interrupt(struct sensor *sensor)
{
#if MYNEWT_VAL(BMA400_INT_ENABLE)
    struct bma400 *bma400;
    struct bma400_private_driver_data *pdd;
    struct bma400_int_status int_status;
    int rc;
    uint8_t woke;
    int8_t wakeup_pin;
    int wakeup_pin_state;
    int16_t host_wakeup_pin;

    bma400 = (struct bma400 *)SENSOR_GET_DEVICE(sensor);
    wakeup_pin = (int8_t)bma400->cfg.wakeup_cfg.int_num - 1;
    pdd = &bma400->pdd;

    rc = bma400_get_int_status(bma400, &int_status);
    if (rc != 0) {
        BMA400_LOG_ERROR("Can not read int status err=0x%02x\n", rc);
        return rc;
    }

    if (wakeup_pin >= 0 &&
        (pdd->notify_ctx.snec_evtype & (SENSOR_EVENT_TYPE_WAKEUP | SENSOR_EVENT_TYPE_SLEEP)) != 0) {
        host_wakeup_pin = pdd->intr.ints[wakeup_pin].host_pin;
        wakeup_pin_state = hal_gpio_read(host_wakeup_pin);
        woke = wakeup_pin_state == pdd->intr.ints[wakeup_pin].active;

        bma400_arm_interrupt(bma400, bma400->cfg.wakeup_cfg.int_num,
                             wakeup_pin_state ? HAL_GPIO_TRIG_FALLING : HAL_GPIO_TRIG_RISING);

        if (woke != pdd->woke) {
            pdd->woke = woke;
            /*
             * Wakeup interrupt pin stays active whole time till device goes to sleep,
             * notify client only once.
             */
            if (woke) {
                if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_WAKEUP) {
                    sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_WAKEUP);
                }
            } else {
                if (pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_SLEEP) {
                    sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_SLEEP);
                }
            }
        }
    }

    if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_SINGLE_TAP) &&
        (int_status.int_stat1 & BMA400_INT_STAT1_S_TAP_INT_STAT)) {
        sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_SINGLE_TAP);
    }
    if ((pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_DOUBLE_TAP) &&
        (int_status.int_stat1 & BMA400_INT_STAT1_D_TAP_INT_STAT)) {
        sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_DOUBLE_TAP);
    }
    if ((pdd->notify_ctx.snec_evtype & bma400->cfg.gen_int_cfg[0].event_type) &&
        (int_status.int_stat0 & BMA400_INT_STAT0_GEN1_INT_STAT)) {
        sensor_mgr_put_notify_evt(&pdd->notify_ctx, bma400->cfg.gen_int_cfg[0].event_type);
    }
    if ((pdd->notify_ctx.snec_evtype & bma400->cfg.gen_int_cfg[1].event_type) &&
        (int_status.int_stat0 & BMA400_INT_STAT0_GEN2_INT_STAT)) {
        sensor_mgr_put_notify_evt(&pdd->notify_ctx, bma400->cfg.gen_int_cfg[1].event_type);
    }
    if ((pdd->notify_ctx.snec_evtype & bma400->cfg.activity_cfg.event_type) &&
        (int_status.int_stat0 & BMA400_INT_STAT2_ACTCH_XYZ_INT_STAT)) {
        sensor_mgr_put_notify_evt(&pdd->notify_ctx, bma400->cfg.activity_cfg.event_type);
    }
    if ((pdd->notify_ctx.snec_evtype & (SENSOR_EVENT_TYPE_ORIENT_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_X_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_Y_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_Z_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE |
            SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE)) &&
        (int_status.int_stat0 & BMA400_INT_STAT0_ORIENTCH_INT_STAT)) {
        rc = bma400_process_orientation_change(bma400);
    }

    return rc;
#else
    return SYS_ENODEV;
#endif
}

static struct sensor_driver bma400_sensor_driver = {
    .sd_read               = bma400_sensor_read,
    .sd_set_config         = bma400_sensor_set_config,
    .sd_get_config         = bma400_sensor_get_config,
    .sd_set_notification   = bma400_sensor_set_notification,
    .sd_unset_notification = bma400_sensor_unset_notification,
    .sd_handle_interrupt   = bma400_sensor_handle_interrupt,
};

int
bma400_config(struct bma400 *bma400, struct bma400_cfg *cfg)
{
    struct sensor * sensor;
    int rc;
    uint8_t chip_id;

    bma400->cfg = *cfg;

    sensor = &bma400->sensor;

    rc = bma400_get_chip_id(bma400, &chip_id);
    if (rc != 0) {
        return rc;
    }
    if (chip_id != 0x90) {
        BMA400_LOG_ERROR("received incorrect chip ID 0x%02X\n", chip_id);
        return SYS_EINVAL;
    }

    rc = reset_and_recfg(bma400);
    if (rc != 0) {
        return rc;
    }

    rc = sensor_set_type_mask(sensor, cfg->sensor_mask);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
bma400_init(struct os_dev *dev, void *arg)
{
    struct bma400 *bma400 = (struct bma400 *)dev;
    struct sensor *sensor;
    struct bma400_private_driver_data *pdd;
    int rc;

    if (!dev) {
        return SYS_ENODEV;
    }

    sensor = &bma400->sensor;

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        return rc;
    }

    rc = sensor_set_driver(sensor,
                           SENSOR_TYPE_ACCELEROMETER |
                           SENSOR_TYPE_TEMPERATURE,
                           &bma400_sensor_driver);
    if (rc != 0) {
        return rc;
    }

    if (arg) {
        rc = sensor_set_interface(sensor, arg);
        if (rc != 0) {
            return rc;
        }
    }

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        return rc;
    }

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER)
    rc = hal_spi_config(sensor->s_itf.si_num, &spi_bma400_settings);
	if (rc == EINVAL) {
		/* If spi is already enabled, for nrf52, it returns -1, We should not
			* fail if the spi is already enabled
			*/
		return rc;
	}

	rc = hal_spi_enable(sensor->s_itf.si_num);
	if (rc) {
		return rc;
	}

	rc = hal_gpio_init_out(sensor->s_itf.si_cs_pin, 1);
	if (rc) {
		return rc;
	}
#endif
#endif

#if MYNEWT_VAL(BMA400_INT_ENABLE)
    init_interrupt(&bma400->pdd.intr);

    pdd = &bma400->pdd;

    pdd->notify_ctx.snec_sensor = sensor;
#endif

    return 0;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    bma400_init((struct os_dev *)bnode, arg);
}

#if MYNEWT_VAL(BMA400_I2C_SUPPORT)
static int
bma400_create_i2c_sensor_dev(struct bma400 *bma400, const char *name,
                             const struct bma400_create_dev_cfg *cfg)
{
    const struct bus_i2c_node_cfg *i2c_cfg = &cfg->i2c_cfg;
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    bma400->node_is_spi = false;

    bma400->sensor.s_itf.si_dev = &bma400->i2c_node.bnode.odev;
    bus_node_set_callbacks((struct os_dev *)bma400, &cbs);

    rc = bus_i2c_node_create(name, &bma400->i2c_node, i2c_cfg, NULL);

    return rc;
}
#endif

#if MYNEWT_VAL(BMA400_SPI_SUPPORT)
static int
bma400_create_spi_sensor_dev(struct bma400 *bma400, const char *name,
                              const struct bma400_create_dev_cfg *cfg)
{
    const struct bus_spi_node_cfg *spi_cfg = &cfg->spi_cfg;
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    bma400->node_is_spi = true;

    bma400->sensor.s_itf.si_dev = &bma400->spi_node.bnode.odev;
    bus_node_set_callbacks((struct os_dev *)bma400, &cbs);

    rc = bus_spi_node_create(name, &bma400->spi_node, spi_cfg, NULL);

    return rc;
}
#endif

int
bma400_create_dev(struct bma400 *bma400, const char *name,
                  const struct bma400_create_dev_cfg *cfg)
{
#if MYNEWT_VAL(BMA400_SPI_SUPPORT)
    if (cfg->node_is_spi) {
        return bma400_create_spi_sensor_dev(bma400, name, cfg);
    }
#endif
#if MYNEWT_VAL(BMA400_I2C_SUPPORT)
    if (!cfg->node_is_spi) {
        return bma400_create_i2c_sensor_dev(bma400, name, cfg);
    }
#endif
    return SYS_EINVAL;
}

#else
int
bma400_create_dev(struct bma400 *bma400, const char *name,
                  const struct bma400_create_dev_cfg *cfg)
{
    return os_dev_create((struct os_dev *)bma400, name,
                         OS_DEV_INIT_PRIMARY, 0, bma400_init, (void *)&cfg->itf);
}

#endif /* BUS_DRIVER_PRESENT */
