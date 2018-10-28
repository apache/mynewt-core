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
#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"
#include "sensor/sensor.h"
#include "sensor/light.h"
#include "tsl2591/tsl2591.h"
#include "tsl2591_priv.h"
#include "modlog/modlog.h"
#include "syscfg/syscfg.h"
#include "stats/stats.h"

/* Define the stats section and records */
STATS_SECT_START(tsl2591_stat_section)
    STATS_SECT_ENTRY(polled)
    STATS_SECT_ENTRY(gain_changed)
    STATS_SECT_ENTRY(timing_changed)
    STATS_SECT_ENTRY(ints_cleared)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(tsl2591_stat_section)
    STATS_NAME(tsl2591_stat_section, polled)
    STATS_NAME(tsl2591_stat_section, gain_changed)
    STATS_NAME(tsl2591_stat_section, timing_changed)
    STATS_NAME(tsl2591_stat_section, ints_cleared)
    STATS_NAME(tsl2591_stat_section, errors)
STATS_NAME_END(tsl2591_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(tsl2591_stat_section) g_tsl2591stats;

#define TSL2591_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(TSL2591_LOG_MODULE), __VA_ARGS__)

#if MYNEWT_VAL(TSL2591_ITIME_DELAY)
static int g_tsl2591_itime_delay_ms;
#endif

/* Exports for the sensor API */
static int tsl2591_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int tsl2591_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_tsl2591_sensor_driver = {
    tsl2591_sensor_read,
    tsl2591_sensor_get_config
};

int
tsl2591_write8(struct sensor_itf *itf, uint8_t reg, uint32_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    rc = sensor_itf_lock(itf, MYNEWT_VAL(TSL2591_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(TSL2591_I2C_RETRIES));
    if (rc) {
        TSL2591_LOG(ERROR,
                    "Failed to write 0x%02X:0x%02X with value 0x%02lX\n",
                    data_struct.address, reg, value);
        STATS_INC(g_tsl2591stats, errors);
    }

    sensor_itf_unlock(itf);

    return rc;
}

int
tsl2591_write16(struct sensor_itf *itf, uint8_t reg, uint16_t value)
{
    int rc;
    uint8_t payload[3] = { reg, value & 0xFF, (value >> 8) & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 3,
        .buffer = payload
    };

    rc = sensor_itf_lock(itf, MYNEWT_VAL(TSL2591_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(TSL2591_I2C_RETRIES));
    if (rc) {
        TSL2591_LOG(ERROR,
                    "Failed to write @0x%02X with value 0x%02X 0x%02X\n",
                    reg, payload[0], payload[1]);
        STATS_INC(g_tsl2591stats, errors);
    }

    sensor_itf_unlock(itf);

    return rc;
}

int
tsl2591_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;
    uint8_t payload;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &payload
    };

    rc = sensor_itf_lock(itf, MYNEWT_VAL(TSL2591_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    /* Register write */
    payload = reg;
    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(TSL2591_I2C_RETRIES));
    if (rc) {
        TSL2591_LOG(ERROR, "Failed to address sensor\n");
        STATS_INC(g_tsl2591stats, errors);
        goto err;
    }

    /* Read one byte back */
    payload = 0;
    rc = i2cn_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                          MYNEWT_VAL(TSL2591_I2C_RETRIES));
    *value = payload;
    if (rc) {
        TSL2591_LOG(ERROR, "Failed to read @0x%02X\n", reg);
        STATS_INC(g_tsl2591stats, errors);
    }

err:
    sensor_itf_unlock(itf);

    return rc;
}

int
tsl2591_read16(struct sensor_itf *itf, uint8_t reg, uint16_t *value)
{
    int rc;
    uint8_t payload[2] = { reg, 0 };

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    rc = sensor_itf_lock(itf, MYNEWT_VAL(TSL2591_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    /* Register write */
    rc = i2cn_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(TSL2591_I2C_RETRIES));
    if (rc) {
        TSL2591_LOG(ERROR, "Failed to address sensor\n");
        STATS_INC(g_tsl2591stats, errors);
        goto err;
    }

    /* Read two bytes back */
    memset(payload, 0, 2);
    data_struct.len = 2;
    rc = i2cn_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                          MYNEWT_VAL(TSL2591_I2C_RETRIES));
    *value = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
    if (rc) {
        TSL2591_LOG(ERROR, "Failed to read @0x%02X\n", reg);
        STATS_INC(g_tsl2591stats, errors);
        goto err;
    }

err:
    sensor_itf_unlock(itf);

    return rc;
}

int
tsl2591_enable(struct sensor_itf *itf, uint8_t state)
{
    /* Enable the device by setting the PON and AEN bits */
    return tsl2591_write8(itf, TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE,
                          state ? TSL2591_ENABLE_POWERON | TSL2591_ENABLE_AEN :
                          TSL2591_ENABLE_POWEROFF);
}

int
tsl2591_get_enable(struct sensor_itf *itf, uint8_t *enabled)
{
    int rc;
    uint8_t reg;

    /* Check the two enable bits (PON and AEN) */
    rc =  tsl2591_read8(itf, TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE,
                        &reg);
    if (rc) {
        goto err;
    }

    *enabled = reg & (TSL2591_ENABLE_POWERON | TSL2591_ENABLE_AEN) ? 1 : 0;

    return 0;
err:
    return rc;
}

int
tsl2591_set_integration_time(struct sensor_itf *itf,
                             uint8_t int_time)
{
    int rc;
    uint8_t gain;

    if (int_time > TSL2591_LIGHT_ITIME_600MS) {
      int_time = TSL2591_LIGHT_ITIME_600MS;
    }

    rc = tsl2591_get_gain(itf, &gain);
    if (rc) {
        goto err;
    }

    rc = tsl2591_write8(itf, TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL,
                        int_time | gain);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(TSL2591_ITIME_DELAY)
    /* Set the intergration time delay value in ms (+8% margin of error) */
    g_tsl2591_itime_delay_ms = (int_time + 1) * 108;
#endif

    /* Increment the timing changed counter */
    STATS_INC(g_tsl2591stats, timing_changed);

    return 0;
err:
    return rc;
}

int
tsl2591_get_integration_time(struct sensor_itf *itf, uint8_t *itime)
{
    int rc;
    uint8_t reg;

    rc = tsl2591_read8(itf, TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL,
                       &reg);
    if (rc) {
        goto err;
    }

    *itime = reg & 0x07;

    return 0;
err:
    return rc;
}

int
tsl2591_set_gain(struct sensor_itf *itf, uint8_t gain)
{
    int rc;
    uint8_t int_time;

    if ((gain != TSL2591_LIGHT_GAIN_LOW)
        && (gain != TSL2591_LIGHT_GAIN_MED)
        && (gain != TSL2591_LIGHT_GAIN_HIGH)
        && (gain != TSL2591_LIGHT_GAIN_MAX)) {
            TSL2591_LOG(ERROR, "Invalid gain value\n");
            rc = SYS_EINVAL;
            goto err;
    }

    rc = tsl2591_get_integration_time(itf, &int_time);
    if (rc) {
        goto err;
    }

    rc = tsl2591_write8(itf, TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL,
                        int_time | gain);
    if (rc) {
        goto err;
    }

    /* Increment the gain change counter */
    STATS_INC(g_tsl2591stats, gain_changed);

    return 0;
err:
    return rc;
}

int
tsl2591_get_gain(struct sensor_itf *itf, uint8_t *gain)
{
    int rc;
    uint8_t reg;

    rc = tsl2591_read8(itf, TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL,
                       &reg);
    if (rc) {
        goto err;
    }

    *gain = reg & 0x30;

    return 0;
err:
    return rc;
}

int
tsl2591_get_data_r(struct sensor_itf *itf, uint16_t *broadband, uint16_t *ir)
{
    int rc;

#if MYNEWT_VAL(TSL2591_ITIME_DELAY)
    /* Insert a delay of integration time to ensure valid sample */
    os_time_delay((OS_TICKS_PER_SEC * g_tsl2591_itime_delay_ms) / 1000);
#endif

    /* CHAN0 must be read before CHAN1 */
    /* See: https://forums.adafruit.com/viewtopic.php?f=19&t=124176 */
    *broadband = *ir = 0;
    rc = tsl2591_read16(itf, TSL2591_COMMAND_BIT |
                        TSL2591_REGISTER_CHAN0_LOW, broadband);
    if (rc) {
        goto err;
    }
    rc = tsl2591_read16(itf, TSL2591_COMMAND_BIT |
                        TSL2591_REGISTER_CHAN1_LOW, ir);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
tsl2591_get_data(struct sensor_itf *itf, uint16_t *broadband, uint16_t *ir)
{
    int rc;
    uint8_t itime;
    uint16_t igain;
    uint16_t maxval;
    uint16_t divisor;

#if !(MYNEWT_VAL(TSL2591_AUTO_GAIN))
    rc = tsl2591_get_data_r(itf, broadband, ir);
    if (rc) {
        goto err;
    }

    /* Increment the polling counter */
    STATS_INC(g_tsl2591stats, polled);

    return rc;
#endif

    /* Use auto-gain algorithm for better range at the expensive of */
    /* unpredictable conversion times */

    /* Get the integration time to determine max raw value */
    rc = tsl2591_get_integration_time(itf, &itime);
    if (rc) {
        goto err;
    }
    /* Max value for 100ms = 37888, otherwise 65535 */
    maxval = itime ? 65535 : 37888;

    /* Set gain to 1x for the baseline conversion */
    rc = tsl2591_set_gain(itf, TSL2591_LIGHT_GAIN_LOW);
    if (rc) {
        goto err;
    }

    /* Get the baseline conversion values */
    /* Note: double-read required to empty cached values with prev gain */
    rc = tsl2591_get_data_r(itf, broadband, ir);
    rc = tsl2591_get_data_r(itf, broadband, ir);
    if (rc) {
        goto err;
    }

    /* Determine the ideal gain setting */
    divisor = *broadband > *ir ? *broadband : *ir;
    /* Avoid potential divide by 0 errors in low light */
    igain = maxval / (divisor == 0 ? 1 : divisor);


    /* Find the closest gain <= igain */
    if (igain < 25) {
        /* Gain 1x, return the current sample */
        return 0;
    } else if (igain < 428) {
        /* Set gain to ~25x */
        rc = tsl2591_set_gain(itf, TSL2591_LIGHT_GAIN_MED);
        if (rc) {
            goto err;
        }
    } else if (igain < 9876) {
        /* Set gain to ~428x */
        rc = tsl2591_set_gain(itf, TSL2591_LIGHT_GAIN_HIGH);
        if (rc) {
            goto err;
        }
    } else {
        /* Set gain to ~9876x */
        rc = tsl2591_set_gain(itf, TSL2591_LIGHT_GAIN_MAX);
        if (rc) {
            goto err;
        }
    }

    /* Get a new data sample */
    /* Note: double-read required to empty cached values with prev gain */
    rc = tsl2591_get_data_r(itf, broadband, ir);
    rc = tsl2591_get_data_r(itf, broadband, ir);
    if (rc) {
        goto err;
    }

    /* Increment the polling counter */
    STATS_INC(g_tsl2591stats, polled);

    return 0;
err:
    return rc;
}

int
tsl2591_init(struct os_dev *dev, void *arg)
{
    struct tsl2591 *tsl2591;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    tsl2591 = (struct tsl2591 *) dev;

    tsl2591->cfg.mask = SENSOR_TYPE_ALL;

    sensor = &tsl2591->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_tsl2591stats),
        STATS_SIZE_INIT_PARMS(g_tsl2591stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(tsl2591_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_tsl2591stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

#ifdef ARCH_sim
    /* Register the sim driver */
    tsl2591_sim_init();
#endif

    rc = sensor_init(sensor, dev);
    if (rc) {
        goto err;
    }

    /* Add the light driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_LIGHT,
            (struct sensor_driver *) &g_tsl2591_sensor_driver);
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

    return 0;
err:
    STATS_INC(g_tsl2591stats, errors);
    return rc;
}

float tsl2591_calculate_lux_f(struct sensor_itf *itf, uint16_t broadband,
  uint16_t ir, struct tsl2591_cfg *cfg)
{
    int      rc;
    uint8_t  itime;
    uint8_t  gain;
    float    again;
    float    cpl;
    float    lux;

    /* Check for overflow conditions */
    if ((broadband == 0xFFFF) | (ir == 0xFFFF)) {
        return 0;
    }

    rc = tsl2591_get_gain(itf, &gain);
    if (rc) {
        return 0;
    }

    switch (gain)
    {
      case TSL2591_LIGHT_GAIN_MED :
        again = 25.0F;
        break;
      case TSL2591_LIGHT_GAIN_HIGH :
        again = 428.0F;
        break;
      case TSL2591_LIGHT_GAIN_MAX :
        again = 9876.0F;
        break;
      case TSL2591_LIGHT_GAIN_LOW :
      default:
        again = 1.0F;
        break;
    }

    rc = tsl2591_get_integration_time(itf, &itime);
    if (rc) {
        return 0;
    }

    cpl = ((float)((itime+1)*101) * again) / TSL2591_LUX_DF;

    lux = (((float)broadband - (float)ir)) *
      (1.0F - ((float)ir/(float)broadband)) / cpl;

    /* Note that this implementation will truncate values < 1.0 lux! */
    return lux;
}

uint32_t
tsl2591_calculate_lux(struct sensor_itf *itf, uint16_t broadband, uint16_t ir,
  struct tsl2591_cfg *cfg)
{
    return (uint32_t)tsl2591_calculate_lux_f(itf, broadband, ir, cfg);
}

static int
tsl2591_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    struct tsl2591 *tsl2591;
    struct sensor_light_data sld;
    struct sensor_itf *itf;
    uint16_t full;
    uint16_t ir;
    uint32_t lux;
    int rc;

    /* If the read isn't looking for light data, don't do anything. */
    if (!(type & SENSOR_TYPE_LIGHT)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);
    tsl2591 = (struct tsl2591 *)SENSOR_GET_DEVICE(sensor);

    /* Get a new light sample */
    if (type & SENSOR_TYPE_LIGHT) {
        full = ir = 0;

        rc = tsl2591_get_data(itf, &full, &ir);
        if (rc) {
            goto err;
        }

        lux = tsl2591_calculate_lux(itf, full, ir, &(tsl2591->cfg));
        sld.sld_full = full;
        sld.sld_ir = ir;
        sld.sld_lux = lux;

        sld.sld_full_is_valid = 1;
        sld.sld_ir_is_valid   = 1;
        sld.sld_lux_is_valid  = 1;

        /* Call data function */
        rc = data_func(sensor, data_arg, &sld, SENSOR_TYPE_LIGHT);
        if (rc != 0) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
tsl2591_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if ((type != SENSOR_TYPE_LIGHT)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_INT32;

    return 0;
err:
    return rc;
}

int
tsl2591_config(struct tsl2591 *tsl2591, struct tsl2591_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(tsl2591->sensor));

    rc = tsl2591_enable(itf, 1);
    if (rc) {
        goto err;
    }

    rc = tsl2591_set_integration_time(itf, cfg->integration_time);
    if (rc) {
        goto err;
    }

    tsl2591->cfg.integration_time = cfg->integration_time;

    rc = tsl2591_set_gain(itf, cfg->gain);
    if (rc) {
        goto err;
    }

    tsl2591->cfg.gain = cfg->gain;

    rc = sensor_set_type_mask(&(tsl2591->sensor), cfg->mask);
    if (rc) {
        goto err;
    }

    tsl2591->cfg.mask = cfg->mask;

    return 0;
err:
    return rc;
}
