/**************************************************************************/
/*!
    @file     tsl2561.h
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
/**************************************************************************/

#ifndef __ADAFRUIT_TSL2561_H__
#define __ADAFRUIT_TSL2561_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

enum tsl2561_light_gain {
    TSL2561_LIGHT_GAIN_1X         = 0x00,         /* 1X    */
    TSL2561_LIGHT_GAIN_16X        = 0x01 << 4     /* 16X   */
};

enum tsl2561_light_itime {
    TSL2561_LIGHT_ITIME_13MS      = 0x00,         /* 13ms  */
    TSL2561_LIGHT_ITIME_101MS     = 0x01,         /* 101ms */
    TSL2561_LIGHT_ITIME_402MS     = 0x01 << 1     /* 402ms */
};

struct tsl2561_cfg {
    uint8_t gain;
    uint8_t integration_time;
    sensor_type_t mask;
};

struct tsl2561 {
    struct os_dev dev;
    struct sensor sensor;
    struct tsl2561_cfg cfg;
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
int tsl2561_init(struct os_dev *dev, void *arg);

/**
 * Enable or disables the sensor to save power
 *
 * @param The sensor interface
 * @param state  1 to enable the sensor, 0 to disable it
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2561_enable(struct sensor_itf *itf, uint8_t state);

/**
 * Gets the current 'enabled' state for the IC
 *
 * @param The sensor interface
 * @param ptr to the enabled variable to be filled up
 * @return 0 on success, non-zero on failure
 */
int tsl2561_get_enable(struct sensor_itf *itf, uint8_t *enabled);

/**
 * Gets a new data sample from the light sensor.
 *
 * @param The sensor interface
 * @param broadband The full (visible + ir) sensor output
 * @param ir        The ir sensor output
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2561_get_data(struct sensor_itf *itf, uint16_t *broadband, uint16_t *ir);

/**
 * Sets the integration time used when sampling light values.
 *
 * @param The sensor interface
 * @param int_time The integration time which can be one of:
 *                  - 0x00: 13ms
 *                  - 0x01: 101ms
 *                  - 0x02: 402ms
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2561_set_integration_time(struct sensor_itf *itf, uint8_t int_time);

/**
 * Gets the current integration time used when sampling light values.
 *
 * @param The sensor interface
 * @param ptr to the integration time which can be one of:
 *         - 0x00: 13ms
 *         - 0x01: 101ms
 *         - 0x02: 402ms
 * @return 0 on success, non-zero on failure
 */
int tsl2561_get_integration_time(struct sensor_itf *itf, uint8_t *int_time);

/**
 * Sets the gain increment used when sampling light values.
 *
 * @param The sensor interface
 * @param gain The gain increment which can be one of:
 *                  - 0x00: 1x (no gain)
 *                  - 0x10: 16x gain
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2561_set_gain(struct sensor_itf *itf, uint8_t gain);

/**
 * Gets the current gain increment used when sampling light values.
 *
 * @param The sensor interface
 * @param ptr to the gain increment which can be one of:
 *         - 0x00: 1x (no gain)
 *         - 0x10: 16x gain
 * @return 0 on success, non-zero on failure
 */
int tsl2561_get_gain(struct sensor_itf *itf, uint8_t *gain);

/**
 * Sets the upper and lower interrupt thresholds
 *
 * @param The sensor interface
 * @param rate    Sets the rate of interrupts to the host processor:
 *                - 0   Every ADC cycle generates interrupt
 *                - 1   Any value outside of threshold range
 *                - 2   2 integration time periods out of range
 *                - 3   3 integration time periods out of range
 *                - 4   4 integration time periods out of range
 *                - 5   5 integration time periods out of range
 *                - 6   6 integration time periods out of range
 *                - 7   7 integration time periods out of range
 *                - 8   8 integration time periods out of range
 *                - 9   9 integration time periods out of range
 *                - 10  10 integration time periods out of range
 *                - 11  11 integration time periods out of range
 *                - 12  12 integration time periods out of range
 *                - 13  13 integration time periods out of range
 *                - 14  14 integration time periods out of range
 *                - 15  15 integration time periods out of range
 * @param lower   The lower threshold
 * @param upper   The upper threshold
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2561_setup_interrupt(struct sensor_itf *itf, uint8_t rate, uint16_t lower, uint16_t upper);

/**
 * Enables or disables the HW interrupt on the device
 *
 * @param The sensor interface
 * @param enable  0 to disable the interrupt, 1 to enablee it
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2561_enable_interrupt(struct sensor_itf *itf, uint8_t enable);

/**
 * Clear an asserted interrupt on the device
 *
 * @param The sensor interface
 * @return 0 on success, non-zero on failure
 */
int tsl2561_clear_interrupt(struct sensor_itf *itf);

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int tsl2561_config(struct tsl2561 *, struct tsl2561_cfg *);

#if MYNEWT_VAL(TSL2561_CLI)
int tsl2561_shell_init(void);
#endif


#ifdef __cplusplus
}
#endif

#endif /* __ADAFRUIT_TSL2561_H__ */
