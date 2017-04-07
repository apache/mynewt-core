/*****************************************************************************/
/*!
    @file     tsl2561.c
    @author   ktown (Adafruit Industries)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2016, Adafruit Industries (adafruit.com)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "defs/error.h"
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "hal/hal_i2c.h"
#include "sensor/sensor.h"
#include "sensor/light.h"
#include "tsl2561/tsl2561.h"
#include "tsl2561_priv.h"

#if MYNEWT_VAL(TSL2561_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(TSL2561_STATS)
#include "stats/stats.h"
#endif

/* ToDo: Add timer based polling in bg and data ready callback (os_event?) */
/* ToDo: Move values to struct incl. address to allow multiple instances */

uint8_t g_tsl2561_gain;
uint8_t g_tsl2561_integration_time;
uint8_t g_tsl2561_enabled;

#if MYNEWT_VAL(TSL2561_STATS)
/* Define the stats section and records */
STATS_SECT_START(tsl2561_stat_section)
    STATS_SECT_ENTRY(samples_13ms)
    STATS_SECT_ENTRY(samples_101ms)
    STATS_SECT_ENTRY(samples_402ms)
    STATS_SECT_ENTRY(ints_cleared)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(tsl2561_stat_section)
    STATS_NAME(tsl2561_stat_section, samples_13ms)
    STATS_NAME(tsl2561_stat_section, samples_101ms)
    STATS_NAME(tsl2561_stat_section, samples_402ms)
    STATS_NAME(tsl2561_stat_section, ints_cleared)
    STATS_NAME(tsl2561_stat_section, errors)
STATS_NAME_END(tsl2561_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(tsl2561_stat_section) g_tsl2561stats;
#endif

#if MYNEWT_VAL(TSL2561_LOG)
#define LOG_MODULE_TSL2561    (2561)
#define TSL2561_INFO(...)     LOG_INFO(&_log, LOG_MODULE_TSL2561, __VA_ARGS__)
#define TSL2561_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_TSL2561, __VA_ARGS__)
static struct log _log;
#else
#define TSL2561_INFO(...)
#define TSL2561_ERR(...)
#endif

/* Exports for the sensor interface.
 */
static void *tsl2561_sensor_get_interface(struct sensor *, sensor_type_t);
static int tsl2561_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int tsl2561_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_tsl2561_sensor_driver = {
    tsl2561_sensor_get_interface,
    tsl2561_sensor_read,
    tsl2561_sensor_get_config
};

int
tsl2561_write8(uint8_t reg, uint32_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = MYNEWT_VAL(TSL2561_I2CADDR),
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        TSL2561_ERR("Failed to write 0x%02X:0x%02X with value 0x%02X\n",
                    data_struct.address, reg, value);
#if MYNEWT_VAL(TSL2561_STATS)
        STATS_INC(g_tsl2561stats, errors);
#endif
    }

    return rc;
}

int
tsl2561_write16(uint8_t reg, uint16_t value)
{
    int rc;
    uint8_t payload[3] = { reg, value & 0xFF, (value >> 8) & 0xFF };

    struct hal_i2c_master_data data_struct = {
        .address = MYNEWT_VAL(TSL2561_I2CADDR),
        .len = 3,
        .buffer = payload
    };

    rc = hal_i2c_master_write(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        TSL2561_ERR("Failed to write @0x%02X with value 0x%02X 0x%02X\n",
                    reg, payload[0], payload[1]);
    }

    return rc;
}

int
tsl2561_read8(uint8_t reg, uint8_t *value)
{
    int rc;
    uint8_t payload;

    struct hal_i2c_master_data data_struct = {
        .address = MYNEWT_VAL(TSL2561_I2CADDR),
        .len = 1,
        .buffer = &payload
    };

    /* Register write */
    payload = reg;
    rc = hal_i2c_master_write(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        TSL2561_ERR("Failed to address sensor\n");
        goto err;
    }

    /* Read one byte back */
    payload = 0;
    rc = hal_i2c_master_read(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);
    *value = payload;
    if (rc) {
        TSL2561_ERR("Failed to read @0x%02X\n", reg);
    }

err:
    return rc;
}

int
tsl2561_read16(uint8_t reg, uint16_t *value)
{
    int rc;
    uint8_t payload[2] = { reg, 0 };

    struct hal_i2c_master_data data_struct = {
        .address = MYNEWT_VAL(TSL2561_I2CADDR),
        .len = 1,
        .buffer = payload
    };

    /* Register write */
    rc = hal_i2c_master_write(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        TSL2561_ERR("Failed to address sensor\n");
        goto err;
    }

    /* Read two bytes back */
    memset(payload, 0, 2);
    data_struct.len = 2;
    rc = hal_i2c_master_read(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);
    *value = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
    if (rc) {
        TSL2561_ERR("Failed to read @0x%02X\n", reg);
        goto err;
    }

err:
    return rc;
}

int
tsl2561_enable(uint8_t state)
{
    int rc;

    /* Enable the device by setting the control bit to 0x03 */
    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_CONTROL,
                        state ? TSL2561_CONTROL_POWERON :
                                TSL2561_CONTROL_POWEROFF);
    if (!rc) {
        g_tsl2561_enabled = state ? 1 : 0;
    }

    return rc;
}

uint8_t
tsl2561_get_enable (void)
{
    return g_tsl2561_enabled;
}

int
tsl2561_set_integration_time(uint8_t int_time)
{
    int rc;

    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING,
                        int_time | g_tsl2561_gain);
    if (rc) {
        goto err;
    }

    g_tsl2561_integration_time = int_time;

err:
    return rc;
}

uint8_t
tsl2561_get_integration_time(void)
{
    return g_tsl2561_integration_time;
}

int
tsl2561_set_gain(uint8_t gain)
{
    int rc;

    if ((gain != TSL2561_LIGHT_GAIN_1X) && (gain != TSL2561_LIGHT_GAIN_16X)) {
        TSL2561_ERR("Invalid gain value\n");
        rc = SYS_EINVAL;
        goto err;
    }

    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING,
                        g_tsl2561_integration_time | gain);
    if (rc) {
        goto err;
    }

    g_tsl2561_gain = gain;

err:
    return rc;
}

uint8_t
tsl2561_get_gain(void)
{
    return g_tsl2561_gain;
}

int
tsl2561_get_data(uint16_t *broadband, uint16_t *ir, struct tsl2561 *tsl2561)
{
    int rc;
    int delay_ticks;

    /* Wait integration time ms before getting a data sample */
    switch (tsl2561->cfg.integration_time) {
        case TSL2561_LIGHT_ITIME_13MS:
            delay_ticks = 14 * OS_TICKS_PER_SEC / 1000;
        break;
        case TSL2561_LIGHT_ITIME_101MS:
            delay_ticks = 102 * OS_TICKS_PER_SEC / 1000;
        break;
        case TSL2561_LIGHT_ITIME_402MS:
        default:
            delay_ticks = 403 * OS_TICKS_PER_SEC / 1000;
        break;
    }
    os_time_delay(delay_ticks);

    *broadband = *ir = 0;
    rc = tsl2561_read16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN0_LOW,
                        broadband);
    if (rc) {
        goto err;
    }
    rc = tsl2561_read16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN1_LOW,
                        ir);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(TSL2561_STATS)
    switch (tsl2561->cfg.integration_time) {
        case TSL2561_LIGHT_ITIME_13MS:
            STATS_INC(g_tsl2561stats, samples_13ms);
        break;
        case TSL2561_LIGHT_ITIME_101MS:
            STATS_INC(g_tsl2561stats, samples_101ms);
        break;
        case TSL2561_LIGHT_ITIME_402MS:
            STATS_INC(g_tsl2561stats, samples_402ms);
        default:
        break;
    }
#endif

err:
    return rc;
}

int
tsl2561_setup_interrupt (uint8_t rate, uint16_t lower, uint16_t upper)
{
    int rc;
    uint8_t intval;

    /* Set lower threshold */
    rc = tsl2561_write16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_THRESHHOLDL_LOW,
                         lower);
    if (rc) {
        goto err;
    }

    /* Set upper threshold */
    rc = tsl2561_write16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_THRESHHOLDH_LOW,
                         upper);
    if (rc) {
        goto err;
    }

    /* Set rate */
    rc = tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT, &intval);
    if (rc) {
        goto err;
    }
    /* Maintain the INTR Control Select bits */
    rate = (intval & 0xF0) | (rate & 0xF);
    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT,
                        rate);
    if (rc) {
        goto err;
    }

err:
    return rc;
}

int
tsl2561_enable_interrupt (uint8_t enable)
{
    int rc;
    uint8_t persist_val;

    if (enable > 1) {
        TSL2561_ERR("Invalid value 0x%02X in tsl2561_enable_interrupt\n",
                    enable);
        rc = SYS_EINVAL;
        goto err;
    }

    /* Read the current value to maintain PERSIST state */
    rc = tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT, &persist_val);
    if (rc) {
        goto err;
    }

    /* Enable (1) or disable (0)  level interrupts */
    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT,
                        ((enable & 0x01) << 4) | (persist_val & 0x0F) );
    if (rc) {
        goto err;
    }

err:
    return rc;
}

int
tsl2561_clear_interrupt (void)
{
    int rc;
    uint8_t payload = { TSL2561_COMMAND_BIT | TSL2561_CLEAR_BIT };

    struct hal_i2c_master_data data_struct = {
        .address = MYNEWT_VAL(TSL2561_I2CADDR),
        .len = 1,
        .buffer = &payload
    };

    /* To clear the interrupt set the CLEAR bit in the COMMAND register */
    rc = hal_i2c_master_write(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(TSL2561_STATS)
    STATS_INC(g_tsl2561stats, ints_cleared);
#endif

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
tsl2561_init(struct os_dev *dev, void *arg)
{
    struct tsl2561 *tsl2561;
    struct sensor *sensor;
    int rc;

    tsl2561 = (struct tsl2561 *) dev;

#if MYNEWT_VAL(TSL2561_LOG)
    log_register("tsl2561", &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

    sensor = &tsl2561->sensor;

#if MYNEWT_VAL(TSL2561_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_tsl2561stats),
        STATS_SIZE_INIT_PARMS(g_tsl2561stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(tsl2561_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register("tsl2561", STATS_HDR(g_tsl2561stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the light driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_LIGHT,
            (struct sensor_driver *) &g_tsl2561_sensor_driver);
    if (rc != 0) {
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

static void *
tsl2561_sensor_get_interface(struct sensor *sensor, sensor_type_t type)
{
    return (NULL);
}

static uint32_t
tsl2561_calculate_lux(uint16_t broadband, uint16_t ir, struct tsl2561_cfg *cfg)
{
    uint64_t chscale;
    uint64_t channel1;
    uint64_t channel0;
    uint16_t clipthreshold;
    uint64_t ratio1;
    uint64_t ratio;
    int64_t  b, m;
    uint64_t temp;
    uint32_t lux;

    /* Make sure the sensor isn't saturated! */
    switch (cfg->integration_time) {
        case TSL2561_LIGHT_ITIME_13MS:
            clipthreshold = TSL2561_CLIPPING_13MS;
            break;
        case TSL2561_LIGHT_ITIME_101MS:
            clipthreshold = TSL2561_CLIPPING_101MS;
            break;
        default:
            clipthreshold = TSL2561_CLIPPING_402MS;
            break;
    }

    /* Return 65536 lux if the sensor is saturated */
    if ((broadband > clipthreshold) || (ir > clipthreshold)) {
        return 65536;
    }

    /* Get the correct scale depending on the intergration time */
    switch (cfg->integration_time) {
        case TSL2561_LIGHT_ITIME_13MS:
            chscale = TSL2561_LUX_CHSCALE_TINT0;
            break;
        case TSL2561_LIGHT_ITIME_101MS:
            chscale = TSL2561_LUX_CHSCALE_TINT1;
            break;
        default: /* No scaling ... integration time = 402ms */
            chscale = (1 << TSL2561_LUX_CHSCALE);
            break;
    }

    /* Scale for gain (1x or 16x) */
    if (!cfg->gain) {
        chscale = chscale << 4;
    }

    /* Scale the channel values */
    channel0 = (broadband * chscale) >> TSL2561_LUX_CHSCALE;
    channel1 = (ir * chscale) >> TSL2561_LUX_CHSCALE;

    ratio1 = 0;
    /* Find the ratio of the channel values (Channel1/Channel0) */
    if (channel0 != 0) {
        ratio1 = (channel1 << (TSL2561_LUX_RATIOSCALE+1)) / channel0;
    }

    /* round the ratio value */
    ratio = (ratio1 + 1) >> 1;

#if MYNEWT_VAL(TSL2561_PACKAGE_CS)
    if ((ratio >= 0) && (ratio <= TSL2561_LUX_K1C)) {
        b = TSL2561_LUX_B1C;
        m = TSL2561_LUX_M1C;
    } else if (ratio <= TSL2561_LUX_K2C) {
        b = TSL2561_LUX_B2C;
        m = TSL2561_LUX_M2C;
    } else if (ratio <= TSL2561_LUX_K3C) {
        b = TSL2561_LUX_B3C;
        m = TSL2561_LUX_M3C;
    } else if (ratio <= TSL2561_LUX_K4C) {
        b = TSL2561_LUX_B4C;
        m = TSL2561_LUX_M4C;
    } else if (ratio <= TSL2561_LUX_K5C) {
        b = TSL2561_LUX_B5C;
        m = TSL2561_LUX_M5C;
    } else if (ratio <= TSL2561_LUX_K6C) {
        b = TSL2561_LUX_B6C;
        m = TSL2561_LUX_M6C;
    } else if (ratio <= TSL2561_LUX_K7C) {
        b = TSL2561_LUX_B7C;
        m = TSL2561_LUX_M7C;
    } else if (ratio > TSL2561_LUX_K8C) {
        b = TSL2561_LUX_B8C;
        m = TSL2561_LUX_M8C;
    }
#else
    if ((ratio >= 0) && (ratio <= TSL2561_LUX_K1T)) {
        b = TSL2561_LUX_B1T;
        m = TSL2561_LUX_M1T;
    } else if (ratio <= TSL2561_LUX_K2T) {
        b = TSL2561_LUX_B2T;
        m = TSL2561_LUX_M2T;
    } else if (ratio <= TSL2561_LUX_K3T) {
        b = TSL2561_LUX_B3T;
        m = TSL2561_LUX_M3T;
    } else if (ratio <= TSL2561_LUX_K4T) {
        b = TSL2561_LUX_B4T;
        m = TSL2561_LUX_M4T;
    } else if (ratio <= TSL2561_LUX_K5T) {
        b = TSL2561_LUX_B5T;
        m = TSL2561_LUX_M5T;
    } else if (ratio <= TSL2561_LUX_K6T) {
        b = TSL2561_LUX_B6T;
        m = TSL2561_LUX_M6T;
    } else if (ratio <= TSL2561_LUX_K7T) {
        b = TSL2561_LUX_B7T;
        m = TSL2561_LUX_M7T;
    } else if (ratio > TSL2561_LUX_K8T) {
        b = TSL2561_LUX_B8T;
        m = TSL2561_LUX_M8T;
    }
#endif

    temp = ((channel0 * b) - (channel1 * m));

    /* Do not allow negative lux value */
    if (temp < 0) {
        temp = 0;
    }
    /* Round lsb (2^(LUX_SCALE-1)) */
    temp += (1 << (TSL2561_LUX_LUXSCALE - 1));

    /* Strip off fractional portion */
    lux = temp >> TSL2561_LUX_LUXSCALE;

    return lux;
}

static int
tsl2561_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    struct tsl2561 *tsl2561;
    struct sensor_light_data sld;
    uint16_t full;
    uint16_t ir;
    uint32_t lux;
    int rc;

    /* If the read isn't looking for accel or mag data, don't do anything. */
    if (!(type & SENSOR_TYPE_LIGHT)) {
        rc = SYS_EINVAL;
        goto err;
    }

    tsl2561 = (struct tsl2561 *) SENSOR_GET_DEVICE(sensor);

    /* Get a new accelerometer sample */
    if (type & SENSOR_TYPE_LIGHT) {
        full = ir = 0;

        rc = tsl2561_get_data(&full, &ir, tsl2561);
        if (rc) {
            goto err;
        }

        lux = tsl2561_calculate_lux(full, ir, &(tsl2561->cfg));
        sld.sld_full = full;
        sld.sld_ir = ir;
        sld.sld_lux = lux;

        sld.sld_full_is_valid = 1;
        sld.sld_ir_is_valid   = 1;
        sld.sld_lux_is_valid  = 1;

        /* Call data function */
        rc = data_func(sensor, data_arg, &sld);
        if (rc != 0) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
tsl2561_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if ((type != SENSOR_TYPE_LIGHT)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_INT32;

    return (0);
err:
    return (rc);
}

int
tsl2561_config(struct tsl2561 *tsl2561, struct tsl2561_cfg *cfg)
{
    int rc;

    rc = tsl2561_enable(1);

    rc |= tsl2561_set_integration_time(cfg->integration_time);

    rc |= tsl2561_set_gain(cfg->gain);
    if (rc) {
        goto err;
    }

    /* Overwrite the configuration data. */
    memcpy(&tsl2561->cfg, cfg, sizeof(*cfg));

err:
    return (rc);
}
