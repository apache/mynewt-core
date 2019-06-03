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
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#else
#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"
#endif
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "kxtj3/kxtj3.h"
#include "kxtj3_priv.h"
#include "hal/hal_gpio.h"
#include "modlog/modlog.h"
#include "stats/stats.h"
#include <syscfg/syscfg.h>

#define KXTJ3_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(KXTJ3_LOG_MODULE), __VA_ARGS__)

/* Exports for the sensor API.*/
static int kxtj3_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int kxtj3_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);
static int kxtj3_sensor_set_config(struct sensor *, void *);
static int kxtj3_sensor_set_notification(struct sensor *,
                                         sensor_event_type_t);
static int kxtj3_sensor_unset_notification(struct sensor *,
                                           sensor_event_type_t);
static int kxtj3_sensor_handle_interrupt(struct sensor *);

static const struct sensor_driver g_kxtj3_sensor_driver = {
    .sd_read       = kxtj3_sensor_read,
    .sd_get_config = kxtj3_sensor_get_config,
    .sd_set_config = kxtj3_sensor_set_config,
    .sd_set_notification   = kxtj3_sensor_set_notification,
    .sd_unset_notification = kxtj3_sensor_unset_notification,
    .sd_handle_interrupt = kxtj3_sensor_handle_interrupt,
};

static void
kxtj3_delay_ms(uint32_t delay_ms)
{
    delay_ms = (delay_ms * OS_TICKS_PER_SEC) / 1000 + 1;
    os_time_delay(delay_ms);
}

/**
 * Writes a single byte to the specified register
 *
 * @param The Sesnsor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
kxtj3_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;
    uint8_t payload[2] = {reg, value};
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write(itf->si_dev, payload, 2);
#else
    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC, 1,
                           MYNEWT_VAL(KXTJ3_I2C_RETRIES));
    if (rc) {
        KXTJ3_LOG(ERROR,
                   "Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                   data_struct.address, reg, value);
    }
#endif
    return rc;
}

/**
 * Read data from the sensor of variable length (MAX: 8 bytes)
 *
 * @param The Sensor interface
 * @param Register to read from
 * @param Bufer to read into
 * @param Length of the buffer
 *
 * @return 0 on success and non-zero on failure
 */
static int
kxtj3_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
              uint8_t len)
{
    int rc;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = bus_node_simple_write_read_transact(itf->si_dev, &reg, 1, buffer, len);
#else
    uint8_t payload[23] = {reg, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    /* Clear the supplied buffer */
    memset(buffer, 0, len);

    rc = sensor_itf_lock(itf, MYNEWT_VAL(KXTJ3_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    /* Register write */
    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(KXTJ3_I2C_RETRIES));
    if (rc) {
        KXTJ3_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                   data_struct.address);
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = i2cn_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                          MYNEWT_VAL(KXTJ3_I2C_RETRIES));
    if (rc) {
        KXTJ3_LOG(ERROR, "Failed to read from 0x%02X:0x%02X\n",
                   data_struct.address, reg);
    }

    /* Copy the I2C results into the supplied buffer */
    memcpy(buffer, payload, len);

err:
    sensor_itf_unlock(itf);
#endif

    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @param The Sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
kxtj3_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;
    rc = kxtj3_readlen(itf, reg, value, 1);
    return rc;
}

/**
 * Get chip ID/WAI from the sensor
 *
 * @param The sensor interface
 * @param Pointer to the variable to fill up chip ID in
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_get_chip_id(struct sensor_itf *itf, uint8_t *id)
{
    int rc;
    uint8_t idtmp;

    /* Check if we can read the chip address */
    rc = kxtj3_read8(itf, KXTJ3_WHO_AM_I, &idtmp);
    if (rc) {
        goto err;
    }

    *id = idtmp;

    return 0;
err:
    return rc;
}

/**
 * Sets sensor operating mode (PC bit on/off)
 *
 * @param The sensor interface
 * @param Operation mode for the sensor
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_oper_mode(struct sensor_itf *itf,
                    enum kxtj3_oper_mode oper_mode)
{
    int rc;
    uint8_t reg_val;

    /* read CTRL_REG1 and set/clear only the PC bit */
    rc = kxtj3_read8(itf, KXTJ3_CTRL_REG1, &reg_val);
    if (rc) {
        goto err;
    }

    if (oper_mode == KXTJ3_OPER_MODE_OPERATING) {
        reg_val |= KXTJ3_CTRL_REG1_PC;
    }
    else if (oper_mode == KXTJ3_OPER_MODE_STANDBY) {
        reg_val &= ~KXTJ3_CTRL_REG1_PC;
    }
    else {
        KXTJ3_LOG(ERROR, "Incorrect oper_mode %d \n", oper_mode);
        rc = SYS_EINVAL;
        goto err;
    }

    rc = kxtj3_write8(itf, KXTJ3_CTRL_REG1, reg_val);
    if (rc) {
      goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Sets sensor ODR
 *
 * @param The sensor interface
 * @param ODR for the sensor
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_odr(struct sensor_itf *itf, enum kxtj3_odr odr)
{
    int rc;
    uint8_t reg_val;

    switch(odr) {
    case KXTJ3_ODR_1600HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_1600;
        break;
    case KXTJ3_ODR_800HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_800;
        break;
    case KXTJ3_ODR_400HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_400;
        break;
    case KXTJ3_ODR_200HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_200;
        break;
    case KXTJ3_ODR_100HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_100;
        break;
    case KXTJ3_ODR_50HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_50;
        break;
    case KXTJ3_ODR_25HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_25;
        break;
    case KXTJ3_ODR_12P5HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_12P5;
        break;
    case KXTJ3_ODR_6P25HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_6P25;
        break;
    case KXTJ3_ODR_3P125HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_3P125;
        break;
    case KXTJ3_ODR_1P563HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_1P563;
        break;
    case KXTJ3_ODR_0P781HZ:
        reg_val = KXTJ3_DATA_CTRL_REG_OSA_0P781;
        break;
    default:
        KXTJ3_LOG(ERROR, "Incorrect odr %d \n", odr);
        rc = SYS_EINVAL;
        goto err;
    }

    rc = kxtj3_write8(itf, KXTJ3_DATA_CTRL_REG, reg_val);
    if (rc) {
      goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Resets sensor
 *
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_reset_sensor(struct sensor_itf *itf)
{
    int rc;

    rc = kxtj3_write8(itf, KXTJ3_CTRL_REG2, KXTJ3_CTRL_REG2_SRST);
    if (rc) {
        goto err;
    }

    kxtj3_delay_ms(20);

    return 0;
err:
    return rc;
}

/**
 * Sets perf_mode and grange for the kxtj3 sensor
 * Note, PC bit must be zero before calling the function.
 *
 * @param The sensor interface
 * @param Performance mode for the sensor
 * @param Grange for the sensor
 * @return 0 on success, non-zero on failure
 */
int
kxtj3_set_res_and_grange(struct sensor_itf *itf,
                         enum kxtj3_perf_mode perf_mode,
                         enum kxtj3_grange grange)
{
    int rc;
    uint8_t reg_val;

    rc = kxtj3_read8(itf, KXTJ3_CTRL_REG1, &reg_val);
    if (rc) {
        goto err;
    }

    /* clear RES bit */
    reg_val &= ~KXTJ3_CTRL_REG1_RES;

    /* set RES bit */
    if (perf_mode |= KXTJ3_PERF_MODE_LOW_POWER_8BIT) {
        reg_val |= KXTJ3_CTRL_REG1_RES;
    }

    /* clear GSEL bits */
    reg_val &= ~KXTJ3_CTRL_REG1_GSEL_MASK;

    /* set GSEL bits */
    if (grange == KXTJ3_GRANGE_16G) {
        if (perf_mode == KXTJ3_PERF_MODE_HIGH_RES_14BIT) {
            reg_val |= KXTJ3_CTRL_REG1_GSEL_16G_14;
        }
        else {
            reg_val |= KXTJ3_CTRL_REG1_GSEL_16G;
        }
    }
    else if (grange == KXTJ3_GRANGE_8G) {
        if (perf_mode == KXTJ3_PERF_MODE_HIGH_RES_14BIT) {
            reg_val |= KXTJ3_CTRL_REG1_GSEL_8G_14;
        }
        else {
            reg_val |= KXTJ3_CTRL_REG1_GSEL_8G;
        }
    }
    else if (grange == KXTJ3_GRANGE_4G) {
        reg_val |= KXTJ3_CTRL_REG1_GSEL_4G;
    }
    else if (grange == KXTJ3_GRANGE_2G) {
        reg_val |= KXTJ3_CTRL_REG1_GSEL_2G;
    }
    else {
        KXTJ3_LOG(ERROR, "Incorrect grange %d \n", grange);
        rc = SYS_EINVAL;
        goto err;
    }

    rc = kxtj3_write8(itf, KXTJ3_CTRL_REG1, reg_val);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Fills kxtj3 default config
 *
 * @param Pointer to kxtj3 config
 *
 * @return 0
 */
static int
kxtj3_default_cfg(struct kxtj3_cfg *cfg)
{

    cfg->oper_mode = KXTJ3_OPER_MODE_STANDBY;
    cfg->perf_mode = KXTJ3_PERF_MODE_LOW_POWER_8BIT;
    cfg->grange = KXTJ3_GRANGE_2G;
    cfg->odr = KXTJ3_ODR_50HZ;
    cfg->wuf.odr = KXTJ3_WUF_ODR_0P781HZ;
    cfg->wuf.threshold =  STANDARD_ACCEL_GRAVITY / 2.0F; /* m/s2 */
    cfg->wuf.delay = 0.25F; /* seconds */
    cfg->int_enable = 0;
    cfg->int_polarity = 1;
    cfg->int_latch = 1;
    cfg->sensors_mask = SENSOR_TYPE_ACCELEROMETER;

    return 0;
}

/**
 * Get raw accel data from sensor
 *
 * @param The sensor ineterface
 * @param pointer to data buffer, 6 bytes
 * @return 0 on success, non-zero on error
 */
static int
kxtj3_get_raw_xyz_data(struct sensor_itf *itf, uint8_t *xyz_raw)
{
    int rc;

    rc = kxtj3_readlen(itf, KXTJ3_XOUT_L, xyz_raw, 6);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets parameters to convert raw data to sda formant
 *
 * @param Pointer to kxjt3 config
 * @param Pointer to left shift bits, filled by function
 * @param Pointer to LSB to g, filled by function
 * @return 0 on success, non-zero on error
 */
static int
kxtj3_get_conversation_params(struct kxtj3_cfg *cfg,
                              uint8_t *shift,
                              float *lsb_g)
{
    int rc;
    uint16_t counts = 0xffff;

    switch(cfg->perf_mode) {
    case KXTJ3_PERF_MODE_LOW_POWER_8BIT:
        *shift = 8;
        break;
    case KXTJ3_PERF_MODE_HIGH_RES_12BIT:
        *shift = 4;
        break;
    case KXTJ3_PERF_MODE_HIGH_RES_14BIT:
        *shift = 2;
        break;
    default:
        KXTJ3_LOG(ERROR, "Incorrect perf_mode %d \n", cfg->perf_mode);
        rc = SYS_EINVAL;
        goto err;
    }

    counts = counts >> *shift;
    counts = counts + 1;

    switch(cfg->grange) {
    case KXTJ3_GRANGE_2G:
        *lsb_g = 4.0F / counts;
        break;
    case KXTJ3_GRANGE_4G:
        *lsb_g = 8.0F / counts;
        break;
    case KXTJ3_GRANGE_8G:
        *lsb_g = 16.0F / counts;
        break;
    case KXTJ3_GRANGE_16G:
        *lsb_g = 32.0F / counts;
        break;
    default:
        KXTJ3_LOG(ERROR, "Incorrect grange %d \n", cfg->grange);
        rc = SYS_EINVAL;
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Convert raw accel data to sda fomrat
 *
 * @param Pointer to kxjt3 data struct
 * @param Pointer to accel raw data
 * @param Pointer to sda data, filled by function
 * @return 0 on success, non-zero on error
 */
static int
kxtj3_convert_raw_xyz_data_to_sda(struct kxtj3 *kxtj3,
                                  uint8_t *xyz_raw,
                                  struct sensor_accel_data *sad)
{
    int rc;
    float lsb_g;
    uint8_t shift;
    int16_t x,y,z;

    rc = kxtj3_get_conversation_params(&kxtj3->cfg, &shift, &lsb_g);
    if (rc) {
        goto err;
    }

    x = ((int16_t)xyz_raw[0]) | (((int16_t)xyz_raw[1]) << 8);
    y = ((int16_t)xyz_raw[2]) | (((int16_t)xyz_raw[3]) << 8);
    z = ((int16_t)xyz_raw[4]) | (((int16_t)xyz_raw[5]) << 8);

    x = x >> shift;
    y = y >> shift;
    z = z >> shift;

    sad->sad_x = (float)x*lsb_g*STANDARD_ACCEL_GRAVITY;
    sad->sad_y = (float)y*lsb_g*STANDARD_ACCEL_GRAVITY;
    sad->sad_z = (float)z*lsb_g*STANDARD_ACCEL_GRAVITY;

    sad->sad_x_is_valid = 1;
    sad->sad_y_is_valid = 1;
    sad->sad_z_is_valid = 1;

    return 0;
err:
    return rc;
}

/**
 * Writes wuf odr to sensor
 *
 * @param the sensor interface
 * @param wuf odr
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_wuf_odr(struct sensor_itf *itf, uint8_t odr)
{
    uint8_t reg_val;
    int rc;

    rc = kxtj3_read8(itf, KXTJ3_CTRL_REG2, &reg_val);
    if (rc) {
        return rc;
    }

    /* clear old owuf and set new */
    reg_val &= ~KXTJ3_CTRL_REG2_OWUF_MASK;
    reg_val |= odr & KXTJ3_CTRL_REG2_OWUF_MASK;

    return kxtj3_write8(itf, KXTJ3_CTRL_REG2, reg_val);
}

/**
 * Writes wuf counter to sensor
 *
 * @param the sensor interface
 * @param wuf counter
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_wuf_counter(struct sensor_itf *itf, uint8_t counter)
{
    return kxtj3_write8(itf, KXTJ3_WAKEUP_COUNTER, counter);
}

/**
 * Writes wuf threshold to sensor
 *
 * @param the sensor interface
 * @param wuf threshold
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_wuf_threshold(struct sensor_itf *itf, uint16_t threshold)
{
    uint8_t reg_val;
    int rc;

    reg_val = threshold >> 4;
    rc = kxtj3_write8(itf, KXTJ3_WAKEUP_THRESHOLD_H, reg_val);
    if (rc) {
        return rc;
    }

    reg_val = 0xF0 & (threshold << 4);
    rc = kxtj3_write8(itf, KXTJ3_WAKEUP_THRESHOLD_L, reg_val);
    if (rc) {
        return rc;
    }

    return 0;
}

/**
 * Converts wake-up config to sensor wuf registers values
 *
 * @param pointer to wake-up configuration
 * @param pointer to wuf odr, filled by function
 * @param pointer to wuf counter, filled by function
 * @param pointer wuf threshold, filled by function
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_convert_wuf_cfg_to_reg_val(struct kxtj3_wuf_cfg *cfg,
                                 uint8_t *odr_reg,
                                 uint8_t *counter_reg,
                                 uint16_t *threshold_reg)
{
    int rc = 0;
    float odr_hz;
    int value;

    switch(cfg->odr) {
    case KXTJ3_WUF_ODR_100HZ:
        *odr_reg = KXTJ3_CTRL_REG2_OWUF_100;
        odr_hz = 100.0F;
        break;
    case KXTJ3_WUF_ODR_50HZ:
        *odr_reg = KXTJ3_CTRL_REG2_OWUF_50;
        odr_hz = 50.0F;
        break;
    case KXTJ3_WUF_ODR_25HZ:
        *odr_reg = KXTJ3_CTRL_REG2_OWUF_25;
        odr_hz = 25.0F;
        break;
    case KXTJ3_WUF_ODR_12P5HZ:
        *odr_reg = KXTJ3_CTRL_REG2_OWUF_12P5;
        odr_hz = 12.5F;
        break;
    case KXTJ3_WUF_ODR_6P25HZ:
        *odr_reg = KXTJ3_CTRL_REG2_OWUF_6P25;
        odr_hz = 6.25F;
        break;
    case KXTJ3_WUF_ODR_3P125HZ:
        *odr_reg = KXTJ3_CTRL_REG2_OWUF_3P125;
        odr_hz = 3.125F;
        break;
    case KXTJ3_WUF_ODR_1P563HZ:
        *odr_reg = KXTJ3_CTRL_REG2_OWUF_1P563;
        odr_hz = 1.563F;
        break;
    case KXTJ3_WUF_ODR_0P781HZ:
        *odr_reg = KXTJ3_CTRL_REG2_OWUF_0P781;
        odr_hz = 0.781F;
        break;
    default:
        KXTJ3_LOG(ERROR, "Incorrect wuf odr %d \n", cfg->odr);
        rc = SYS_EINVAL;
    }

    if (rc) {
        return rc;
    }

    /* wuf cfg delay to kxtj3 wuf counter format */
    value = cfg->delay * odr_hz;

    /* mix/max check */
    if (value < 0) {
        value = 0;
    } else if (value > 0xff) {
        value = 0xff;
    }
    *counter_reg = value;

    /* wuf cfg threshold (in m/s2) to kxtj3 threshold format */
    value = (cfg->threshold / STANDARD_ACCEL_GRAVITY) * 256;

    /* mix/max check */
    if (value < 0) {
        value = 0;
    }
    else if (value > 0xfff) {
        value = 0xfff;
    }
    *threshold_reg = value;

    return 0;
}

/**
 * Sets sensor wake-up configuration
 *
 * @param the sensor interface
 * @param pointer to wake-up configuration
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_wuf_cfg(struct sensor_itf *itf, struct kxtj3_wuf_cfg *cfg)
{
    uint8_t odr;
    uint8_t counter;
    uint16_t threshold;
    int rc;

    rc = kxtj3_convert_wuf_cfg_to_reg_val(cfg, &odr, &counter, &threshold);
    if (rc) {
        return rc;
    }

    rc = kxtj3_set_wuf_odr(itf, odr);
    if (rc) {
        return rc;
    }

    rc = kxtj3_set_wuf_counter(itf, counter);
    if (rc) {
        return rc;
    }

    rc = kxtj3_set_wuf_threshold(itf, threshold);
    if (rc) {
        return rc;
    }

    return 0;
}

/**
 * Enable/Disable sensor wake-up function
 *
 * @param the sensor interface
 * @param interrupt response, 1 = enable, 0 = disable
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_wuf_enable(struct sensor_itf *itf, uint8_t enabled)
{
    uint8_t reg_val;
    int rc;

    rc = kxtj3_read8(itf, KXTJ3_CTRL_REG1, &reg_val);
    if (rc) {
        return rc;
    }

    if (enabled) {
        reg_val |= KXTJ3_CTRL_REG1_WUFE;
    }
    else {
        reg_val &= ~KXTJ3_CTRL_REG1_WUFE;
    }

    return kxtj3_write8(itf, KXTJ3_CTRL_REG1, reg_val);
}

/**
 * Set interrupt response
 *
 * @param the sensor interface
 * @param interrupt response, 1 = latch, 0 = pulse
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_int_response(struct sensor_itf *itf, uint8_t latch)
{
    uint8_t reg_val;
    int rc;

    rc = kxtj3_read8(itf, KXTJ3_INT_CTRL_REG1, &reg_val);
    if (rc) {
        return rc;
    }

    if (latch) {
        reg_val &= ~KXTJ3_INT_CTRL_REG1_IEL;
    }
    else {
        reg_val |= KXTJ3_INT_CTRL_REG1_IEL;
    }

    return kxtj3_write8(itf, KXTJ3_INT_CTRL_REG1, reg_val);
}

/**
 * Set interrupt polarity
 *
 * @param the sensor interface
 * @param interrupt polarity, 1 = high, 0 = low
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_int_polarity(struct sensor_itf *itf, uint8_t active_high)
{
    uint8_t reg_val;
    int rc;

    rc = kxtj3_read8(itf, KXTJ3_INT_CTRL_REG1, &reg_val);
    if (rc) {
        return rc;
    }

    if (active_high) {
        reg_val |= KXTJ3_INT_CTRL_REG1_IEA;
    }
    else {
        reg_val &= ~KXTJ3_INT_CTRL_REG1_IEA;
    }

    return kxtj3_write8(itf, KXTJ3_INT_CTRL_REG1, reg_val);
}

/**
 * Set sensor interrupt pin enable/disable
 *
 * @param the sensor interface
 * @param interrupt enabled, 1 = enabled, 0 = disabled
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_set_int_enable(struct sensor_itf *itf, uint8_t enabled)
{
    uint8_t reg_val;
    int rc;

    rc = kxtj3_read8(itf, KXTJ3_INT_CTRL_REG1, &reg_val);
    if (rc) {
        return rc;
    }

    if (enabled) {
        reg_val |= KXTJ3_INT_CTRL_REG1_IEN;
    }
    else {
        reg_val &= ~KXTJ3_INT_CTRL_REG1_IEN;
    }

    return kxtj3_write8(itf, KXTJ3_INT_CTRL_REG1, reg_val);
}

/**
 * Latch interrupt source information and sets
 * interrupt pin to inactive state
 *
 * @param the sensor interface
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_clear_int(struct sensor_itf *itf)
{
    uint8_t reg_val;
    int rc;

    rc = kxtj3_read8(itf, KXTJ3_INT_REL, &reg_val);

    return rc;
}

/**
 * Gets int source1, reports which function caused interrupt.
 *
 * @param the sensor interface
 * @param Pointer to the int_source1, filled by function
 * @return 0 on success, non-zero on failure
 */
static int
kxtj3_get_int_source1(struct sensor_itf *itf, uint8_t *int_source1)
{
    int rc;

    rc = kxtj3_read8(itf, KXTJ3_INT_SOURCE1, int_source1);

    return rc;
}

static void
init_interrupt(struct kxtj3_int *interrupt, struct sensor_int *ints)
{
    os_error_t error;

    error = os_sem_init(&interrupt->wait, 0);
    assert(error == OS_OK);

    interrupt->active = false;
    interrupt->asleep = false;
    interrupt->ints = ints;
}

static void
undo_interrupt(struct kxtj3_int *interrupt)
{
    OS_ENTER_CRITICAL(interrupt->lock);
    interrupt->active = false;
    interrupt->asleep = false;
    OS_EXIT_CRITICAL(interrupt->lock);
}

static void
wait_interrupt(struct kxtj3_int *interrupt)
{
    bool wait;

    OS_ENTER_CRITICAL(interrupt->lock);

    /* Check if we did not missed interrupt */
    if (hal_gpio_read(interrupt->ints[0].host_pin) ==
                                            interrupt->ints[0].active) {
        OS_EXIT_CRITICAL(interrupt->lock);
        return;
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
        os_error_t error;

        error = os_sem_pend(&interrupt->wait, -1);
        assert(error == OS_OK);
    }
}

static void
wake_interrupt(struct kxtj3_int *interrupt)
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

static void
kxtj3_int_irq_handler(void *arg)
{
    struct sensor *sensor = arg;
    struct kxtj3 *kxtj3;

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);

    if(kxtj3->pdd.interrupt) {
        wake_interrupt(kxtj3->pdd.interrupt);
    }

    sensor_mgr_put_interrupt_evt(sensor);
}

static int
init_intpin(struct kxtj3 *kxtj3, hal_gpio_irq_handler_t handler,
            void *arg)
{
    hal_gpio_irq_trig_t trig;
    int pin = -1;
    int rc;

    if (MYNEWT_VAL(SENSOR_MAX_INTERRUPTS_PINS) > 0) {
        /* KXTJ3 has one int pin, must be set to ints[0] */
        pin = kxtj3->sensor.s_itf.si_ints[0].host_pin;
    }

    if (pin < 0) {
        KXTJ3_LOG(ERROR, "Interrupt pin not configured\n");
        return SYS_EINVAL;
    }

    if (kxtj3->sensor.s_itf.si_ints[0].active) {
        trig = HAL_GPIO_TRIG_RISING;
    } else {
        trig = HAL_GPIO_TRIG_FALLING;
    }

    rc = hal_gpio_irq_init(pin,
                           handler,
                           arg,
                           trig,
                           HAL_GPIO_PULL_NONE);

    if (rc != 0) {
        KXTJ3_LOG(ERROR, "Failed to initialise interrupt pin %d\n", pin);
        return rc;
    }

    return 0;
}

static int
disable_interrupt(struct sensor *sensor, uint8_t int_to_disable)
{
    struct kxtj3 *kxtj3;
    struct kxtj3_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    if (int_to_disable == 0) {
        return SYS_EINVAL;
    }

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &kxtj3->pdd;

    pdd->int_enabled_bits &= ~int_to_disable;

    /* disable int pin */
    if (!pdd->int_enabled_bits) {
        hal_gpio_irq_disable(itf->si_ints[0].host_pin);
        /* disable interrupt in device */
        rc = kxtj3_set_int_enable(itf, 0);
        if (rc) {
            pdd->int_enabled_bits |= int_to_disable;
            return rc;
        }
    }

    return 0;
}

static int
enable_interrupt(struct sensor *sensor, uint8_t int_to_enable)
{
    struct kxtj3 *kxtj3;
    struct kxtj3_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    if (int_to_enable == 0) {
        rc = SYS_EINVAL;
        goto err;
    }

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &kxtj3->pdd;

    rc = kxtj3_clear_int(itf);
    if (rc) {
        goto err;
    }

    /* if no interrupts are currently in use enable int pin */
    if (!pdd->int_enabled_bits) {
        hal_gpio_irq_enable(itf->si_ints[0].host_pin);

        rc = kxtj3_set_int_enable(itf, 1);
        if (rc) {
            goto err;
        }
    }

    pdd->int_enabled_bits |= int_to_enable;

    return 0;
err:
    return rc;
}

int
kxtj3_enable_wakeup_notify(struct sensor *sensor)
{
    struct kxtj3 *kxtj3;
    struct kxtj3_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &kxtj3->pdd;

    /* Sensor config must be done on standby mode */
    rc = kxtj3_set_oper_mode(itf, KXTJ3_OPER_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    rc = enable_interrupt(&(kxtj3->sensor), KXTJ3_INT_WUFE);
    if (rc) {
        goto err;
    }

    rc = kxtj3_set_wuf_enable(itf, 1);
    if (rc) {
        goto err;
    }

    if (kxtj3->cfg.oper_mode == KXTJ3_OPER_MODE_OPERATING) {
        /* Set sensor back to operating mode */
        rc = kxtj3_set_oper_mode(itf, KXTJ3_OPER_MODE_OPERATING);
        if (rc) {
            goto err;
        }
    }

    pdd->notify_ctx.snec_evtype |= SENSOR_EVENT_TYPE_WAKEUP;

    return 0;
err:
    return rc;
}

int
kxtj3_disable_wakeup_notify(struct sensor *sensor)
{
    struct kxtj3 *kxtj3;
    struct kxtj3_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &kxtj3->pdd;

    pdd->notify_ctx.snec_evtype &= ~SENSOR_EVENT_TYPE_WAKEUP;

    /* Sensor config must be done on standby mode */
    rc = kxtj3_set_oper_mode(itf, KXTJ3_OPER_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    rc = disable_interrupt(sensor, KXTJ3_INT_WUFE);
    if (rc) {
        goto err;
    }

    rc = kxtj3_set_wuf_enable(itf, 0);
    if (rc) {
        goto err;
    }

    if (kxtj3->cfg.oper_mode == KXTJ3_OPER_MODE_OPERATING) {
        /* Set sensor back to operating mode */
        rc = kxtj3_set_oper_mode(itf, KXTJ3_OPER_MODE_OPERATING);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;

}

int
kxtj3_wait_for_wakeup(struct sensor *sensor)
{
    struct kxtj3 *kxtj3;
    struct kxtj3_pdd *pdd;
    struct sensor_itf *itf;
    int rc;

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &kxtj3->pdd;

    if (pdd->interrupt) {
        KXTJ3_LOG(ERROR, "Interrupt used\n");
        return SYS_EINVAL;
    }

    pdd->interrupt = &kxtj3->intr;

    rc = kxtj3_set_oper_mode(itf, KXTJ3_OPER_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    rc = enable_interrupt(&(kxtj3->sensor), KXTJ3_INT_WUFE);
    if (rc) {
        goto err;
    }

    rc = kxtj3_set_wuf_enable(itf, 1);
    if (rc) {
        goto err;
    }

    undo_interrupt(&kxtj3->intr);

    rc = kxtj3_set_oper_mode(itf, KXTJ3_OPER_MODE_OPERATING);
    if (rc) {
        goto err;
    }

    wait_interrupt(&kxtj3->intr);

    rc = kxtj3_set_oper_mode(itf, KXTJ3_OPER_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    rc = disable_interrupt(&(kxtj3->sensor), KXTJ3_INT_WUFE);
    if (rc) {
        goto err;
    }

    rc = kxtj3_set_wuf_enable(itf, 0);
    if (rc) {
        goto err;
    }

    if (kxtj3->cfg.oper_mode == KXTJ3_OPER_MODE_OPERATING) {
        rc = kxtj3_set_oper_mode(itf, KXTJ3_OPER_MODE_OPERATING);
        if (rc) {
            goto err;
        }
    }

    pdd->interrupt = NULL;

    return 0;
err:
    pdd->interrupt = NULL;
    return rc;
}

/**
 * Get sensor data of specific type. This function also allocates a buffer
 * to fill up the data in.
 *
 * @param Sensor structure used by the SensorAPI
 * @param Sensor type
 * @param Data function provided by the caler of the API
 * @param Argument for the data function
 * @param Timeout if any for reading
 * @return 0 on success, non-zero on error
 */
static int
kxtj3_sensor_read(struct sensor *sensor,
                  sensor_type_t type,
                  sensor_data_func_t data_func,
                  void *data_arg,
                  uint32_t timeout)
{
    int rc;
    struct sensor_itf *itf;
    struct kxtj3 *kxtj3;
    struct sensor_accel_data sad;
    uint8_t xyz_raw[6];

    if ((type & SENSOR_TYPE_ACCELEROMETER) == 0) {
        /* not supported type */
        return 0;
    }

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);

    /* read raw data from sensor */
    rc = kxtj3_get_raw_xyz_data(itf, xyz_raw);
    if (rc) {
        goto err;
    }

    /* convert raw data to sda format */
    rc = kxtj3_convert_raw_xyz_data_to_sda(kxtj3, xyz_raw, &sad);
    if (rc) {
        goto err;
    }

    /* call data function */
    rc = data_func(sensor, data_arg, &sad, SENSOR_TYPE_ACCELEROMETER);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

static int
kxtj3_sensor_set_notification(struct sensor *sensor, sensor_event_type_t event)
{
    int rc;

    if (event != SENSOR_EVENT_TYPE_WAKEUP) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = kxtj3_enable_wakeup_notify(sensor);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

static int
kxtj3_sensor_unset_notification(struct sensor *sensor, sensor_event_type_t event)
{
    int rc;

    if (event != SENSOR_EVENT_TYPE_WAKEUP) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = kxtj3_disable_wakeup_notify(sensor);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

static int
kxtj3_sensor_handle_interrupt(struct sensor *sensor)
{
    struct kxtj3 *kxtj3;
    struct kxtj3_pdd *pdd;
    struct sensor_itf *itf;
    uint8_t int_source1;
    int rc;

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &kxtj3->pdd;

    rc = kxtj3_get_int_source1(itf, &int_source1);
    if (rc != 0) {
        KXTJ3_LOG(ERROR, "Int source1 read fail rc=%d\n", rc);
        return rc;
    }

    if (int_source1 & KXTJ3_INT_SOURCE1_WUFS &&
        pdd->notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_WAKEUP) {
        sensor_mgr_put_notify_evt(&pdd->notify_ctx, SENSOR_EVENT_TYPE_WAKEUP);
    }

    rc = kxtj3_clear_int(itf);

    return rc;
}


static int
kxtj3_sensor_get_config(struct sensor *sensor,
                        sensor_type_t type,
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

static int
kxtj3_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct kxtj3 *kxtj3;

    kxtj3 = (struct kxtj3 *)SENSOR_GET_DEVICE(sensor);

    return kxtj3_config(kxtj3, (struct kxtj3_cfg*)cfg);
}

int
kxtj3_init(struct os_dev *dev, void *arg)
{
    struct kxtj3 *kxtj3;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    kxtj3 = (struct kxtj3 *) dev;

    rc = kxtj3_default_cfg(&kxtj3->cfg);
    if (rc) {
        goto err;
    }

    sensor = &kxtj3->sensor;

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the accelerometer driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER,
         (struct sensor_driver *) &g_kxtj3_sensor_driver);
    if (rc != 0) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        goto err;
    }

    init_interrupt(&kxtj3->intr, kxtj3->sensor.s_itf.si_ints);

    kxtj3->pdd.notify_ctx.snec_sensor = sensor;

    rc = init_intpin(kxtj3, kxtj3_int_irq_handler, sensor);
    if (rc) {
        return rc;
    }

    return 0;
err:
    return rc;
}

int
kxtj3_config(struct kxtj3 *kxtj3, struct kxtj3_cfg *cfg)
{
    int rc;
    uint8_t id;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(kxtj3->sensor));

    /* Check if we can read the chip address */
    rc = kxtj3_get_chip_id(itf, &id);
    if (rc) {
        goto err;
    }

    KXTJ3_LOG(INFO, "kxtj3_config kxtj3 id  0x%02X\n",id);

    if (id != KXTJ3_WHO_AM_I_WIA_ID) {
        kxtj3_delay_ms(1000);

        rc = kxtj3_get_chip_id(itf, &id);
        if (rc) {
            goto err;
        }

        if(id != KXTJ3_WHO_AM_I_WIA_ID) {
            rc = SYS_EINVAL;
            goto err;
        }
    }

    /* Reset sensor. Sensor is in standby-mode after reset */
    rc = kxtj3_reset_sensor(itf);
    if (rc) {
        goto err;
    }

    /* Set perf mode and grange */
    rc = kxtj3_set_res_and_grange(itf, cfg->perf_mode, cfg->grange);
    if (rc) {
        goto err;
    }
    kxtj3->cfg.perf_mode = cfg->perf_mode;
    kxtj3->cfg.grange = cfg->grange;

    /* Set ODR */
    rc = kxtj3_set_odr(itf, cfg->odr);
    if (rc) {
        goto err;
    }
    kxtj3->cfg.odr = cfg->odr;

    /* Set Wake-up config */
    rc = kxtj3_set_wuf_cfg(itf, &cfg->wuf);
    if (rc) {
        goto err;
    }
    kxtj3->cfg.wuf = cfg->wuf;

    /* Set interrupt config */
    rc = kxtj3_set_int_enable(itf, cfg->int_enable);
    if (rc) {
        goto err;
    }
    kxtj3->cfg.int_enable = cfg->int_enable;

    rc = kxtj3_set_int_polarity(itf, cfg->int_polarity);
    if (rc) {
        goto err;
    }
    kxtj3->cfg.int_polarity = cfg->int_polarity;

    rc = kxtj3_set_int_response(itf, cfg->int_latch);
    if (rc) {
        goto err;
    }
    kxtj3->cfg.int_latch = cfg->int_latch;

    /* Set requested oper mode */
    rc = kxtj3_set_oper_mode(itf, cfg->oper_mode);
    if (rc) {
        goto err;
    }
    kxtj3->cfg.oper_mode = cfg->oper_mode;

    rc = sensor_set_type_mask(&(kxtj3->sensor), cfg->sensors_mask);
    if (rc) {
        goto err;
    }

    kxtj3->cfg.sensors_mask = cfg->sensors_mask;

    return 0;
err:
    return rc;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    struct sensor_itf *itf = arg;

    kxtj3_init((struct os_dev *)bnode, itf);
}

int
kxtj3_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                            const struct bus_i2c_node_cfg *i2c_cfg,
                            struct sensor_itf *sensor_itf)
{
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    sensor_itf->si_dev = &node->bnode.odev;
    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_i2c_node_create(name, node, i2c_cfg, sensor_itf);

    return rc;
}
#endif

