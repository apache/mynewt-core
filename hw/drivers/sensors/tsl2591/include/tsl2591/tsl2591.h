/**************************************************************************/
/*!
    @file     tsl2591.h
    @author   Kevin Townsend

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, Kevin Townsend
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
/**************************************************************************/

#ifndef __TSL2591_H__
#define __TSL2591_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

enum tsl2591_light_gain {
    TSL2591_LIGHT_GAIN_LOW        = 0x00, /* 1X     */
    TSL2591_LIGHT_GAIN_MED        = 0x10, /* ~25X   */
    TSL2591_LIGHT_GAIN_HIGH       = 0x20, /* ~428X  */
    TSL2591_LIGHT_GAIN_MAX        = 0x30  /* ~9876X */
};

enum tsl2591_light_itime {
    TSL2591_LIGHT_ITIME_100MS     = 0x00, /* 100ms */
    TSL2591_LIGHT_ITIME_200MS     = 0x01, /* 200ms */
    TSL2591_LIGHT_ITIME_300MS     = 0x02, /* 300ms */
    TSL2591_LIGHT_ITIME_400MS     = 0x03, /* 400ms */
    TSL2591_LIGHT_ITIME_500MS     = 0x04, /* 500ms */
    TSL2591_LIGHT_ITIME_600MS     = 0x05  /* 600ms */
};

struct tsl2591_cfg {
    uint8_t gain;
    uint8_t integration_time;
    sensor_type_t mask;
};

struct tsl2591 {
    struct os_dev dev;
    struct sensor sensor;
    struct tsl2591_cfg cfg;
    os_time_t last_read_time;
};

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this luminosity sensor
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int tsl2591_init(struct os_dev *dev, void *arg);

/**
 * Enable or disables the sensor to save power
 *
 * @param The sensor interface
 * @param state  1 to enable the sensor, 0 to disable it
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2591_enable(struct sensor_itf *itf, uint8_t state);

/**
 * Gets the current 'enabled' state for the IC
 *
 * @param The sensor interface
 * @param ptr to the enabled variable to be filled up
 * @return 0 on success, non-zero on failure
 */
int tsl2591_get_enable(struct sensor_itf *itf, uint8_t *enabled);

/**
 * Gets a new data sample from the light sensor.
 *
 * @param The sensor interface
 * @param broadband The full (visible + ir) sensor output
 * @param ir        The ir sensor output
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2591_get_data(struct sensor_itf *itf, uint16_t *broadband, uint16_t *ir);

/**
 * Sets the integration time used when sampling light values.
 *
 * @param The sensor interface
 * @param int_time The integration time which can be one of:
 *                  - 0x00: 100ms
 *                  - 0x01: 200ms
 *                  - 0x02: 300ms
 *                  - 0x03: 400ms
 *                  - 0x04: 500ms
 *                  - 0x05: 600ms
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2591_set_integration_time(struct sensor_itf *itf, uint8_t int_time);

/**
 * Gets the current integration time used when sampling light values.
 *
 * @param The sensor interface
 * @param ptr to the integration time which can be one of:
 *                  - 0x00: 100ms
 *                  - 0x01: 200ms
 *                  - 0x02: 300ms
 *                  - 0x03: 400ms
 *                  - 0x04: 500ms
 *                  - 0x05: 600ms
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2591_get_integration_time(struct sensor_itf *itf, uint8_t *int_time);

/**
 * Sets the gain increment used when sampling light values.
 *
 * @param The sensor interface
 * @param gain The gain increment which can be one of:
 *                  - 0x00: Low (no gain)
 *                  - 0x10: Medium (~25x gain)
 *                  - 0x20: High (~428x gain)
 *                  - 0x30: Max (~9876x gain)
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2591_set_gain(struct sensor_itf *itf, uint8_t gain);

/**
 * Gets the current gain increment used when sampling light values.
 *
 * @param The sensor interface
 * @param ptr to the gain increment which can be one of:
 *                  - 0x00: Low (no gain)
 *                  - 0x10: Medium (~25x gain)
 *                  - 0x20: High (~428x gain)
 *                  - 0x30: Max (~9876x gain)
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2591_get_gain(struct sensor_itf *itf, uint8_t *gain);

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int tsl2591_config(struct tsl2591 *, struct tsl2591_cfg *);

#if MYNEWT_VAL(TSL2591_CLI)
int tsl2591_shell_init(void);
#endif


#ifdef __cplusplus
}
#endif

#endif /* __TSL2591_H__ */
