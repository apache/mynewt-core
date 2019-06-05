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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include "os/mynewt.h"
#include "os/os_cputime.h"
#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"
#include "sensor/sensor.h"
#include "sensor/pressure.h"
#include "sensor/temperature.h"
#include "icp101xx/icp101xx.h"
#include "icp101xx_priv.h"
#include "modlog/modlog.h"
#include "stats/stats.h"
#include <syscfg/syscfg.h>

/* Define stat names for querying */
STATS_NAME_START(icp101xx_stat_section)
    STATS_NAME(icp101xx_stat_section, read_errors)
    STATS_NAME(icp101xx_stat_section, write_errors)
STATS_NAME_END(icp101xx_stat_section)

#define ICP101XX_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(ICP101XX_LOG_MODULE), __VA_ARGS__)

/* Exports for the sensor API.*/
static int icp101xx_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int icp101xx_sensor_set_config(struct sensor *, void *);
static int icp101xx_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_icp101xx_sensor_driver = {
    .sd_read               = icp101xx_sensor_read,
    .sd_set_config         = icp101xx_sensor_set_config,
    .sd_get_config         = icp101xx_sensor_get_config
};

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
icp101xx_sensor_read(struct sensor *sensor, sensor_type_t type,
                     sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc;
    struct icp101xx *icp101xx;
    union {
        struct sensor_temp_data std;
        struct sensor_press_data spd;
    } databuf;
    float temperature_degc;
    float pressure_pa;

    icp101xx = (struct icp101xx *) SENSOR_GET_DEVICE(sensor);

    if (!(type & SENSOR_TYPE_PRESSURE) &&
        !(type & SENSOR_TYPE_TEMPERATURE)) {
        rc = SYS_EINVAL;
        goto err;
    }
    
    memset(&databuf, 0, sizeof(databuf));
    
    rc = icp101xx_get_data(icp101xx, &(icp101xx->cfg), &temperature_degc, &pressure_pa);
    if (rc) {
        goto err;
    }
    if (icp101xx->cfg.skip_first_data) {
        icp101xx->cfg.skip_first_data = 0;
        ICP101XX_LOG(DEBUG, "Skip 1rst data. Measurement not yet started.\n");
        goto err;
    }

    if (type & SENSOR_TYPE_PRESSURE) {
        databuf.spd.spd_press_is_valid = 1;
        databuf.spd.spd_press = pressure_pa;
        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf, SENSOR_TYPE_PRESSURE);
        if (rc) {
            goto err;
        }
    }
    if (type & SENSOR_TYPE_TEMPERATURE) {
        databuf.std.std_temp_is_valid = 1;
        databuf.std.std_temp = temperature_degc;
        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf, SENSOR_TYPE_TEMPERATURE);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

/**
 * Send a new configuration register set to the sensor.
 *
 * @param sensor Ptr to the sensor-specific stucture
 * @param cfg Ptr to the sensor-specific configuration structure
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
icp101xx_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct icp101xx* icp101xx = (struct icp101xx *)SENSOR_GET_DEVICE(sensor);
    
    return icp101xx_config(icp101xx, (struct icp101xx_cfg*)cfg);
}

/**
 * Get the configuration of the sensor for the sensor type.  This includes
 * the value type of the sensor.
 *
 * @param sensor Ptr to the sensor
 * @param type The type of sensor value to get configuration for
 * @param cfg A pointer to the sensor value to place the returned result into.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static int
icp101xx_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                           struct sensor_cfg *cfg)
{
    int rc;

    if ((type != SENSOR_TYPE_PRESSURE) &&
        (type != SENSOR_TYPE_TEMPERATURE)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;

    return 0;
err:
    return rc;
}

int
icp101xx_write_reg(struct sensor_itf *itf, uint16_t reg, uint8_t *buf,
                   uint8_t len)
{
    int rc;
    uint8_t payload[7] = {(reg & 0xFF00) >> 8, reg & 0x00FF, 0, 0, 0, 0, 0};

    if (len > (sizeof(payload) - 2)) {
        return OS_EINVAL;
    }

    if(len > 0) {
        memcpy(&payload[2], buf, sizeof(uint8_t)*len);
    }
    
    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = len+2,
        .buffer = payload
    };

    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC, 1,
                           MYNEWT_VAL(ICP101XX_I2C_RETRIES));
    if (rc) {
        ICP101XX_LOG(ERROR,
                     "Failed to write to 0x%02X:0x%02X\n",
                      data_struct.address, reg);
    }

    return rc;
}

int
icp101xx_read_reg(struct sensor_itf *itf, uint16_t reg, uint8_t *buf,
                  uint8_t len)
{
    int rc;
    uint8_t payload[11] = {(reg & 0xFF00) >> 8, reg & 0x00FF,
                           0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    if (sizeof(payload) < len) {
        return SYS_EINVAL;
    }

    /* Clear the supplied buffer */
    memset(buf, 0, len);

    rc = sensor_itf_lock(itf, MYNEWT_VAL(ICP101XX_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    /* Register write */
    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10,
                           1, MYNEWT_VAL(ICP101XX_I2C_RETRIES));
    if (rc) {
        ICP101XX_LOG(ERROR, "I2C access failed at address 0x%02X\n",
                     data_struct.address);
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = i2cn_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10,
                          1, MYNEWT_VAL(ICP101XX_I2C_RETRIES));
    if (rc) {
        ICP101XX_LOG(ERROR, "Failed to read from 0x%02X:0x%02X\n",
                     data_struct.address, reg);
    }

    /* Copy the I2C results into the supplied buffer */
    memcpy(buf, payload, len);

err:
    sensor_itf_unlock(itf);

    return rc;
}

int
icp101xx_read(struct sensor_itf *itf, uint8_t * buf, uint32_t len)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = buf
    };

    /* Clear the supplied buffer */
    memset(buf, 0, len);

    rc = sensor_itf_lock(itf, MYNEWT_VAL(ICP101XX_ITF_LOCK_TMO));
    if (rc) {
        goto err;
    }
    
    /* Read len bytes back */
    data_struct.len = len;
    rc = i2cn_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10,
                          1, MYNEWT_VAL(ICP101XX_I2C_RETRIES));
    if (rc) {
        ICP101XX_LOG(ERROR, "Failed to read %d bytes from 0x%x\n", 
                     data_struct.address);
    }

err:
    sensor_itf_unlock(itf);

    return rc;
}

static int
default_cfg(struct icp101xx_cfg *cfg)
{
    cfg->skip_first_data = 1;
    cfg->measurement_mode = ICP101XX_MEAS_LOW_NOISE_P_FIRST;

    cfg->p_Pa_calib[0] = 45000.0;
    cfg->p_Pa_calib[1] = 80000.0;
    cfg->p_Pa_calib[2] = 105000.0;
    cfg->LUT_lower = 3.5 * (1 << 20);
    cfg->LUT_upper = 11.5 * (1 << 20);
    cfg->quadr_factor = 1 / 16777216.0;
    cfg->offst_factor = 2048.0;
    return 0;
}

static uint8_t
compute_crc(uint8_t *frame)
{
    uint8_t crc = ICP101XX_CRC8_INIT;
    uint8_t current_byte;
    uint8_t bit;

    /* Calculates 8-bit checksum with given polynomial. */
    for (current_byte = 0; current_byte < ICP101XX_RESP_DWORD_LEN; ++current_byte)
    {
        crc ^= (frame[current_byte]);
        for (bit = 8; bit > 0; --bit)
        {
            if (crc & 0x80) {
                crc = (crc << 1) ^ ICP101XX_CRC8_POLYNOM;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

static bool
check_crc(uint8_t *frame)
{
    uint8_t crc = compute_crc(frame);

    if (crc != frame[ICP101XX_RESP_FRAME_LEN - 1]) {
        ICP101XX_LOG(ERROR, "CRC computed 0x%x doesn't match 0x%x\n", crc,
                     frame[ICP101XX_RESP_FRAME_LEN - 1]);
    }
    return (crc == frame[ICP101XX_RESP_FRAME_LEN - 1]);
}

static int
read_otp(struct icp101xx *icp101xx, struct icp101xx_cfg *cfg, int16_t *calibration_data)
{
    struct sensor_itf *itf;
    uint8_t data_write[10];
    uint8_t data_read[10] = {0};
    uint8_t crc;
    int rc;
    int i;

    itf = SENSOR_GET_ITF(&(icp101xx->sensor));

    /* OTP Read mode */
    data_write[0] = (ICP101XX_OTP_READ_ADDR & 0xFF00) >> 8;
    data_write[1] = ICP101XX_OTP_READ_ADDR & 0x00FF;
    crc = compute_crc(data_write);
    
    data_write[2] = crc;
    rc = icp101xx_write_reg(itf, ICP101XX_CMD_SET_CAL_PTR, data_write, 3);
    if (rc) {
        STATS_INC(icp101xx->stats, write_errors);
        return rc;
    }

    /* Read OTP values */
    for (i = 0; i < 4; i++) {
        rc = icp101xx_read_reg(itf, ICP101XX_CMD_INC_CAL_PTR, data_read, 3);
        if (rc) {
            STATS_INC(icp101xx->stats, read_errors);
            return rc;
        }

        calibration_data[i] = data_read[0] << 8 | data_read[1];
        /* Check CRC */
        if (!check_crc(&data_read[0])) {
            return SYS_EINVAL;
        }
    }

    ICP101XX_LOG(DEBUG, "OTP : %d %d %d %d\n", 
                 calibration_data[0], calibration_data[1], calibration_data[2],
                 calibration_data[3]);
    
    /* Keep track of the OTP values in cfg */
    for (i = 0; i < 4; i++) {
        cfg->sensor_constants[i] = (float)calibration_data[i];
    }
    
    return 0;
}

static int
send_measurement_command(struct icp101xx *icp101xx, struct icp101xx_cfg *cfg)
{
    struct sensor_itf *itf;
    int rc = 0;
    uint16_t reg;

    itf = SENSOR_GET_ITF(&(icp101xx->sensor));

    switch (cfg->measurement_mode) {
        case ICP101XX_MEAS_LOW_POWER_P_FIRST:
            reg = ICP101XX_CMD_MEAS_LOW_POWER_P_FIRST;
            break;
        case ICP101XX_MEAS_LOW_POWER_T_FIRST:
            reg = ICP101XX_CMD_MEAS_LOW_POWER_T_FIRST;
            break;
        case ICP101XX_MEAS_NORMAL_P_FIRST:
            reg = ICP101XX_CMD_MEAS_NORMAL_P_FIRST;
            break;
        case ICP101XX_MEAS_NORMAL_T_FIRST:
            reg = ICP101XX_CMD_MEAS_NORMAL_T_FIRST;
            break;
        case ICP101XX_MEAS_LOW_NOISE_P_FIRST:
            reg = ICP101XX_CMD_MEAS_LOW_NOISE_P_FIRST;
            break;
        case ICP101XX_MEAS_LOW_NOISE_T_FIRST:
            reg = ICP101XX_CMD_MEAS_LOW_NOISE_T_FIRST;
            break;
        case ICP101XX_MEAS_ULTRA_LOW_NOISE_P_FIRST:
            reg = ICP101XX_CMD_MEAS_ULTRA_LOW_NOISE_P_FIRST;
            break;
        case ICP101XX_MEAS_ULTRA_LOW_NOISE_T_FIRST:
            reg = ICP101XX_CMD_MEAS_ULTRA_LOW_NOISE_T_FIRST;
            break;
        default:
            return SYS_EINVAL;
    }
    /* Send Measurement Command */
    rc = icp101xx_write_reg(itf, reg, NULL, 0);
    if (rc) {
        STATS_INC(icp101xx->stats, write_errors);
    }

    return rc;
}

static int
read_raw_data(struct icp101xx *icp101xx, struct icp101xx_cfg *cfg, int32_t *raw_pressure,
              int32_t *raw_temperature)
{
    struct sensor_itf *itf;
    uint8_t data_read[10] = {0};
    int rc;

    itf = SENSOR_GET_ITF(&(icp101xx->sensor));

    if (0 == icp101xx->cfg.skip_first_data) {
        /* Read previous measure */
        rc = icp101xx_read(itf, data_read, 9);
        if (rc) {
            STATS_INC(icp101xx->stats, read_errors);
            goto err;
        }
        
        /* Check CRC */
        if (!check_crc(&data_read[0]) ||
            !check_crc(&data_read[3]) ||
            !check_crc(&data_read[6])) {
            rc = SYS_EINVAL;
            goto err;
        }
        
        switch (cfg->measurement_mode) {
            case ICP101XX_MEAS_LOW_POWER_P_FIRST:
            case ICP101XX_MEAS_NORMAL_P_FIRST:
            case ICP101XX_MEAS_LOW_NOISE_P_FIRST:
            case ICP101XX_MEAS_ULTRA_LOW_NOISE_P_FIRST:
                /* Read P first */
                /* Pressure */
                *raw_pressure = (data_read[0] << (8*2) | data_read[1] << (8*1) | 
                                 data_read[3] << (8*0));
                /* don't care of data_read[4] because LLSB must be disregarded */
                /* don't care of data_read[2] & data_read[5] because it's crc */
                /* Temperature */
                *raw_temperature = data_read[6] << 8 | data_read[7];
                /* don't care of data_read[8] because it's crc */
                break;
            
            case ICP101XX_MEAS_LOW_POWER_T_FIRST:
            case ICP101XX_MEAS_NORMAL_T_FIRST:
            case ICP101XX_MEAS_LOW_NOISE_T_FIRST:
            case ICP101XX_MEAS_ULTRA_LOW_NOISE_T_FIRST:
                /* Read T first */
                /* Temperature */
                *raw_temperature = data_read[0] << 8 | data_read[1];
                /* don't care of data_read[2] because it's crc */
                /* Pressure */
                *raw_pressure = (data_read[3] << (8*2) | data_read[4] << (8*1) |
                                 data_read[6] << (8*0));
                /* don't care of data_read[7] because LLSB must be disregarded */
                /* don't care of data_read[5] & data_read[8] because it's crc */
                break;
            
            default:
                rc = SYS_ENOTSUP;
                goto err;
        }
    }

    /* Restart next measurement */
    rc = send_measurement_command(icp101xx, cfg);
    if (rc) {
        goto err;
    }
err:
    return rc;
}

/* p_Pa -- List of 3 values corresponding to applied pressure in Pa */
/* p_LUT -- List of 3 values corresponding to the measured p_LUT values
    at the applied pressures. */
static void
calculate_conversion_constants(float *p_Pa, float *p_LUT, float *out)
{
    float A,B,C;

    C = (p_LUT[0] * p_LUT[1] * (p_Pa[0] - p_Pa[1]) +
        p_LUT[1] * p_LUT[2] * (p_Pa[1] - p_Pa[2]) +
        p_LUT[2] * p_LUT[0] * (p_Pa[2] - p_Pa[0])) /
        (p_LUT[2] * (p_Pa[0] - p_Pa[1]) +
        p_LUT[0] * (p_Pa[1] - p_Pa[2]) +
        p_LUT[1] * (p_Pa[2] - p_Pa[0]));
    A = (p_Pa[0] * p_LUT[0] - p_Pa[1] * p_LUT[1] - (p_Pa[1] - p_Pa[0]) * C) /
        (p_LUT[0] - p_LUT[1]);
    B = (p_Pa[0] - A) * (p_LUT[0] + C);

    out[0] = A;
    out[1] = B;
    out[2] = C;
}

static int
process_data(struct icp101xx_cfg *cfg, int32_t raw_press, int32_t raw_temp,
             float * pressure, float * temperature)
{
    float t;
    float s1,s2,s3;
    float in[3];
    float out[3];
    float A,B,C;

    /* Convert pressure (depends on temperature value) */
    t = (float)(raw_temp - 32768);
    s1 = (cfg->LUT_lower + (float)(cfg->sensor_constants[0] * t * t) *
          cfg->quadr_factor);
    s2 = (cfg->offst_factor * cfg->sensor_constants[3] +
          (float)(cfg->sensor_constants[1] * t * t) * cfg->quadr_factor);
    s3 = (cfg->LUT_upper + (float)(cfg->sensor_constants[2] * t * t) *
          cfg->quadr_factor);
    in[0] = s1;
    in[1] = s2;
    in[2] = s3;

    calculate_conversion_constants(cfg->p_Pa_calib, in, out);
    A = out[0];
    B = out[1];
    C = out[2];
    *pressure = A + B / (C + raw_press);
    
    /* Convert temperature */
    *temperature = -45.f + 175.f/65536.f * raw_temp;

    return 0;
}

int
icp101xx_init(struct os_dev *dev, void *arg)
{
    struct icp101xx *icp101xx;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    icp101xx = (struct icp101xx *) dev;

    rc = default_cfg(&(icp101xx->cfg));
    if (rc) {
        goto err;
    }

    sensor = &icp101xx->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(((struct icp101xx *)dev)->stats),
        STATS_SIZE_INIT_PARMS(((struct icp101xx *)dev)->stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(icp101xx_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(((struct icp101xx *)dev)->stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
    
    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the pressure driver */
    rc = sensor_set_driver(sensor, (SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE),
                           (struct sensor_driver *) &g_icp101xx_sensor_driver);
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

    return (0);
err:
    return (rc);
}

int
icp101xx_config(struct icp101xx *icp101xx, struct icp101xx_cfg *cfg)
{
    int rc;
    uint8_t id;
    int16_t otp[4];

    /* Reset sensor */
    icp101xx_soft_reset(icp101xx);

    /* Check if we can read the chip address */
    rc = icp101xx_get_whoami(icp101xx, &id);
    if (rc) {
        goto err;
    }

    if (id != ICP101XX_ID) {
        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        rc = icp101xx_get_whoami(icp101xx, &id);
        if (rc) {
            goto err;
        }

        if (id != ICP101XX_ID) {
            ICP101XX_LOG(ERROR, "Bad chip id : %04X\n", id);
            rc = SYS_EINVAL;
            goto err;
        }
    }

    rc = read_otp(icp101xx, &(icp101xx->cfg), otp);
    if (rc) {
        goto err;
    }

    rc = sensor_set_type_mask(&(icp101xx->sensor), cfg->bc_mask);
    if (rc) {
        goto err;
    }

    icp101xx->cfg.bc_mask = cfg->bc_mask;
    icp101xx->cfg.measurement_mode = cfg->measurement_mode;

    return 0;
err:
    return rc;
}

int
icp101xx_get_whoami(struct icp101xx *icp101xx, uint8_t * whoami)
{
    int rc;
    uint16_t reg_value;
    uint8_t data_read[3] = {0};
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(icp101xx->sensor));

    /* Read ID of pressure sensor */
    rc = icp101xx_read_reg(itf, ICP101XX_CMD_READ_ID, data_read, 3);
    if (rc) {
        STATS_INC(icp101xx->stats, read_errors);
        goto err;
    }

    reg_value = data_read[0] << 8 | data_read[1];
    /* Get only ICP-101xx-specific product code bits */
    reg_value &= ICP101XX_PRODUCT_SPECIFIC_BITMASK;
    /* Check CRC */
    if (!check_crc(&data_read[0])) {
        rc = SYS_EINVAL;
        goto err;
    }

    *whoami = (uint8_t)reg_value;

    return 0;
err:
    return rc;
}

int
icp101xx_soft_reset(struct icp101xx *icp101xx)
{
    struct sensor_itf *itf;
    int rc = 0;

    itf = SENSOR_GET_ITF(&(icp101xx->sensor));

    /* Send Soft Reset Command */
    rc = icp101xx_write_reg(itf, ICP101XX_CMD_SOFT_RESET, NULL, 0);
    if (rc) {
        STATS_INC(icp101xx->stats, write_errors);
    }
    os_cputime_delay_usecs(170);

    return rc;
}

int
icp101xx_get_data(struct icp101xx *icp101xx, struct icp101xx_cfg *cfg,
                  float * temperature, float * pressure)
{
    int rc;
    int32_t raw_press, raw_temp;
    float pressure_pa;
    float temperature_degc;

    rc = read_raw_data(icp101xx, cfg, &raw_press, &raw_temp);
    if (rc) {
        goto err;
    }
    /* ICP101XX_LOG(DEBUG, "raw P = %d\n", raw_press); */
    /* ICP101XX_LOG(DEBUG, "raw T = %d\n", raw_temp); */

    rc = process_data(cfg, raw_press, raw_temp,
                      &pressure_pa, &temperature_degc);
    if (rc == 0) {
        *pressure = pressure_pa;
        *temperature = temperature_degc;
    }

err:
    return rc;
}
