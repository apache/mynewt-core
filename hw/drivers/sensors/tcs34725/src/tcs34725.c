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

#include "defs/error.h"
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "hal/hal_i2c.h"
#include "sensor/sensor.h"
#include "tcs34725/tcs34725.h"
#include "tcs34725_priv.h"
#include "sensor/color.h"

#if MYNEWT_VAL(TCS34725_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(TCS34725_STATS)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(TCS34725_STATS)
/* Define the stats section and records */
STATS_SECT_START(tcs34725_stat_section)
    STATS_SECT_ENTRY(samples_2_4ms)
    STATS_SECT_ENTRY(samples_24ms)
    STATS_SECT_ENTRY(samples_50ms)
    STATS_SECT_ENTRY(samples_101ms)
    STATS_SECT_ENTRY(samples_154ms)
    STATS_SECT_ENTRY(samples_700ms)
    STATS_SECT_ENTRY(samples_userdef)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(tcs34725_stat_section)
    STATS_NAME(tcs34725_stat_section, samples_2_4ms)
    STATS_NAME(tcs34725_stat_section, samples_24ms)
    STATS_NAME(tcs34725_stat_section, samples_50ms)
    STATS_NAME(tcs34725_stat_section, samples_101ms)
    STATS_NAME(tcs34725_stat_section, samples_154ms)
    STATS_NAME(tcs34725_stat_section, samples_700ms)
    STATS_NAME(tcs34725_stat_section, samples_userdef)
    STATS_NAME(tcs34725_stat_section, errors)
STATS_NAME_END(tcs34725_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(tcs34725_stat_section) g_tcs34725stats;
#endif

#if MYNEWT_VAL(TCS34725_LOG)
#define LOG_MODULE_TCS34725 (307)
#define TCS34725_INFO(...)  LOG_INFO(&_log, LOG_MODULE_TCS34725, __VA_ARGS__)
#define TCS34725_ERR(...)   LOG_ERROR(&_log, LOG_MODULE_TCS34725, __VA_ARGS__)
static struct log _log;
#else
#define TCS34725_INFO(...)
#define TCS34725_ERR(...)
#endif

/* Exports for the sensor API */
static int tcs34725_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int tcs34725_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_tcs34725_sensor_driver = {
    tcs34725_sensor_read,
    tcs34725_sensor_get_config
};

/**
 * Writes a single byte to the specified register
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
tcs34725_write8(struct sensor_itf *itf, uint8_t reg, uint32_t value)
{
    int rc;
    uint8_t payload[2] = { reg | TCS34725_COMMAND_BIT, value & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        TCS34725_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                       data_struct.address, reg, value);
#if MYNEWT_VAL(TCS34725_STATS)
        STATS_INC(g_tcs34725stats, errors);
#endif
    }

    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
tcs34725_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;
    uint8_t payload;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &payload
    };

    /* Register write */
    payload = reg | TCS34725_COMMAND_BIT;
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        TCS34725_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(TCS34725_STATS)
        STATS_INC(g_tcs34725stats, errors);
#endif
        goto error;
    }

    /* Read one byte back */
    payload = 0;
    rc = hal_i2c_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    *value = payload;
    if (rc) {
        TCS34725_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, reg);
#if MYNEWT_VAL(TCS34725_STATS)
        STATS_INC(g_tcs34725stats, errors);
#endif
    }

error:
    return rc;
}

/**
 * Read data from the sensor of variable length (MAX: 8 bytes)
 *
 * @param Register to read from
 * @param Bufer to read into
 * @param Length of the buffer
 *
 * @return 0 on success and non-zero on failure
 */
int
tcs34725_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len)
{
    int rc;
    uint8_t payload[9] = { reg | TCS34725_COMMAND_BIT, 0, 0, 0, 0, 0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    /* Clear the supplied buffer */
    memset(buffer, 0, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        TCS34725_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(TCS34725_STATS)
        STATS_INC(g_tcs34725stats, errors);
#endif
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = hal_i2c_master_read(itf->si_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        TCS34725_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, reg);
#if MYNEWT_VAL(TCS34725_STATS)
        STATS_INC(g_tcs34725stats, errors);
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
 * Writes a multiple bytes to the specified register
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The data buffer to write from
 *
 * @return 0 on success, non-zero error on failure.
 */
int
tcs34725_writelen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len)
{
    int rc;
    uint8_t payload[9] = { reg, 0, 0, 0, 0, 0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        TCS34725_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(TCS34725_STATS)
        STATS_INC(g_tcs34725stats, errors);
#endif
        goto err;
    }

    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, len);

    if (rc) {
        TCS34725_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, reg);
#if MYNEWT_VAL(TCS34725_STATS)
        STATS_INC(g_tcs34725stats, errors);
#endif
        goto err;
    }

    return 0;
err:
    return rc;
}


#if MYNEWT_VAL(MATHLIB_SUPPORT)
/**
 * Float power function
 *
 * @param float base
 * @param float exponent
 */
static float
powf(float base, float exp)
{
    return (float)(pow((double)base, (double)exp));
}

#endif

/**
 *
 * Enables the device
 *
 * @param The sensor interface
 * @param enable/disable
 * @return 0 on success, non-zero on error
 */
int
tcs34725_enable(struct sensor_itf *itf, uint8_t enable)
{
    int rc;
    uint8_t reg;

    rc = tcs34725_read8(itf, TCS34725_REG_ENABLE, &reg);
    if (rc) {
        goto err;
    }

    os_time_delay((3 * OS_TICKS_PER_SEC)/1000 + 1);

    if (enable) {
        rc = tcs34725_write8(itf, TCS34725_REG_ENABLE,
                             reg | TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);
        if (rc) {
            goto err;
        }
    } else {
        rc = tcs34725_write8(itf, TCS34725_REG_ENABLE, reg &
                             ~(TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN));
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accellerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
tcs34725_init(struct os_dev *dev, void *arg)
{
    struct tcs34725 *tcs34725;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    tcs34725 = (struct tcs34725 *) dev;

    tcs34725->cfg.mask = SENSOR_TYPE_ALL;

#if MYNEWT_VAL(TCS34725_LOG)
    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

    sensor = &tcs34725->sensor;

#if MYNEWT_VAL(TCS34725_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_tcs34725stats),
        STATS_SIZE_INIT_PARMS(g_tcs34725stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(tcs34725_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register("tcs34725", STATS_HDR(g_tcs34725stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the color sensor driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_COLOR,
                           (struct sensor_driver *) &g_tcs34725_sensor_driver);
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

    rc = sensor_set_type_mask(sensor, tcs34725->cfg.mask);
    if (rc) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Indicates whether the sensor is enabled or not
 *
 * @param The sensor interface
 * @param ptr to is_enabled variable
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_get_enable(struct sensor_itf *itf, uint8_t *is_enabled)
{
    int rc;
    uint8_t tmp;

    rc = tcs34725_read8(itf, TCS34725_REG_ENABLE, &tmp);
    if (rc) {
        goto err;
    }

    *is_enabled = tmp;

    return 0;
err:
    return rc;
}

/**
 * Sets integration time
 *
 * @param The sensor interface
 * @param integration time to be set
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_set_integration_time(struct sensor_itf *itf, uint8_t int_time)
{
    int rc;

    rc = tcs34725_write8(itf, TCS34725_REG_ATIME, int_time);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets integration time set earlier
 *
 * @param The sensor interface
 * @param ptr to integration time
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_get_integration_time(struct sensor_itf *itf, uint8_t *int_time)
{
    int rc;
    uint8_t tmp;

    rc = tcs34725_read8(itf, TCS34725_REG_ATIME, &tmp);
    if (rc) {
        goto err;
    }

    *int_time = tmp;

    return 0;
err:
    return rc;
}

/**
 * Set gain of the sensor
 *
 * @param The sensor interface
 * @param gain
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_set_gain(struct sensor_itf *itf, uint8_t gain)
{
    int rc;

    if (gain > TCS34725_GAIN_60X) {
        TCS34725_ERR("Invalid gain value\n");
        rc = SYS_EINVAL;
        goto err;
    }

    rc = tcs34725_write8(itf, TCS34725_REG_CONTROL, gain);
    if (rc) {
        goto err;
    }

err:
    return rc;
}

/**
 * Get gain of the sensor
 *
 * @param The sensor interface
 * @param ptr to gain
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_get_gain(struct sensor_itf *itf, uint8_t *gain)
{
    int rc;
    uint8_t tmp;

    rc = tcs34725_read8(itf, TCS34725_REG_CONTROL, &tmp);
    if (rc) {
        goto err;
    }

    *gain = tmp;

    return 0;
err:
    return rc;
}

/**
 * Get chip ID from the sensor
 *
 * @param The sensor interface
 * @param Pointer to the variable to fill up chip ID in
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_get_chip_id(struct sensor_itf *itf, uint8_t *id)
{
    int rc;
    uint8_t idtmp;

    /* Check if we can read the chip address */
    rc = tcs34725_read8(itf, TCS34725_REG_ID, &idtmp);
    if (rc) {
        goto err;
    }

    *id = idtmp;

    return 0;
err:
    return rc;
}

/**
 * Configure the sensor
 *
 * @param ptr to the sensor
 * @param ptr to sensor config
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_config(struct tcs34725 *tcs34725, struct tcs34725_cfg *cfg)
{
    int rc;
    uint8_t id;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(tcs34725->sensor));

    rc = tcs34725_get_chip_id(itf, &id);
    if (id != TCS34725_ID || rc != 0) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = tcs34725_enable(itf, 1);
    if (rc) {
        goto err;
    }

    rc = tcs34725_set_integration_time(itf, cfg->integration_time);
    if (rc) {
        goto err;
    }

    tcs34725->cfg.integration_time = cfg->integration_time;

    rc = tcs34725_set_gain(itf, cfg->gain);
    if (rc) {
        goto err;
    }

    tcs34725->cfg.gain = cfg->gain;

    rc = tcs34725_enable_interrupt(itf, cfg->int_enable);
    if (rc) {
        goto err;
    }

    tcs34725->cfg.int_enable = cfg->int_enable;

    rc = sensor_set_type_mask(&(tcs34725->sensor), cfg->mask);
    if (rc) {
        goto err;
    }

    tcs34725->cfg.mask = cfg->mask;

    return 0;
err:
    return (rc);
}

/**
 * Reads the raw red, green, blue and clear channel values
 *
 *
 * @param The sensor interface
 * @param red value to return
 * @param green value to return
 * @param blue value to return
 * @param clear channel value
 * @param driver sturcture containing config
 */
int
tcs34725_get_rawdata(struct sensor_itf *itf, uint16_t *r, uint16_t *g,
                     uint16_t *b, uint16_t *c, struct tcs34725 *tcs34725)
{
    uint8_t payload[8] = {0};
    int rc;

    *c = *r = *g = *b = 0;

    rc = tcs34725_readlen(itf, TCS34725_REG_CDATAL, payload, 8);
    if (rc) {
        goto err;
    }

    *c = payload[1] << 8 | payload[0];
    *r = payload[3] << 8 | payload[2];
    *g = payload[5] << 8 | payload[4];
    *b = payload[7] << 8 | payload[6];

#if MYNEWT_VAL(TCS34725_STATS)
    switch (tcs34725->cfg.integration_time) {
        case TCS34725_INTEGRATIONTIME_2_4MS:
            STATS_INC(g_tcs34725stats, samples_2_4ms);
            break;
        case TCS34725_INTEGRATIONTIME_24MS:
            STATS_INC(g_tcs34725stats, samples_24ms);
            break;
        case TCS34725_INTEGRATIONTIME_50MS:
            STATS_INC(g_tcs34725stats, samples_50ms);
            break;
        case TCS34725_INTEGRATIONTIME_101MS:
            STATS_INC(g_tcs34725stats, samples_101ms);
            break;
        case TCS34725_INTEGRATIONTIME_154MS:
            STATS_INC(g_tcs34725stats, samples_154ms);
            break;
        case TCS34725_INTEGRATIONTIME_700MS:
            STATS_INC(g_tcs34725stats, samples_700ms);
        default:
            STATS_INC(g_tcs34725stats, samples_userdef);
        break;
    }

#endif

    return 0;
err:
    return rc;

}

/**
 *
 * Converts raw RGB values to color temp in deg K and lux
 *
 * @param The sensor interface
 * @param ptr to sensor color data ptr
 * @param ptr to sensor
 */
static int
tcs34725_calc_colortemp_lux(struct sensor_itf *itf,
                            struct sensor_color_data *scd,
                            struct tcs34725 *tcs34725)
{
    int rc;

    rc = 0;
#if MYNEWT_VAL(USE_TCS34725_TAOS_DN25)
    float n;
    /**
     * From the designer's notebook by TAOS:
     * Mapping sensor response  RGB values to CIE tristimulus values(XYZ)
     * based on broad enough transformation, the light sources chosen were a
     * high color temperature fluorescent (6500K), a low color temperature
     * fluorescent (3000K), and an incandescent (60W)
     * Note: y = Illuminance or lux
     *
     * For applications requiring more precision,
     * narrower range of light sources should be used and a new correlation
     * matrix could be formulated and CIE tristimulus values should be
     * calculated. Please refer the manual for calculating tristumulus values.
     *
     * x = (-0.14282F * r) + (1.54924F * g) + (-0.95641F * b);
     * y = (-0.32466F * r) + (1.57837F * g) + (-0.73191F * b);
     * z = (-0.68202F * r) + (0.77073F * g) + ( 0.56332F * b);
     *
     *
     * Calculating chromaticity co-ordinates, the light can be plotted on a two
     * dimensional chromaticity diagram
     *
     * xc = x / (x + y + z);
     * yc = y / (x + y + z);
     *
     * Use McCamy's formula to determine the CCT
     * n = (xc - 0.3320F) / (0.1858F - yc);
     */

    /*
     * n can be calculated directly using the following formula for
     * above considerations
     */
    n = ((0.23881)*scd->scd_r + (0.25499)*scd->scd_g + (-0.58291)*scd->scd_b) /
         ((0.11109)*scd->scd_r + (-0.85406)*scd->scd_g + (0.52289)*scd->scd_b);


    /*
     * Calculate the final CCT
     * CCT is only meant to characterize near white lights.
     */

#if MYNEWT_VAL(USE_MATH)
    scd->scd_colortemp = (449.0F * powf(n, 3)) + (3525.0F * powf(n, 2)) + (6823.3F * n) + 5520.33F;
#else
    scd->scd_colortemp = (449.0F * n * n * n) + (3525.0F * n * n) + (6823.3F * n) + 5520.33F;
#endif

    scd->scd_lux = (-0.32466F * scd->scd_r) + (1.57837F * scd->scd_g) + (-0.73191F * scd->scd_b);

    scd->scd_colortemp_is_valid = 1;
    scd->scd_lux_is_valid = 1;

    goto err;
#else
    uint8_t againx;
    uint8_t atime;
    uint16_t atime_ms;
    uint16_t r_comp;
    uint16_t g_comp;
    uint16_t b_comp;
    float cpl;
    uint8_t agc_cur;

    const struct tcs_agc agc_list[4] = {
        { TCS34725_GAIN_60X, TCS34725_INTEGRATIONTIME_700MS,     0, 47566 },
        { TCS34725_GAIN_16X, TCS34725_INTEGRATIONTIME_154MS,  3171, 63422 },
        { TCS34725_GAIN_4X,  TCS34725_INTEGRATIONTIME_154MS, 15855, 63422 },
        { TCS34725_GAIN_1X,  TCS34725_INTEGRATIONTIME_2_4MS,   248,     0 }
    };

    agc_cur = 0;
    while(1) {
        if (agc_list[agc_cur].max_cnt && scd->scd_c > agc_list[agc_cur].max_cnt) {
            agc_cur++;
        } else if (agc_list[agc_cur].min_cnt &&
                   scd->scd_c < agc_list[agc_cur].min_cnt) {
            agc_cur--;
            break;
        } else {
            break;
        }

        rc = tcs34725_set_gain(itf, agc_list[agc_cur].ta_gain);
        if (rc) {
            goto err;
        }

        rc = tcs34725_set_integration_time(itf, agc_list[agc_cur].ta_time);
        if (rc) {
            goto err;
        }

        /* Shock absorber */
        os_time_delay((256 - ((uint16_t)agc_list[agc_cur].ta_time) * 2.4 * 2 * OS_TICKS_PER_SEC)/1000 + 1);

        rc = tcs34725_get_rawdata(itf, &scd->scd_r, &scd->scd_g, &scd->scd_b, &scd->scd_c, tcs34725);
        if (rc) {
            goto err;
        }
        break;
    }

    atime = (uint16_t)agc_list[agc_cur].ta_time;

    /* Formula from the datasheet */
    atime_ms = ((256 - atime) * 2.4);

    switch(agc_list[agc_cur].ta_time) {
        case TCS34725_GAIN_1X:
            againx = 1;
            break;
        case TCS34725_GAIN_4X:
            againx = 4;
            break;
        case TCS34725_GAIN_16X:
            againx = 16;
            break;
        case TCS34725_GAIN_60X:
            againx = 60;
            break;
        default:
            rc = SYS_EINVAL;
            goto err;
    }

    scd->scd_ir = (scd->scd_r + scd->scd_g + scd->scd_b > scd->scd_c) ?
                  (scd->scd_r + scd->scd_g + scd->scd_b - scd->scd_c) / 2 : 0;

    r_comp = scd->scd_r - scd->scd_ir;
    g_comp = scd->scd_g - scd->scd_ir;
    b_comp = scd->scd_b - scd->scd_ir;

    scd->scd_cratio = (float)scd->scd_ir / (float)scd->scd_c;

    scd->scd_saturation = ((256 - atime) > 63) ? 65535 : 1024 * (256 - atime);

    scd->scd_saturation75 = (atime_ms < 150) ? (scd->scd_saturation - scd->scd_saturation / 4) : scd->scd_saturation;

    scd->scd_is_sat = (atime_ms < 150 && scd->scd_c > scd->scd_saturation75) ? 1 : 0;

    cpl = (atime_ms * againx) / (TCS34725_GA * TCS34725_DF);

    scd->scd_maxlux = 65535 / (cpl * 3);

    scd->scd_lux = (TCS34725_R_COEF * (float)r_comp + TCS34725_G_COEF * (float)g_comp +
           TCS34725_B_COEF * (float)b_comp) / cpl;

    scd->scd_colortemp = TCS34725_CT_COEF * (float)b_comp / (float)r_comp + TCS34725_CT_OFFSET;

    scd->scd_lux_is_valid = 1;
    scd->scd_colortemp_is_valid = 1;
    scd->scd_saturation_is_valid = 1;
    scd->scd_saturation75_is_valid = 1;
    scd->scd_is_sat_is_valid = 1;
    scd->scd_cratio_is_valid = 1;
    scd->scd_maxlux_is_valid = 1;
    scd->scd_ir_is_valid = 1;
#endif

err:
    return rc;
}

static int
tcs34725_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    struct tcs34725 *tcs34725;
    struct sensor_color_data scd;
    struct sensor_itf *itf;
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t c;
    int rc;

    /* If the read isn't looking for accel or mag data, don't do anything. */
    if (!(type & SENSOR_TYPE_COLOR)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);
    tcs34725 = (struct tcs34725 *) SENSOR_GET_DEVICE(sensor);

    /* Get a new accelerometer sample */
    if (type & SENSOR_TYPE_COLOR) {
        r = g = b = c = 0;

        rc = tcs34725_get_rawdata(itf, &r, &g, &b, &c, tcs34725);
        if (rc) {
            goto err;
        }

        scd.scd_r = r;
        scd.scd_g = g;
        scd.scd_b = b;
        scd.scd_c = c;

        scd.scd_r_is_valid = 1;
        scd.scd_g_is_valid = 1;
        scd.scd_b_is_valid = 1;
        scd.scd_c_is_valid = 1;

        rc = tcs34725_calc_colortemp_lux(itf, &scd, tcs34725);
        if (rc) {
            goto err;
        }

        /* Call data function */
        rc = data_func(sensor, data_arg, &scd);
        if (rc != 0) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

/**
 * enables/disables interrupts
 *
 * @param The sensor interface
 * @param enable/disable
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_enable_interrupt(struct sensor_itf *itf, uint8_t enable)
{
    uint8_t reg;
    int rc;

    rc = tcs34725_read8(itf, TCS34725_REG_ENABLE, &reg);
    if (rc) {
        goto err;
    }

    if (enable) {
        reg |= TCS34725_ENABLE_AIEN;
    } else {
        reg &= ~TCS34725_ENABLE_AIEN;
    }

    rc = tcs34725_write8(itf, TCS34725_REG_ENABLE, reg);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Clears the interrupt by writing to the command register
 * as a special function
 * ______________________________________________________
 * |   CMD |     TYPE    |         ADDR/SF              |
 * |    7  |     6:5     |           4:0                |
 * |    1  |      11     |          00110               |
 * |_______|_____________|______________________________|
 *
 * @param The sensor interface
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_clear_interrupt(struct sensor_itf *itf)
{
    int rc;
    uint8_t payload = TCS34725_COMMAND_BIT | TCS34725_CMD_TYPE | TCS34725_CMD_ADDR;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 0,
        .buffer = &payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Sets threshold limits for interrupts, if the low threshold is set above
 * the high threshold, the high threshold is ignored and only the low
 * threshold is evaluated
 *
 * @param The sensor interface
 * @param lower threshold
 * @param higher threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_set_int_limits(struct sensor_itf *itf, uint16_t low, uint16_t high)
{
    uint8_t payload[4];
    int rc;

    payload[0] = low & 0xFF;
    payload[1] = low >> 8;
    payload[2] = high & 0xFF;
    payload[3] = high >> 8;

    rc = tcs34725_writelen(itf, TCS34725_REG_AILTL, payload, sizeof(payload));
    if (rc) {
        return rc;
    }
    return 0;
}

/**
 *
 * Gets threshold limits for interrupts, if the low threshold is set above
 * the high threshold, the high threshold is ignored and only the low
 * threshold is evaluated
 *
 * @param The sensor interface
 * @param ptr to lower threshold
 * @param ptr to higher threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_get_int_limits(struct sensor_itf *itf, uint16_t *low, uint16_t *high)
{
    uint8_t payload[4];
    int rc;

    rc = tcs34725_readlen(itf, TCS34725_REG_AILTL, payload, sizeof(payload));
    if (rc) {
        return rc;
    }

    *low   = payload[0];
    *low  |= payload[1] << 8;
    *high  = payload[2];
    *high |= payload[3] << 8;

    return 0;
}

static int
tcs34725_sensor_get_config(struct sensor *sensor, sensor_type_t type,
                           struct sensor_cfg *cfg)
{
    int rc;

    if ((type != SENSOR_TYPE_COLOR)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_INT32;

    return (0);
err:
    return (rc);
}
