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

static const struct sensor_driver g_kxtj3_sensor_driver = {
    .sd_read       = kxtj3_sensor_read,
    .sd_get_config = kxtj3_sensor_get_config
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
    uint8_t reg_val = 0;

    /* set RES bit */
    if (perf_mode |= KXTJ3_PERF_MODE_LOW_POWER_8BIT) {
        reg_val |= KXTJ3_CTRL_REG1_RES;
    }

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

    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_i2c_node_create(name, node, i2c_cfg, sensor_itf);

    return rc;
}
#endif

