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
#include "sysinit/sysinit.h"
#include "hal/hal_i2c.h"
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

static uint8_t g_tsl2561_gain;
static uint8_t g_tsl2561_integration_time;
static uint8_t g_tsl2561_enabled;

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
        TSL2561_ERR("Failed to write @0x%02X with value 0x%02X\n", reg, value);
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
        goto error;
    }

    /* Read one byte back */
    payload = 0;
    rc = hal_i2c_master_read(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);
    *value = payload;
    if (rc) {
        TSL2561_ERR("Failed to read @0x%02X\n", reg);
    }

error:
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
        goto error;
    }

    /* Read two bytes back */
    memset(payload, 0, 2);
    data_struct.len = 2;
    rc = hal_i2c_master_read(MYNEWT_VAL(TSL2561_I2CBUS), &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);
    *value = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
    if (rc) {
        TSL2561_ERR("Failed to read @0x%02X\n", reg);
        goto error;
    }

    /* ToDo: Log raw reads */
    // console_printf("0x%04X\n", (uint16_t)payload[0] | ((uint16_t)payload[1] << 8));

error:
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
tsl2561_get_data(uint16_t *broadband, uint16_t *ir)
{
    int rc;
    int delay_ticks;

    /* Wait integration time ms before getting a data sample */
    switch (g_tsl2561_integration_time) {
        case TSL2561_INTEGRATIONTIME_13MS:
            delay_ticks = 14 * OS_TICKS_PER_SEC / 1000;
        break;
        case TSL2561_INTEGRATIONTIME_101MS:
            delay_ticks = 102 * OS_TICKS_PER_SEC / 1000;
        break;
        case TSL2561_INTEGRATIONTIME_402MS:
        default:
            delay_ticks = 403 * OS_TICKS_PER_SEC / 1000;
        break;
    }
    os_time_delay(delay_ticks);

    *broadband = *ir = 0;
    rc = tsl2561_read16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN0_LOW,
                        broadband);
    if (rc) {
        goto error;
    }
    rc = tsl2561_read16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN1_LOW,
                        ir);
    if (rc) {
        goto error;
    }

#if MYNEWT_VAL(TSL2561_STATS)
    switch (g_tsl2561_integration_time) {
        case TSL2561_INTEGRATIONTIME_13MS:
            STATS_INC(g_tsl2561stats, samples_13ms);
        break;
        case TSL2561_INTEGRATIONTIME_101MS:
            STATS_INC(g_tsl2561stats, samples_101ms);
        break;
        case TSL2561_INTEGRATIONTIME_402MS:
            STATS_INC(g_tsl2561stats, samples_402ms);
        default:
        break;
    }
#endif

error:
    return rc;
}

int
tsl2561_set_integration_time(uint8_t int_time)
{
    int rc;

    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING,
                        g_tsl2561_integration_time | g_tsl2561_gain);
    if (rc) {
        goto error;
    }

    g_tsl2561_integration_time = int_time;

error:
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

    if ((gain != TSL2561_GAIN_1X) && (gain != TSL2561_GAIN_16X)) {
        TSL2561_ERR("Invalid gain value\n");
        rc = EINVAL;
        goto error;
    }

    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING,
                        g_tsl2561_integration_time | gain);
    if (rc) {
        goto error;
    }

    g_tsl2561_gain = gain;

error:
    return rc;
}

uint8_t
tsl2561_get_gain(void)
{
    return g_tsl2561_gain;
}

int tsl2561_setup_interrupt (uint8_t rate, uint16_t lower, uint16_t upper)
{
    int rc;
    uint8_t intval;

    /* Set lower threshold */
    rc = tsl2561_write16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_THRESHHOLDL_LOW,
                         lower);
    if (rc) {
        goto error;
    }

    /* Set upper threshold */
    rc = tsl2561_write16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_THRESHHOLDH_LOW,
                         upper);
    if (rc) {
        goto error;
    }

    /* Set rate */
    rc = tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT, &intval);
    if (rc) {
        goto error;
    }
    /* Maintain the INTR Control Select bits */
    rate = (intval & 0xF0) | (rate & 0xF);
    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT,
                        rate);
    if (rc) {
        goto error;
    }

error:
    return rc;
}

int tsl2561_enable_interrupt (uint8_t enable)
{
    int rc;
    uint8_t persist_val;

    if (enable > 1) {
        TSL2561_ERR("Invalid value 0x%02X in tsl2561_enable_interrupt\n",
                    enable);
        rc = EINVAL;
        goto error;
    }

    /* Read the current value to maintain PERSIST state */
    rc = tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT, &persist_val);
    if (rc) {
        goto error;
    }

    /* Enable (1) or disable (0)  level interrupts */
    rc = tsl2561_write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT,
                        ((enable & 0x01) << 4) | (persist_val & 0x0F) );
    if (rc) {
        goto error;
    }

error:
    return rc;
}

int tsl2561_clear_interrupt (void)
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
        goto error;
    }

#if MYNEWT_VAL(TSL2561_STATS)
    STATS_INC(g_tsl2561stats, ints_cleared);
#endif

error:
    return rc;
}

void
tsl2561_init(void)
{
    int rc;

#if !MYNEWT_VAL(TSL2561_TASK)
    return;
#endif

#if MYNEWT_VAL(TSL2561_LOG)
    log_register("tsl2561", &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

#if MYNEWT_VAL(TSL2561_CLI)
    rc = tsl2561_shell_init();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

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

    /* Enable the device by default */
    rc = tsl2561_enable(1);
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = tsl2561_set_gain(TSL2561_GAIN_1X);
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = tsl2561_set_integration_time(TSL2561_INTEGRATIONTIME_13MS);
    SYSINIT_PANIC_ASSERT(rc == 0);
}
