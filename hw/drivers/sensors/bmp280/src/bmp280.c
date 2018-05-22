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
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "sensor/sensor.h"
#include "bmp280/bmp280.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "bmp280_priv.h"
#include "hal/hal_gpio.h"
#include "log/log.h"
#include "stats/stats.h"

#ifndef MATHLIB_SUPPORT
static double NAN = 0.0/0.0;
#endif

static struct hal_spi_settings spi_bmp280_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE0,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

/* Define the stats section and records */
STATS_SECT_START(bmp280_stat_section)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(invalid_data_errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(bmp280_stat_section)
    STATS_NAME(bmp280_stat_section, read_errors)
    STATS_NAME(bmp280_stat_section, write_errors)
    STATS_NAME(bmp280_stat_section, invalid_data_errors)
STATS_NAME_END(bmp280_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(bmp280_stat_section) g_bmp280stats;

#define LOG_MODULE_BMP280    (2801)
#define BMP280_INFO(...)     LOG_INFO(&_log, LOG_MODULE_BMP280, __VA_ARGS__)
#define BMP280_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_BMP280, __VA_ARGS__)
static struct log _log;

/* Exports for the sensor API */
static int bmp280_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int bmp280_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);
static int bmp280_sensor_set_config(struct sensor *, void *);

static const struct sensor_driver g_bmp280_sensor_driver = {
    .sd_read = bmp280_sensor_read,
    .sd_get_config = bmp280_sensor_get_config,
    .sd_set_config = bmp280_sensor_set_config,
};

static int
bmp280_default_cfg(struct bmp280_cfg *cfg)
{
    cfg->bc_iir = BMP280_FILTER_OFF;
    cfg->bc_mode = BMP280_MODE_NORMAL;

    cfg->bc_boc[0].boc_type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    cfg->bc_boc[0].boc_oversample = BMP280_SAMPLING_NONE;
    cfg->bc_boc[1].boc_type = SENSOR_TYPE_PRESSURE;
    cfg->bc_boc[1].boc_oversample = BMP280_SAMPLING_NONE;
    cfg->bc_s_mask = SENSOR_TYPE_ALL;

    return 0;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with bmp280
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
bmp280_init(struct os_dev *dev, void *arg)
{
    struct bmp280 *bmp280;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    bmp280 = (struct bmp280 *) dev;

    rc = bmp280_default_cfg(&bmp280->cfg);
    if (rc) {
        goto err;
    }

    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    sensor = &bmp280->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_bmp280stats),
        STATS_SIZE_INIT_PARMS(g_bmp280stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(bmp280_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_bmp280stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the driver with all the supported type */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_AMBIENT_TEMPERATURE |
                           SENSOR_TYPE_PRESSURE,
                           (struct sensor_driver *) &g_bmp280_sensor_driver);
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

    if (sensor->s_itf.si_type == SENSOR_ITF_SPI) {
        rc = hal_spi_config(sensor->s_itf.si_num, &spi_bmp280_settings);
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

    return (0);
err:
    return (rc);

}

#if MYNEWT_VAL(BMP280_SPEC_CALC)
/**
 * Compensates temperature in DegC
 * output value of "23.12" equals 23.12 DegC.
 *
 * @param uncompensated raw temperature value
 * @param per device data
 * @return compensated temperature double on success, NAN on failure
 */
static double
bmp280_compensate_temperature(int32_t rawtemp, struct bmp280_pdd *pdd)
{
    double var1, var2, comptemp;

    if (rawtemp == 0x800000) {
        BMP280_ERR("Invalid temp data\n");
        STATS_INC(g_bmp280stats, invalid_data_errors);
        return NAN;
    }

    var1 = (((double)rawtemp)/16384.0 - ((double)pdd->bcd.bcd_dig_T1)/1024.0) *
            ((double)pdd->bcd.bcd_dig_T2);
    var2 = ((((double)rawtemp)/131072.0 - ((double)pdd->bcd.bcd_dig_T1)/8192.0) *
            (((double)rawtemp)/131072.0 - ((double)pdd->bcd.bcd_dig_T1)/8192.0)) *
             ((double)pdd->bcd.bcd_dig_T3);

    pdd->t_fine = var1 + var2;

    comptemp = (var1 + var2) / 5120.0;

    return comptemp;
}

/**
 * Returns pressure in Pa as double.
 * output value of "56366.2" equals 56366.2 Pa = 563.662 hPa
 *
 * @param the sensor interface
 * @param uncompensated raw pressure value
 * @param per device data
 * @return compensated pressure on success, NAN on failure
 */
static double
bmp280_compensate_pressure(struct sensor_itf *itf, int32_t rawpress,
                           struct bmp280_pdd *pdd)
{
    double var1, var2, compp;
    int32_t temp;

    if (rawpress == 0x800000) {
        BMP280_ERR("Invalid press data\n");
        STATS_INC(g_bmp280stats, invalid_data_errors);
        return NAN;
    }

    if (!pdd->t_fine) {
        if(!bmp280_get_temperature(itf, &temp)) {
            (void)bmp280_compensate_temperature(temp, pdd);
        }
    }

    var1 = ((double)pdd->t_fine/2.0) - 64000.0;
    var2 = var1 * var1 * ((double)pdd->bcd.bcd_dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double)pdd->bcd.bcd_dig_P5) * 2.0;
    var2 = (var2/4.0)+(((double)pdd->bcd.bcd_dig_P4) * 65536.0);
    var1 = (((double)pdd->bcd.bcd_dig_P3) * var1 * var1 / 524288.0 +
            ((double)pdd->bcd.bcd_dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0)*((double)pdd->bcd.bcd_dig_P1);

    if (var1 == 0.0)
    {
        return 0;
    }

    compp = 1048576.0 - (double)rawpress;
    compp = (compp - (var2 / 4096.0)) * 6250.0 / var1;

    var1 = ((double)pdd->bcd.bcd_dig_P9) * compp * compp / 2147483648.0;
    var2 = compp * ((double)pdd->bcd.bcd_dig_P8) / 32768.0;

    compp = compp + (var1 + var2 + ((double)pdd->bcd.bcd_dig_P7)) / 16.0;

    return compp;
}

#else

/**
 * Compensates temperature in DegC
 * output value of "23.12" equals 23.12 DegC.
 *
 * @param uncompensated raw temperature value
 * @param per device data
 * @return compensated temperature double on success, NAN on failure
 */
static float
bmp280_compensate_temperature(int32_t rawtemp, struct bmp280_pdd *pdd)
{
    int32_t var1, var2, comptemp;

    if (rawtemp == 0x800000) {
        BMP280_ERR("Invalid temp data\n");
        STATS_INC(g_bmp280stats, invalid_data_errors);
        return NAN;
    }

    rawtemp >>= 4;

    var1 = ((((rawtemp>>3) - ((int32_t)pdd->bcd.bcd_dig_T1 <<1))) *
            ((int32_t)pdd->bcd.bcd_dig_T2)) >> 11;

    var2 = (((((rawtemp>>4) - ((int32_t)pdd->bcd.bcd_dig_T1)) *
              ((rawtemp>>4) - ((int32_t)pdd->bcd.bcd_dig_T1))) >> 12) *
            ((int32_t)pdd->bcd.bcd_dig_T3)) >> 14;

    pdd->t_fine = var1 + var2;

    comptemp = ((int32_t)(pdd->t_fine * 5 + 128)) >> 8;

    return (float)comptemp/100;
}

/**
 * Returns pressure in Pa as double.
 * output value of "56366.2" equals 56366.2 Pa = 563.662 hPa
 *
 * @param the sensor interface
 * @param uncompensated raw pressure value
 * @param per device data
 * @return compensated pressure on success, NAN on failure
 */
static float
bmp280_compensate_pressure(struct sensor_itf *itf, int32_t rawpress,
                           struct bmp280_pdd *pdd)
{
    int64_t var1, var2, p;
    int32_t temp;

    if (rawpress == 0x800000) {
        BMP280_ERR("Invalid pressure data\n");
        STATS_INC(g_bmp280stats, invalid_data_errors);
        return NAN;
    }

    if (!pdd->t_fine) {
        if(!bmp280_get_temperature(itf, &temp)) {
            (void)bmp280_compensate_temperature(temp, pdd);
        }
    }

    rawpress >>= 4;

    var1 = ((int64_t)pdd->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)pdd->bcd.bcd_dig_P6;
    var2 = var2 + ((int64_t)(var1*(int64_t)pdd->bcd.bcd_dig_P5) << 17);
    var2 = var2 + (((int64_t)pdd->bcd.bcd_dig_P4) << 35);
    var1 = ((int64_t)(var1 * var1 * (int64_t)pdd->bcd.bcd_dig_P3) >> 8) +
    ((int64_t)(var1 * (int64_t)pdd->bcd.bcd_dig_P2) << 12);
    var1 = (int64_t)((((((int64_t)1) << 47) + var1)) *
           ((int64_t)pdd->bcd.bcd_dig_P1)) >> 33;

    if (var1 == 0) {
        /* Avoid exception caused by division by zero */
        return 0;
    }

    p = 1048576 - rawpress;
    p = ((((int64_t)p << 31) - var2) * 3125) / var1;

    var1 = (int64_t)(((int64_t)pdd->bcd.bcd_dig_P9) * ((int64_t)p >> 13) *
           ((int64_t)p >> 13)) >> 25;
    var2 = (int64_t)(((int64_t)pdd->bcd.bcd_dig_P8) * (int64_t)p) >> 19;

    p = ((int64_t)(p + var1 + var2) >> 8) + (((int64_t)pdd->bcd.bcd_dig_P7) << 4);

    return (float)p/256;
}

#endif

static int
bmp280_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int32_t rawtemp;
    int32_t rawpress;
    struct sensor_itf *itf;
    struct bmp280 *bmp280;
    int rc;
    union {
        struct sensor_temp_data std;
        struct sensor_press_data spd;
    } databuf;

    if (!(type & SENSOR_TYPE_PRESSURE)    &&
        !(type & SENSOR_TYPE_AMBIENT_TEMPERATURE)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);

    bmp280 = (struct bmp280 *)SENSOR_GET_DEVICE(sensor);

    /*
     * For forced mode the sensor goes to sleep after setting the sensor to
     * forced mode and grabbing sensor data
     */
    if (bmp280->cfg.bc_mode == BMP280_MODE_FORCED) {
        rc = bmp280_forced_mode_measurement(itf);
        if (rc) {
            goto err;
        }
    }

    rawtemp = rawpress = 0;

    /* Get a new pressure sample */
    if (type & SENSOR_TYPE_PRESSURE) {
        rc = bmp280_get_pressure(itf, &rawpress);
        if (rc) {
            goto err;
        }

        databuf.spd.spd_press = bmp280_compensate_pressure(itf, rawpress, &(bmp280->pdd));

        if (databuf.spd.spd_press != NAN) {
            databuf.spd.spd_press_is_valid = 1;
        }

        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.spd, SENSOR_TYPE_PRESSURE);
        if (rc) {
            goto err;
        }
    }

    /* Get a new temperature sample */
    if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE) {
        rc = bmp280_get_temperature(itf, &rawtemp);
        if (rc) {
            goto err;
        }

        databuf.std.std_temp = bmp280_compensate_temperature(rawtemp, &(bmp280->pdd));

        if (databuf.std.std_temp != NAN) {
            databuf.std.std_temp_is_valid = 1;
        }

        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.std, SENSOR_TYPE_AMBIENT_TEMPERATURE);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bmp280_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (!(type & SENSOR_TYPE_PRESSURE)    ||
        !(type & SENSOR_TYPE_AMBIENT_TEMPERATURE)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;

    return (0);
err:
    return (rc);
}

static int
bmp280_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct bmp280* bmp280 = (struct bmp280 *)SENSOR_GET_DEVICE(sensor);
    
    return bmp280_config(bmp280, (struct bmp280_cfg*)cfg);
}

/**
 * Check status to see if the sensor is  reading calibration
 *
 * @param The sensor interface
 * @param ptr to indicate calibrating
 * @return 0 on success, non-zero on failure
 */
int
bmp280_is_calibrating(struct sensor_itf *itf, uint8_t *calibrating)
{
    uint8_t status;
    int rc;

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_STATUS, &status, 1);
    if (rc) {
        goto err;
    }

    *calibrating = (status & BMP280_REG_STATUS_IM_UP) != 0;

    return 0;
err:
    return rc;
}

/**
 * Get calibration info from the sensor
 *
 * @param The sensor interface
 * @param ptr to the calib data info
 * @return 0 in success, non-zero on failure
 */
static int
bmp280_get_calibinfo(struct sensor_itf *itf, struct bmp280_calib_data *bcd)
{
    int rc;
    uint8_t payload[24];

    /**
     *------------|------------------|--------------------|
     *  trimming  |    reg addrs     |    bits            |
     *____________|__________________|____________________|
     *	dig_T1    |  0x88  |  0x89   | from 0 : 7 to 8: 15
     *	dig_T2    |  0x8A  |  0x8B   | from 0 : 7 to 8: 15
     *	dig_T3    |  0x8C  |  0x8D   | from 0 : 7 to 8: 15
     *	dig_P1    |  0x8E  |  0x8F   | from 0 : 7 to 8: 15
     *	dig_P2    |  0x90  |  0x91   | from 0 : 7 to 8: 15
     *	dig_P3    |  0x92  |  0x93   | from 0 : 7 to 8: 15
     *	dig_P4    |  0x94  |  0x95   | from 0 : 7 to 8: 15
     *	dig_P5    |  0x96  |  0x97   | from 0 : 7 to 8: 15
     *	dig_P6    |  0x98  |  0x99   | from 0 : 7 to 8: 15
     *	dig_P7    |  0x9A  |  0x9B   | from 0 : 7 to 8: 15
     *	dig_P8    |  0x9C  |  0x9D   | from 0 : 7 to 8: 15
     *	dig_P9    |  0x9E  |  0x9F   | from 0 : 7 to 8: 15
     *------------|------------------|--------------------|
     */

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_DIG_T1, payload, sizeof(payload));
    if (rc) {
        goto err;
    }

    bcd->bcd_dig_T1 = (uint16_t)(payload[0] | (uint16_t)(((uint8_t)payload[1]) << 8));
    bcd->bcd_dig_T2 = (int16_t) (payload[2] | (int16_t)((int8_t)payload[3]) << 8);
    bcd->bcd_dig_T3 = (int16_t) (payload[4] | (int16_t)((int8_t)payload[5]) << 8);

    bcd->bcd_dig_P1 = (uint16_t) (payload[6] |  (uint16_t)(((uint8_t)payload[7]) << 8));
    bcd->bcd_dig_P2 = (int16_t) (payload[8]  | (int16_t)(((int8_t)payload[9])  << 8));
    bcd->bcd_dig_P3 = (int16_t) (payload[10] | (int16_t)(((int8_t)payload[11]) << 8));
    bcd->bcd_dig_P4 = (int16_t) (payload[12] | (int16_t)(((int8_t)payload[13]) << 8));
    bcd->bcd_dig_P5 = (int16_t) (payload[14] | (int16_t)(((int8_t)payload[15]) << 8));
    bcd->bcd_dig_P6 = (int16_t) (payload[16] | (int16_t)(((int8_t)payload[17]) << 8));
    bcd->bcd_dig_P7 = (int16_t) (payload[18] | (int16_t)(((int8_t)payload[19]) << 8));
    bcd->bcd_dig_P8 = (int16_t) (payload[20] | (int16_t)(((int8_t)payload[21]) << 8));
    bcd->bcd_dig_P9 = (int16_t) (payload[22] | (int16_t)(((int8_t)payload[23]) << 8));

    return 0;
err:
    return rc;
}

/**
 * Configure BMP280 sensor
 *
 * @param Sensor device BMP280 structure
 * @param Sensor device BMP280 config
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp280_config(struct bmp280 *bmp280, struct bmp280_cfg *cfg)
{
    int rc;
    uint8_t id;
    uint8_t calibrating;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(bmp280->sensor));

    /* Check if we can read the chip address */
    rc = bmp280_get_chipid(itf, &id);
    if (rc) {
        goto err;
    }

    if (id != BMP280_CHIPID) {
        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        rc = bmp280_get_chipid(itf, &id);
        if (rc) {
            goto err;
        }

        if(id != BMP280_CHIPID) {
            rc = SYS_EINVAL;
            goto err;
        }
    }

    rc = bmp280_reset(itf);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 300)/1000 + 1);

    calibrating = 1;

    while(calibrating) {
        rc = bmp280_is_calibrating(itf, &calibrating);
        if (rc) {
            goto err;
        }
    }

    rc = bmp280_get_calibinfo(itf, &(bmp280->pdd.bcd));
    if (rc) {
        goto err;
    }

    rc = bmp280_set_iir(itf, cfg->bc_iir);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 200)/1000 + 1);

    bmp280->cfg.bc_iir = cfg->bc_iir;

    rc = bmp280_set_mode(itf, cfg->bc_mode);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 200)/1000 + 1);

    bmp280->cfg.bc_mode = cfg->bc_mode;

    rc = bmp280_set_sby_duration(itf, cfg->bc_sby_dur);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 200)/1000 + 1);

    bmp280->cfg.bc_sby_dur = cfg->bc_sby_dur;

    if (cfg->bc_boc[0].boc_type) {
        rc = bmp280_set_oversample(itf, cfg->bc_boc[0].boc_type,
                                   cfg->bc_boc[0].boc_oversample);
        if (rc) {
            goto err;
        }
    }

    bmp280->cfg.bc_boc[0].boc_type = cfg->bc_boc[0].boc_type;
    bmp280->cfg.bc_boc[0].boc_oversample = cfg->bc_boc[0].boc_oversample;

    if (cfg->bc_boc[1].boc_type) {
        rc = bmp280_set_oversample(itf, cfg->bc_boc[1].boc_type,
                                   cfg->bc_boc[1].boc_oversample);
        if (rc) {
            goto err;
        }
    }

    bmp280->cfg.bc_boc[1].boc_type = cfg->bc_boc[1].boc_type;
    bmp280->cfg.bc_boc[1].boc_oversample = cfg->bc_boc[1].boc_oversample;

    os_time_delay((OS_TICKS_PER_SEC * 200)/1000 + 1);

    rc = sensor_set_type_mask(&(bmp280->sensor),  cfg->bc_s_mask);
    if (rc) {
        goto err;
    }

    bmp280->cfg.bc_s_mask = cfg->bc_s_mask;

    return 0;
err:
    return (rc);
}

/**
 * Read multiple length data from BMP280 sensor over I2C
 *
 * @param The sensor interface
 * @param register address
 * @param variable length buffer
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
bmp280_i2c_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *buffer,
                   uint8_t len)
{
    int rc;
    uint8_t payload[24] = { addr, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0 ,0 ,0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    /* Clear the supplied buffer */
    memset(buffer, 0, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 0);
    if (rc) {
        BMP280_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
        STATS_INC(g_bmp280stats, write_errors);
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = hal_i2c_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        BMP280_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, addr);
        STATS_INC(g_bmp280stats, read_errors);
        goto err;
    }

    /* Copy the I2C results into the supplied buffer */
    memcpy(buffer, payload, len);

    return 0;
err:
    return rc;
}

/**
 * Read multiple length data from BMP280 sensor over SPI
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
bmp280_spi_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                   uint8_t len)
{
    int i;
    uint16_t retval;
    int rc;

    rc = 0;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, addr | BMP280_SPI_READ_CMD_BIT);
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        BMP280_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, addr);
        STATS_INC(g_bmp280stats, read_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            BMP280_ERR("SPI_%u read failed addr:0x%02X\n",
                       itf->si_num, addr);
            STATS_INC(g_bmp280stats, read_errors);
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
 * Write multiple length data to bmp280 sensor over I2C
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
bmp280_i2c_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *buffer,
                      uint8_t len)
{
    int rc;
    int i;

    uint8_t payload[2];

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    i = 0;

    while(i < (data_struct.len - 1)) {
        payload[0] = addr + i;
        payload[1] = buffer[i];

        rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
        if (rc) {
            BMP280_ERR("Failed to write 0x%02X:0x%02X\n", data_struct.address, addr);
            STATS_INC(g_bmp280stats, write_errors);
            goto err;
        }

        i++;
    }

    return 0;
err:
    return rc;
}

/**
 * Write multiple length data to BMP280 sensor over SPI
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
bmp280_spi_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                    uint8_t len)
{
    int i;
    int rc;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, addr & ~BMP280_SPI_READ_CMD_BIT);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        BMP280_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, addr);
        STATS_INC(g_bmp280stats, write_errors);
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        rc = hal_spi_tx_val(itf->si_num, payload[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            BMP280_ERR("SPI_%u write failed addr:0x%02X\n",
                       itf->si_num, addr);
            STATS_INC(g_bmp280stats, write_errors);
            goto err;
        }
    }


    rc = 0;

err:
    /* De-select the device */
    hal_gpio_write(itf->si_cs_pin, 1);

    os_time_delay((OS_TICKS_PER_SEC * 30)/1000 + 1);

    return rc;
}

/**
 * Write multiple length data to BMP280 sensor over different interfaces
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp280_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                uint8_t len)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = bmp280_i2c_writelen(itf, addr, payload, len);
    } else {
        rc = bmp280_spi_writelen(itf, addr, payload, len);
    }

    return rc;
}

/**
 * Read multiple length data from BMP280 sensor over different interfaces
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp280_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
               uint8_t len)
{
    int rc;

    if (itf->si_type == SENSOR_ITF_I2C) {
        rc = bmp280_i2c_readlen(itf, addr, payload, len);
    } else {
        rc = bmp280_spi_readlen(itf, addr, payload, len);
    }

    return rc;
}


/**
 * Gets temperature
 *
 * @param temperature
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bmp280_get_temperature(struct sensor_itf *itf, int32_t *temp)
{
    int rc;
    uint8_t tmp[3];

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_TEMP, tmp, 3);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(BMP280_SPEC_CALC)
    *temp = (int32_t)((((uint32_t)(tmp[0])) << 12) |
                      (((uint32_t)(tmp[1])) <<  4) |
                       ((uint32_t)tmp[2] >> 4));
#else
    *temp = ((tmp[0] << 16) | (tmp[1] << 8) | tmp[2]);
#endif

    return 0;
err:
    return rc;
}

/**
 * Gets pressure
 *
 * @param pressure
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bmp280_get_pressure(struct sensor_itf *itf, int32_t *press)
{
    int rc;
    uint8_t tmp[3];

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_PRESS, tmp, 3);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(BMP280_SPEC_CALC)
    *press = (int32_t)((((uint32_t)(tmp[0])) << 12) |
                      (((uint32_t)(tmp[1])) <<  4)  |
                       ((uint32_t)tmp[2] >> 4));
#else
    *press = ((tmp[0] << 16) | (tmp[1] << 8) | tmp[2]);
#endif

    return 0;
err:
    return rc;
}

/**
 * Resets the BMP280 chip
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp280_reset(struct sensor_itf *itf)
{
    uint8_t txdata;

    txdata = 0xB6;

    return bmp280_writelen(itf, BMP280_REG_ADDR_RESET, &txdata, 1);
}

/**
 * Get IIR filter setting
 *
 * @param ptr to fill up iir setting into
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp280_get_iir(struct sensor_itf *itf, uint8_t *iir)
{
    int rc;
    uint8_t tmp;

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_CONFIG, &tmp, 1);
    if (rc) {
        goto err;
    }

    *iir = ((tmp & BMP280_REG_CONFIG_FILTER) >> 5);

    return 0;
err:
    return rc;
}

/**
 * Sets IIR filter
 *
 * @param filter setting
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp280_set_iir(struct sensor_itf *itf, uint8_t iir)
{
    int rc;
    uint8_t cfg;

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_CONFIG, &cfg, 1);
    if (rc) {
        goto err;
    }

    iir = cfg | ((iir << 5) & BMP280_REG_CONFIG_FILTER);

    rc = bmp280_writelen(itf, BMP280_REG_ADDR_CONFIG, &iir, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the operating mode
 *
 * @param ptr to the mode variable to be filled up
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bmp280_get_mode(struct sensor_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t tmp;

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_CTRL_MEAS, &tmp, 1);
    if (rc) {
        goto err;
    }

    *mode = (tmp & BMP280_REG_CTRL_MEAS_MODE);

    return 0;
err:
    return rc;
}

/**
 * Sets the operating mode
 *
 * @param mode
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bmp280_set_mode(struct sensor_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t cfg;

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_CTRL_MEAS, &cfg, 1);
    if (rc) {
        goto err;
    }

    cfg = cfg | (mode & BMP280_REG_CTRL_MEAS_MODE);

    rc = bmp280_writelen(itf, BMP280_REG_ADDR_CTRL_MEAS, &cfg, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the current sampling rate for the type of sensor
 *
 * @param Type of sensor to return sampling rate
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp280_get_oversample(struct sensor_itf *itf, sensor_type_t type,
                      uint8_t *oversample)
{
    int rc;
    uint8_t tmp;

    if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE || type & SENSOR_TYPE_PRESSURE) {
        rc = bmp280_readlen(itf, BMP280_REG_ADDR_CTRL_MEAS, &tmp, 1);
        if (rc) {
            goto err;
        }

        if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE) {
            *oversample = ((tmp & BMP280_REG_CTRL_MEAS_TOVER) >> 5);
        }

        if (type & SENSOR_TYPE_PRESSURE) {
            *oversample = ((tmp & BMP280_REG_CTRL_MEAS_POVER) >> 2);
        }
    }

    return 0;
err:
    return rc;
}

/**
 * Sets the sampling rate
 *
 * @param sensor type
 * @param sampling rate
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bmp280_set_oversample(struct sensor_itf *itf, sensor_type_t type,
                      uint8_t oversample)
{
    int rc;
    uint8_t cfg;

    if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE || type & SENSOR_TYPE_PRESSURE) {
        rc = bmp280_readlen(itf, BMP280_REG_ADDR_CTRL_MEAS, &cfg, 1);
        if (rc) {
            goto err;
        }

        if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE) {
            cfg = cfg | ((oversample << 5) & BMP280_REG_CTRL_MEAS_TOVER);
        }

        if (type & SENSOR_TYPE_PRESSURE) {
            cfg = cfg | ((oversample << 2) & BMP280_REG_CTRL_MEAS_POVER);
        }

        rc = bmp280_writelen(itf, BMP280_REG_ADDR_CTRL_MEAS, &cfg, 1);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

/**
 * Get the chip id
 *
 * @param sensor interface
 * @param ptr to fill up the chip id
 *
 * @return 0 on success, non-zero on failure
 */
int
bmp280_get_chipid(struct sensor_itf *itf, uint8_t *chipid)
{
    int rc;
    uint8_t tmp;

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_CHIPID, &tmp, 1);
    if (rc) {
        goto err;
    }

    *chipid = tmp;

    return 0;
err:
    return rc;
}

/**
 * Set the standy duration setting
 *
 * @param sensor interface
 * @param duration
 * @return 0 on success, non-zero on failure
 */
int
bmp280_set_sby_duration(struct sensor_itf *itf, uint8_t dur)
{
    int rc;
    uint8_t cfg;

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_CONFIG, &cfg, 1);
    if (rc) {
        goto err;
    }

    cfg = cfg | ((dur << 5) & BMP280_REG_CONFIG_STANDBY);

    rc = bmp280_writelen(itf, BMP280_REG_ADDR_CONFIG, &cfg, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Get the standy duration setting
 *
 * @param sensor interface
 * @param ptr to duration
 * @return 0 on success, non-zero on failure
 */
int
bmp280_get_sby_duration(struct sensor_itf *itf, uint8_t *dur)
{
    int rc;
    uint8_t tmp;

    rc = bmp280_readlen(itf, BMP280_REG_ADDR_CONFIG, &tmp, 1);
    if (rc) {
        goto err;
    }

    *dur = tmp & BMP280_REG_CONFIG_STANDBY;

    return 0;
err:
    return rc;
}

/**
 * Take forced measurement
 *
 * @param sensor interface
 * @return 0 on success, non-zero on failure
 */
int
bmp280_forced_mode_measurement(struct sensor_itf *itf)
{
    uint8_t status;
    int rc;

    /*
     * If we are in forced mode, the BME sensor goes back to sleep after each
     * measurement and we need to set it to forced mode once at this point, so
     * it will take the next measurement and then return to sleep again.
     * In normal mode simply does new measurements periodically.
     */
    rc = bmp280_set_mode(itf, BMP280_MODE_FORCED);
    if (rc) {
        goto err;
    }

    status = 1;
    while(status) {
        rc = bmp280_readlen(itf, BMP280_REG_ADDR_STATUS, &status, 1);
        if (rc) {
            goto err;
        }
        os_time_delay(OS_TICKS_PER_SEC/1000);
    }

    return 0;
err:
    return rc;
}
