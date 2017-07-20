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

#include "defs/error.h"
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "hal/hal_spi.h"
#include "sensor/sensor.h"
#include "bme280/bme280.h"
#include "sensor/humidity.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "bme280_priv.h"
#include "hal/hal_gpio.h"

#if MYNEWT_VAL(BME280_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(BME280_STATS)
#include "stats/stats.h"
#endif

#ifndef MATHLIB_SUPPORT

double NAN = 0.0/0.0;
double POS_INF = 1.0 /0.0;
double NEG_INF = -1.0/0.0;

#endif

static struct hal_spi_settings spi_bme280_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE0,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

#if MYNEWT_VAL(BME280_STATS)
/* Define the stats section and records */
STATS_SECT_START(bme280_stat_section)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(invalid_data_errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(bme280_stat_section)
    STATS_NAME(bme280_stat_section, read_errors)
    STATS_NAME(bme280_stat_section, write_errors)
    STATS_NAME(bme280_stat_section, invalid_data_errors)
STATS_NAME_END(bme280_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(bme280_stat_section) g_bme280stats;
#endif

#if MYNEWT_VAL(BME280_LOG)
#define LOG_MODULE_BME280    (280)
#define BME280_INFO(...)     LOG_INFO(&_log, LOG_MODULE_BME280, __VA_ARGS__)
#define BME280_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_BME280, __VA_ARGS__)
static struct log _log;
#else
#define BME280_INFO(...)
#define BME280_ERR(...)
#endif

/* Exports for the sensor API */
static int bme280_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int bme280_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_bme280_sensor_driver = {
    bme280_sensor_read,
    bme280_sensor_get_config
};

static int
bme280_default_cfg(struct bme280_cfg *cfg)
{
    cfg->bc_iir = BME280_FILTER_OFF;
    cfg->bc_mode = BME280_MODE_NORMAL;

    cfg->bc_boc[0].boc_type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    cfg->bc_boc[0].boc_oversample = BME280_SAMPLING_NONE;
    cfg->bc_boc[1].boc_type = SENSOR_TYPE_PRESSURE;
    cfg->bc_boc[1].boc_oversample = BME280_SAMPLING_NONE;
    cfg->bc_boc[2].boc_type = SENSOR_TYPE_RELATIVE_HUMIDITY;
    cfg->bc_boc[2].boc_oversample = BME280_SAMPLING_NONE;
    cfg->bc_s_mask = SENSOR_TYPE_ALL;

    return 0;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with bme280
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
bme280_init(struct os_dev *dev, void *arg)
{
    struct bme280 *bme280;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    bme280 = (struct bme280 *) dev;

    rc = bme280_default_cfg(&bme280->cfg);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(BME280_LOG)
    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

    sensor = &bme280->sensor;

#if MYNEWT_VAL(BME280_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_bme280stats),
        STATS_SIZE_INIT_PARMS(g_bme280stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(bme280_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_bme280stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the driver with all the supported type */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_AMBIENT_TEMPERATURE |
                           SENSOR_TYPE_PRESSURE            |
                           SENSOR_TYPE_RELATIVE_HUMIDITY,
                           (struct sensor_driver *) &g_bme280_sensor_driver);
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

    rc = hal_spi_config(sensor->s_itf.si_num, &spi_bme280_settings);
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

    return (0);
err:
    return (rc);

}

#if MYNEWT_VAL(BME280_SPEC_CALC)
/**
 * Returns temperature in DegC, as double
 * Output value of "51.23" equals 51.23 DegC.
 *
 * @param uncompensated raw temperature value
 * @param Per device data
 * @return 0 on success, non-zero on failure
 */
static double
bme280_compensate_temperature(int32_t rawtemp, struct bme280_pdd *pdd)
{
    double var1, var2, comptemp;

    if (rawtemp == 0x800000) {
        BME280_ERR("Invalid temp data\n");
#if MYNEWT_VAL(BME280_STATS)
        STATS_INC(g_bme280stats, invalid_data_errors);
#endif
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
 * Output value of "96386.2" equals 96386.2 Pa = 963.862 hPa
 *
 * @param The sensor interface
 * @param Uncompensated raw pressure value
 * @param Per device data
 * @return 0 on success, non-zero on failure
 */
static double
bme280_compensate_pressure(struct sensor_itf *itf, int32_t rawpress,
                           struct bme280_pdd *pdd)
{
    double var1, var2, p;
    int32_t temp;

    if (rawpress == 0x800000) {
        BME280_ERR("Invalid press data\n");
#if MYNEWT_VAL(BME280_STATS)
        STATS_INC(g_bme280stats, invalid_data_errors);
#endif
        return NAN;
    }

    if (!pdd->t_fine) {
        if(!bme280_get_temperature(itf, &temp)) {
            (void)bme280_compensate_temperature(temp, pdd);
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

    p = 1048576.0 - (double)rawpress;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;

    var1 = ((double)pdd->bcd.bcd_dig_P9) * p * p / 2147483648.0;
    var2 = p * ((double)pdd->bcd.bcd_dig_P8) / 32768.0;

    p = p + (var1 + var2 + ((double)pdd->bcd.bcd_dig_P7)) / 16.0;

    return p;
}

/**
 * Returns humidity in %rH as double.
 * Output value of "46.332" represents 46.332 %rH
 *
 * @param uncompensated raw humidity value
 * @param Per device data
 * @return 0 on success, non-zero on failure
 */
static double
bme280_compensate_humidity(struct sensor_itf *itf, int32_t rawhumid,
                           struct bme280_pdd *pdd)
{
    double h;
    int32_t temp;

    if (rawhumid == 0x8000) {
        BME280_ERR("Invalid humidity data\n");
#if MYNEWT_VAL(BME280_STATS)
        STATS_INC(g_bme280stats, invalid_data_errors);
#endif
        return NAN;
    }

    if (!pdd->t_fine) {
        if(!bme280_get_temperature(itf, &temp)) {
            (void)bme280_compensate_temperature(temp, pdd);
        }
    }

    h = (((double)pdd->t_fine) - 76800.0);
    h = (rawhumid - (((double)pdd->bcd.bcd_dig_H4) * 64.0 +
         ((double)pdd->bcd.bcd_dig_H5) / 16384.0 * h)) *
         (((double)pdd->bcd.bcd_dig_H2) / 65536.0 * (1.0 +
           ((double)pdd->bcd.bcd_dig_H6) / 67108864.0 * h *
          (1.0 + ((double)pdd->bcd.bcd_dig_H3) / 67108864.0 * h)));

    h = h * (1.0 - ((double)pdd->bcd.bcd_dig_H1) * h / 524288.0);
    if (h > 100.0) {
        h = 100.0;
    } else if (h < 0.0) {
        h = 0.0;
    }

    return h;
}

#else

/**
 * Returns temperature in DegC, as float
 * Output value of "51.23" equals 51.23 DegC.
 *
 * @param uncompensated raw temperature value
 * @param Per device data
 * @return 0 on success, non-zero on failure
 */
static float
bme280_compensate_temperature(int32_t rawtemp, struct bme280_pdd *pdd)
{
    int32_t var1, var2, comptemp;

    if (rawtemp == 0x800000) {
        BME280_ERR("Invalid temp data\n");
#if MYNEWT_VAL(BME280_STATS)
        STATS_INC(g_bme280stats, invalid_data_errors);
#endif
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
 * Returns pressure in Pa as float.
 * Output value of "96386.2" equals 96386.2 Pa = 963.862 hPa
 *
 * @param uncompensated raw pressure value
 * @param Per device data
 * @return 0 on success, non-zero on failure
 */
static float
bme280_compensate_pressure(struct sensor_itf *itf, int32_t rawpress,
                           struct bme280_pdd *pdd)
{
    int64_t var1, var2, p;
    int32_t temp;

    if (rawpress == 0x800000) {
        BME280_ERR("Invalid pressure data\n");
#if MYNEWT_VAL(BME280_STATS)
        STATS_INC(g_bme280stats, invalid_data_errors);
#endif
        return NAN;
    }

    if (!pdd->t_fine) {
        if(!bme280_get_temperature(itf, &temp)) {
            (void)bme280_compensate_temperature(temp, pdd);
        }
    }

    rawpress >>= 4;

    var1 = ((int64_t)pdd->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)pdd->bcd.bcd_dig_P6;
    var2 = var2 + ((int64_t)(var1*(int64_t)pdd->bcd.bcd_dig_P5) << 17);
    var2 = var2 + (((int64_t)pdd->bcd.bcd_dig_P4) << 35);
    var1 = ((int64_t)(var1 * var1 * (int64_t)pdd->bcd.bcd_dig_P3) >> 8) +
    ((int64_t)(var1 * (int64_t)pdd->bcd.bcd_dig_P2) << 12);
    var1 = (int64_t)((((((int64_t)1) << 47)+var1))*((int64_t)pdd->bcd.bcd_dig_P1)) >> 33;

    if (var1 == 0) {
        /* Avoid exception caused by division by zero */
        return 0;
    }

    p = 1048576 - rawpress;
    p = ((((int64_t)p << 31) - var2) * 3125) / var1;

    var1 = (int64_t)(((int64_t)pdd->bcd.bcd_dig_P9) * ((int64_t)p >> 13) * ((int64_t)p >> 13)) >> 25;
    var2 = (int64_t)(((int64_t)pdd->bcd.bcd_dig_P8) * (int64_t)p) >> 19;

    p = ((int64_t)(p + var1 + var2) >> 8) + (((int64_t)pdd->bcd.bcd_dig_P7) << 4);

    return (float)p/256;
}

/**
 * Returns humidity in %rH as float.
 * Output value of "46.332" represents 46.332 %rH
 *
 * @param uncompensated raw humidity value
 * @return 0 on success, non-zero on failure
 */
static float
bme280_compensate_humidity(struct sensor_itf *itf, uint32_t rawhumid,
                           struct bme280_calib_data *bcd)
{
    int32_t h;
    int32_t temp;
    int32_t tmp32;

    if (rawhumid == 0x8000) {
        BME280_ERR("Invalid humidity data\n");
#if MYNEWT_VAL(BME280_STATS)
        STATS_INC(g_bme280stats, invalid_data_errors);
#endif
        return NAN;
    }

    if (!g_t_fine) {
        if(!bme280_get_temperature(&temp)) {
            (void)bme280_compensate_temperature(temp, bcd);
        }
    }

    tmp32 = (g_t_fine - ((int32_t)76800));

    tmp32 = (((((rawhumid << 14) - (((int32_t)pdd->bcd.bcd_dig_H4) << 20) -
             (((int32_t)pdd->bcd.bcd_dig_H5) * tmp32)) + ((int32_t)16384)) >> 15) *
             (((((((tmp32 * ((int32_t)pdd->bcd.bcd_dig_H6)) >> 10) *
              (((tmp32 * ((int32_t)pdd->bcd.bcd_dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
                    ((int32_t)2097152)) * ((int32_t)pdd->bcd.bcd_dig_H2) + 8192) >> 14));

    tmp32 = (tmp32 - (((((tmp32 >> 15) * (tmp32 >> 15)) >> 7) *
                      ((int32_t)pdd->bcd.bcd_dig_H1)) >> 4));

    tmp32 = (tmp32 < 0) ? 0 : tmp32;

    tmp32 = (tmp32 > 419430400) ? 419430400 : tmp32;

    h = (tmp32 >> 12);

    return  h / 1024.0;
}

#endif

static int
bme280_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int32_t rawtemp;
    int32_t rawpress;
    int32_t rawhumid;
    struct sensor_itf *itf;
    struct bme280 *bme280;
    int rc;
    union {
        struct sensor_temp_data std;
        struct sensor_press_data spd;
        struct sensor_humid_data shd;
    } databuf;

    if (!(type & SENSOR_TYPE_PRESSURE)    &&
        !(type & SENSOR_TYPE_AMBIENT_TEMPERATURE) &&
        !(type & SENSOR_TYPE_RELATIVE_HUMIDITY)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);

    bme280 = (struct bme280 *)SENSOR_GET_DEVICE(sensor);

    /*
     * For forced mode the sensor goes to sleep after setting the sensor to
     * forced mode and grabbing sensor data
     */
    if (bme280->cfg.bc_mode == BME280_MODE_FORCED) {
        rc = bme280_forced_mode_measurement(itf);
        if (rc) {
            goto err;
        }
    }

    rawtemp = rawpress = rawhumid = 0;

    /* Get a new pressure sample */
    if (type & SENSOR_TYPE_PRESSURE) {
        rc = bme280_get_pressure(itf, &rawpress);
        if (rc) {
            goto err;
        }

        spd.spd_press = bme280_compensate_pressure(itf, rawpress, &(bme280->pdd));

        if (spd.spd_press != NAN) {
            spd.spd_press_is_valid = 1;
        }

        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.spd, SENSOR_TYPE_PRESSURE);
        if (rc) {
            goto err;
        }
    }

    /* Get a new temperature sample */
    if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE) {
        rc = bme280_get_temperature(itf, &rawtemp);
        if (rc) {
            goto err;
        }

        std.std_temp = bme280_compensate_temperature(rawtemp, &(bme280->pdd));

        if (std.std_temp != NAN) {
            std.std_temp_is_valid = 1;
        }

        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.std, SENSOR_TYPE_AMBIENT_TEMPERATURE);
        if (rc) {
            goto err;
        }
    }

    /* Get a new relative humidity sample */
    if (type & SENSOR_TYPE_RELATIVE_HUMIDITY) {
        rc = bme280_get_humidity(itf, &rawhumid);
        if (rc) {
            goto err;
        }

        shd.shd_humid = bme280_compensate_humidity(itf, rawhumid, &(bme280->pdd));

        if (shd.shd_humid != NAN) {
            shd.shd_humid_is_valid = 1;
        }

        /* Call data function */
        rc = data_func(sensor, data_arg, &databuf.shd, SENSOR_TYPE_RELATIVE_HUMIDITY);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bme280_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (!(type & SENSOR_TYPE_PRESSURE)    ||
        !(type & SENSOR_TYPE_AMBIENT_TEMPERATURE) ||
        !(type & SENSOR_TYPE_RELATIVE_HUMIDITY)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;

    return (0);
err:
    return (rc);
}

/**
 * Check status to see if the sensor is  reading calibration
 *
 * @param The sensor interface
 * @param ptr to indicate calibrating
 * @return 0 on success, non-zero on failure
 */
int
bme280_is_calibrating(struct sensor_itf *itf, uint8_t *calibrating)
{
    uint8_t status;
    int rc;

    rc = bme280_readlen(itf, BME280_REG_ADDR_STATUS, &status, 1);
    if (rc) {
        goto err;
    }

    *calibrating = (status & BME280_REG_STATUS_IM_UP) != 0;

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
bme280_get_calibinfo(struct sensor_itf *itf, struct bme280_calib_data *bcd)
{
    int rc;
    uint8_t payload[33];

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
     *	dig_H1    |       0xA1       | from 0 to 7
     *	dig_H2    |  0xE1  |  0xE2   | from 0 : 7 to 8: 15
     *	dig_H3    |       0xE3       | from 0 to 7
     *	dig_H4    |  0xE4  | 0xE5    | from 4 : 11 to 0: 3
     *	dig_H5    |  0xE5  | 0xE6    | from 0 : 3 to 4: 11
     *	dig_H6    |       0xE7       | from 0 to 7
     *------------|------------------|--------------------|
     * Hence, we read it in two transactions, one starting at
     * BME280_REG_ADDR_DIG_T1, second one starting at
     * BME280_REG_ADDR_DIG_H2.
     */

    rc = bme280_readlen(itf, BME280_REG_ADDR_DIG_T1, payload, sizeof(payload));
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

    bcd->bcd_dig_H1 = payload[25];

    memset(payload, 0, 7);
    rc = bme280_readlen(itf, BME280_REG_ADDR_DIG_H2, payload, 7);
    if (rc) {
        goto err;
    }

    bcd->bcd_dig_H2 = (int16_t) (payload[0] | (int16_t)(((int8_t)payload[1]) << 8));
    bcd->bcd_dig_H3 = payload[2];
    bcd->bcd_dig_H4 = (int16_t)(((int16_t)((int8_t)payload[3]) << 4) |
                                (payload[4] & 0x0F));
    bcd->bcd_dig_H5 = (int16_t)(((int16_t)((int8_t)payload[5]) << 4) |
                                (((int8_t)payload[6]) >> 4));
    bcd->bcd_dig_H6 = (int8_t)payload[7];

    return 0;
err:
    return rc;
}

/**
 * Configure BME280 sensor
 *
 * @param Sensor device BME280 structure
 * @param Sensor device BME280 config
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_config(struct bme280 *bme280, struct bme280_cfg *cfg)
{
    int rc;
    uint8_t id;
    uint8_t calibrating;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(bme280->sensor));

    /* Check if we can read the chip address */
    rc = bme280_get_chipid(itf, &id);
    if (rc) {
        goto err;
    }

    if (id != BME280_CHIPID && id != BMP280_CHIPID) {
        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        rc = bme280_get_chipid(itf, &id);
        if (rc) {
            goto err;
        }

        if(id != BME280_CHIPID && id != BMP280_CHIPID) {
            rc = SYS_EINVAL;
            goto err;
        }
    }

    rc = bme280_reset(itf);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 300)/1000 + 1);

    calibrating = 1;

    while(calibrating) {
        rc = bme280_is_calibrating(itf, &calibrating);
        if (rc) {
            goto err;
        }
    }

    rc = bme280_get_calibinfo(itf, &(bme280->pdd.bcd));
    if (rc) {
        goto err;
    }

    rc = bme280_set_iir(itf, cfg->bc_iir);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 200)/1000 + 1);

    bme280->cfg.bc_iir = cfg->bc_iir;

    rc = bme280_set_mode(itf, cfg->bc_mode);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 200)/1000 + 1);

    bme280->cfg.bc_mode = cfg->bc_mode;

    rc = bme280_set_sby_duration(itf, cfg->bc_sby_dur);
    if (rc) {
        goto err;
    }

    os_time_delay((OS_TICKS_PER_SEC * 200)/1000 + 1);

    bme280->cfg.bc_sby_dur = cfg->bc_sby_dur;

    if (cfg->bc_boc[0].boc_type) {
        rc = bme280_set_oversample(itf, cfg->bc_boc[0].boc_type,
                                   cfg->bc_boc[0].boc_oversample);
        if (rc) {
            goto err;
        }
    }

    bme280->cfg.bc_boc[0].boc_type = cfg->bc_boc[0].boc_type;
    bme280->cfg.bc_boc[0].boc_oversample = cfg->bc_boc[0].boc_oversample;

    if (cfg->bc_boc[1].boc_type) {
        rc = bme280_set_oversample(itf, cfg->bc_boc[1].boc_type,
                                   cfg->bc_boc[1].boc_oversample);
        if (rc) {
            goto err;
        }
    }

    bme280->cfg.bc_boc[1].boc_type = cfg->bc_boc[1].boc_type;
    bme280->cfg.bc_boc[1].boc_oversample = cfg->bc_boc[1].boc_oversample;

    if (cfg->bc_boc[2].boc_type) {
        rc = bme280_set_oversample(itf, cfg->bc_boc[2].boc_type,
                                   cfg->bc_boc[2].boc_oversample);
        if (rc) {
            goto err;
        }
    }

    bme280->cfg.bc_boc[2].boc_type = cfg->bc_boc[2].boc_type;
    bme280->cfg.bc_boc[2].boc_oversample = cfg->bc_boc[2].boc_oversample;

    os_time_delay((OS_TICKS_PER_SEC * 200)/1000 + 1);

    rc = sensor_set_type_mask(&(bme280->sensor),  cfg->bc_s_mask);
    if (rc) {
        goto err;
    }

    bme280->cfg.bc_s_mask = cfg->bc_s_mask;

    return 0;
err:
    return (rc);
}

/**
 * Read multiple length data from BME280 sensor over SPI
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
               uint8_t len)
{
    int i;
    uint16_t retval;
    int rc;

    rc = 0;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    retval = hal_spi_tx_val(itf->si_num, addr | BME280_SPI_READ_CMD_BIT);
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        BME280_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, addr);
#if MYNEWT_VAL(BME280_STATS)
        STATS_INC(g_bme280stats, read_errors);
#endif
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(itf->si_num, 0);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            BME280_ERR("SPI_%u read failed addr:0x%02X\n",
                       itf->si_num, addr);
#if MYNEWT_VAL(BME280_STATS)
            STATS_INC(g_bme280stats, read_errors);
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
 * Write multiple length data to BME280 sensor over SPI
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload,
                uint8_t len)
{
    int i;
    int rc;

    /* Select the device */
    hal_gpio_write(itf->si_cs_pin, 0);

    /* Send the address */
    rc = hal_spi_tx_val(itf->si_num, addr & ~BME280_SPI_READ_CMD_BIT);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        BME280_ERR("SPI_%u register write failed addr:0x%02X\n",
                   itf->si_num, addr);
#if MYNEWT_VAL(BME280_STATS)
        STATS_INC(g_bme280stats, write_errors);
#endif
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        rc = hal_spi_tx_val(itf->si_num, payload[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            BME280_ERR("SPI_%u write failed addr:0x%02X:0x%02X\n",
                       itf->si_num, addr);
#if MYNEWT_VAL(BME280_STATS)
            STATS_INC(g_bme280stats, write_errors);
#endif
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
 * Gets temperature
 *
 * @param temperature
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_get_temperature(struct sensor_itf *itf, int32_t *temp)
{
    int rc;
    uint8_t tmp[3];

    rc = bme280_readlen(itf, BME280_REG_ADDR_TEMP, tmp, 3);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(BME280_SPEC_CALC)
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
 * Gets humidity
 *
 * @param humidity
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_get_humidity(struct sensor_itf *itf, int32_t *humid)
{
    int rc;
    uint8_t tmp[2];

    rc = bme280_readlen(itf, BME280_REG_ADDR_HUM, tmp, 2);
    if (rc) {
        goto err;
    }
#if MYNEWT_VAL(BME280_SPEC_CALC)
    *humid = (tmp[0] << 8 | tmp[1]);
#else
    *humid = (tmp[0] << 8 | tmp[1]);
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
bme280_get_pressure(struct sensor_itf *itf, int32_t *press)
{
    int rc;
    uint8_t tmp[3];

    rc = bme280_readlen(itf, BME280_REG_ADDR_PRESS, tmp, 3);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(BME280_SPEC_CALC)
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
 * Resets the BME280 chip
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_reset(struct sensor_itf *itf)
{
    uint8_t txdata;

    txdata = 0xB6;

    return bme280_writelen(itf, BME280_REG_ADDR_RESET, &txdata, 1);
}

/**
 * Get IIR filter setting
 *
 * @param ptr to fill up iir setting into
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_get_iir(struct sensor_itf *itf, uint8_t *iir)
{
    int rc;
    uint8_t tmp;

    rc = bme280_readlen(itf, BME280_REG_ADDR_CONFIG, &tmp, 1);
    if (rc) {
        goto err;
    }

    *iir = ((tmp & BME280_REG_CONFIG_FILTER) >> 5);

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
bme280_set_iir(struct sensor_itf *itf, uint8_t iir)
{
    int rc;
    uint8_t cfg;

    rc = bme280_readlen(itf, BME280_REG_ADDR_CONFIG, &cfg, 1);
    if (rc) {
        goto err;
    }

    iir = cfg | ((iir << 5) & BME280_REG_CONFIG_FILTER);

    rc = bme280_writelen(itf, BME280_REG_ADDR_CONFIG, &iir, 1);
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
bme280_get_mode(struct sensor_itf *itf, uint8_t *mode)
{
    int rc;
    uint8_t tmp;

    rc = bme280_readlen(itf, BME280_REG_ADDR_CTRL_MEAS, &tmp, 1);
    if (rc) {
        goto err;
    }

    *mode = (tmp & BME280_REG_CTRL_MEAS_MODE);

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
bme280_set_mode(struct sensor_itf *itf, uint8_t mode)
{
    int rc;
    uint8_t cfg;

    rc = bme280_readlen(itf, BME280_REG_ADDR_CTRL_MEAS, &cfg, 1);
    if (rc) {
        goto err;
    }

    cfg = cfg | (mode & BME280_REG_CTRL_MEAS_MODE);

    rc = bme280_writelen(itf, BME280_REG_ADDR_CTRL_MEAS, &cfg, 1);
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
bme280_get_oversample(struct sensor_itf *itf, sensor_type_t type,
                      uint8_t *oversample)
{
    int rc;
    uint8_t tmp;

    if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE || type & SENSOR_TYPE_PRESSURE) {
        rc = bme280_readlen(itf, BME280_REG_ADDR_CTRL_MEAS, &tmp, 1);
        if (rc) {
            goto err;
        }

        if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE) {
            *oversample = ((tmp & BME280_REG_CTRL_MEAS_TOVER) >> 5);
        }

        if (type & SENSOR_TYPE_PRESSURE) {
            *oversample = ((tmp & BME280_REG_CTRL_MEAS_POVER) >> 2);
        }
    }

    if (type & SENSOR_TYPE_RELATIVE_HUMIDITY) {
        rc = bme280_readlen(itf, BME280_REG_ADDR_CTRL_HUM, &tmp, 1);
        if (rc) {
            goto err;
        }
        *oversample = (tmp & BME280_REG_CTRL_HUM_HOVER);
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
bme280_set_oversample(struct sensor_itf *itf, sensor_type_t type,
                      uint8_t oversample)
{
    int rc;
    uint8_t cfg;

    if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE || type & SENSOR_TYPE_PRESSURE) {
        rc = bme280_readlen(itf, BME280_REG_ADDR_CTRL_MEAS, &cfg, 1);
        if (rc) {
            goto err;
        }

        if (type & SENSOR_TYPE_AMBIENT_TEMPERATURE) {
            cfg = cfg | ((oversample << 5) & BME280_REG_CTRL_MEAS_TOVER);
        }

        if (type & SENSOR_TYPE_PRESSURE) {
            cfg = cfg | ((oversample << 2) & BME280_REG_CTRL_MEAS_POVER);
        }

        rc = bme280_writelen(itf, BME280_REG_ADDR_CTRL_MEAS, &cfg, 1);
        if (rc) {
            goto err;
        }
    }

    if (type & SENSOR_TYPE_RELATIVE_HUMIDITY) {
        rc = bme280_readlen(itf, BME280_REG_ADDR_CTRL_HUM, &cfg, 1);
        if (rc) {
            goto err;
        }

        cfg = cfg | (oversample & BME280_REG_CTRL_HUM_HOVER);

        rc = bme280_writelen(itf, BME280_REG_ADDR_CTRL_HUM, &cfg, 1);
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
bme280_get_chipid(struct sensor_itf *itf, uint8_t *chipid)
{
    int rc;
    uint8_t tmp;

    rc = bme280_readlen(itf, BME280_REG_ADDR_CHIPID, &tmp, 1);
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
bme280_set_sby_duration(struct sensor_itf *itf, uint8_t dur)
{
    int rc;
    uint8_t cfg;

    rc = bme280_readlen(itf, BME280_REG_ADDR_CONFIG, &cfg, 1);
    if (rc) {
        goto err;
    }

    cfg = cfg | ((dur << 5) & BME280_REG_CONFIG_STANDBY);

    rc = bme280_writelen(itf, BME280_REG_ADDR_CONFIG, &cfg, 1);
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
bme280_get_sby_duration(struct sensor_itf *itf, uint8_t *dur)
{
    int rc;
    uint8_t tmp;

    rc = bme280_readlen(itf, BME280_REG_ADDR_CONFIG, &tmp, 1);
    if (rc) {
        goto err;
    }

    *dur = tmp & BME280_REG_CONFIG_STANDBY;

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
bme280_forced_mode_measurement(struct sensor_itf *itf)
{
    uint8_t status;
    int rc;

    /*
     * If we are in forced mode, the BME sensor goes back to sleep after each
     * measurement and we need to set it to forced mode once at this point, so
     * it will take the next measurement and then return to sleep again.
     * In normal mode simply does new measurements periodically.
     */
    rc = bme280_set_mode(itf, BME280_MODE_FORCED);
    if (rc) {
        goto err;
    }

    status = 1;
    while(status) {
        rc = bme280_readlen(itf, BME280_REG_ADDR_STATUS, &status, 1);
        if (rc) {
            goto err;
        }
        os_time_delay(OS_TICKS_PER_SEC/1000);
    }

    return 0;
err:
    return rc;
}
