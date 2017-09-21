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
static double NAN = 0.0/0.0;
#endif

static uint16_t cnv_time[6] = {
    MS5837_CNV_TIME_OSR_256,
    MS5837_CNV_TIME_OSR_512,
    MS5837_CNV_TIME_OSR_1024,
    MS5837_CNV_TIME_OSR_2048,
    MS5837_CNV_TIME_OSR_4096,
    MS5837_CNV_TIME_OSR_8192
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

        databuf.spd.spd_press = bme280_compensate_pressure(itf, rawpress, &(bme280->pdd));

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
        rc = bme280_get_temperature(itf, &rawtemp);
        if (rc) {
            goto err;
        }

        databuf.std.std_temp = bme280_compensate_temperature(rawtemp, &(bme280->pdd));

        if (databuf.std.std_temp != NAN) {
            databuf.std.std_temp_is_valid = 1;
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

        databuf.shd.shd_humid = bme280_compensate_humidity(itf, rawhumid, &(bme280->pdd));

        if (databuf.shd.shd_humid != NAN) {
            databuf.shd.shd_humid_is_valid = 1;
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
 * Gets pressure
 *
 * @param pressure
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
ms5837_get_pressure(struct sensor_itf *itf, int32_t *press)
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
 * Write multiple length data to MS5837 sensor over I2C
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
static int
ms5837_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *buffer,
                uint8_t len)
{
    int rc;
    uint8_t payload[3] = {addr, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        MS5837_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(MS5837_STATS)
        STATS_INC(g_ms5837stats, write_errors);
#endif
        goto err;
    }

    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, len);
    if (rc) {
        MS5837_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, reg);
#if MYNEWT_VAL(MS5837_STATS)
        STATS_INC(g_ms5837stats, errors);
#endif
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Read multiple length data from MS5837 sensor over I2C
 *
 * @param The sensor interface
 * @param register address
 * @param variable length buffer
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
static int
ms5837_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *buffer,
               uint8_t len)
{
    int rc;
    uint8_t payload[3] = { addr, 0, 0};

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
        MS5837_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(MS5837_STATS)
        STATS_INC(g_ms5837stats, read_errors);
#endif
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = hal_i2c_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        MS5837_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, addr);
#if MYNEWT_VAL(MS5837_STATS)
        STATS_INC(g_ms5837stats, read_errors);
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
 * Reads the temperature ADC value
 *
 * @the sensor interface
 * @param temperature in Deg C
 *
 * @return 0 on success, non-zero on failure
 */
static int
ms5837_get_temperature(struct sensor_itf *itf, float *temperature)
{
    uint32_t rawtemp, rawpress;
    int32_t dt, temp;
    int64_t off, sens, p, t2, off2, sens2;
    uint8_t cmd;

    /* Read temperature ADC value */
    cmd = ms5837_resolution_osr * 2;
    cmd |= MS5837_START_TEMPERATURE_ADC_CONVERSION;
    rc = ms5837_get_raw_data(cmd, &rawtemp);
    if(rc) {
        goto err;
    }

    /* read pressure ADC value */
    cmd = ms5837_resolution_osr * 2;
    cmd |= MS5837_START_PRESSURE_ADC_CONVERSION;
    rc = ms5837_get_raw_data(cmd, &rawpress);
    if(rc) {
        goto err;
    }

    /* difference between actual and reference temperature = D2 - Tref */
    dt = (int32_t)rawtemp - ((int32_t)eeprom_coeff[MS5837_REFERENCE_TEMPERATURE_INDEX] << 8);

    /* actual temperature = 2000 + dt * tempsens */
    temp = 2000 + ((int64_t)dt * (int64_t)eeprom_coeff[MS5837_TEMP_COEFF_OF_TEMPERATURE_INDEX] >> 23) ;

    /* second order temperature compensation */
    if(temp < 2000)
    {
        t2 = (3 * ((int64_t)dt  * (int64_t)dt)) >> 33;
        off2 = 3 * ((int64_t)temp - 2000) * ((int64_t)temp - 2000)/2;
        sens2 = 5 * ((int64_t)temp - 2000) * ((int64_t)temp - 2000)/8;

        if(temp < -1500)
        {
            off2 += 7 * ((int64_t)temp + 1500) * ((int64_t)temp + 1500);
            sens2 += 4 * ((int64_t)temp + 1500) * ((int64_t)temp + 1500);
        }
    } else {
        t2 = ( 2 * ( (int64_t)dt  * (int64_t)dt  ) ) >> 37;
        off2 = ((int64_t)temp + 1500) * ((int64_t)temp + 1500) >> 4;
        sens2 = 0;
    }

    *temperature = ((float)temp-t2)/100;

    /* off = off_T1 + TCO * dt */
    off = ((int64_t)(eeprom_coeff[MS5837_PRESSURE_OFFSET_INDEX]) << 16) + (((int64_t)(eeprom_coeff[MS5837_TEMP_COEFF_OF_PRESSURE_OFFSET_INDEX]) * dt) >> 7);
    off -= off2;

    /* sensitivity at actual temperature = sens_T1 + TCS * dt */
    sens = ((int64_t)eeprom_coeff[MS5837_PRESSURE_SENSITIVITY_INDEX] << 15) + (((int64_t)eeprom_coeff[MS5837_TEMP_COEFF_OF_PRESSURE_SENSITIVITY_INDEX] * dt) >> 8) ;
    sens -= sens2;

    /* temperature compensated pressure = D1 * sens - off */
    p = (((rawpress * sens) >> 21) - off) >> 13;

    *pressure = (float)p/100;

    return 0;
err:
    return rc;
}


/**
 * Resets the MS5837 chip
 *
 * @return 0 on success, non-zero on failure
 */
int
ms5837_reset(struct sensor_itf *itf)
{
    uint8_t txdata;

    txdata = 0;

    return ms5837_writelen(itf, MS5837_CMD_RESET, &txdata, 0);
}


/**
 * Triggers conversion and read ADC value
 *
 * @param the sensor interface
 * @param cmd used for conversion, considers temperature, pressure and OSR
 * @param ptr to ADC value
 *
 * @return 0 on success, non-zero on failure
 */
static int
ms5837_get_raw_data(struct sensor_itf *itf, uint8_t cmd, uint32_t *data)
{
    int rc;
    uint8_t payload[4] = {cmd, 0, 0, 0};

    /* send conversion command based on OSR, temperature and pressure */
    rc = ms5837_writelen(itf, cmd, payload, 0);
    if (rc) {
        goto err;
    }

    /* delay conversion depending on resolution */
    os_time_delay((cnv_time[(cmd & MS5837_CNV_OSR_MASK)/2] + 1)/1000);

    /* read adc value */
    rc = ms5837_readlen(itf, MS5837_READ_ADC, payload, 3);
    if (rc) {
        goto err;
    }

    *data = ((uint32_t)payload[1] << 16) | ((uint32_t)payload[2] << 8) | payload[3];

    return 0;
err:
    return rc;
}


/**
 * crc4 check for MS5837 EEPROM
 *
 * @param buffer containing EEPROM coefficients
 * @param crc to compare with
 *
 * return 0 on success (CRC is OK), non-zero on failure
 */
int
ms5837_crc_check(uint16_t *prom, uint8_t crc)
{
    uint8_t cnt, bit;
    uint16_t rem, crc_read;

    rem = 0x00;
    crc_read = prom[0];
    prom[MS5837_COEFFICIENT_NUMBERS] = 0;

    /* Clear the CRC byte */
    prom[0] = (0x0FFF & prom[0]);

    for(cnt = 0; cnt < (MS5837_COEFFICIENT_NUMBERS + 1) * 2; cnt++) {
        /* Get next byte */
        if (cnt%2 == 1) {
            rem ^=  (prom[cnt>>1] & 0x00FF);
        } else {
            rem ^=  (prom[cnt>>1] >> 8);
        }

        for(bit = 8; bit > 0; bit--) {
            if(rem & 0x8000) {
                rem = (rem << 1) ^ 0x3000;
            } else {
                rem <<= 1;
            }
        }
    }

    rem >>= 12;
    prom[0] = crc_read;

    return  (rem != crc);
}
