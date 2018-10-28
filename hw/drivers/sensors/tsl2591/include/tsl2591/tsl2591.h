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
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2591_config(struct tsl2591 *, struct tsl2591_cfg *);

/**
 * Calculate light level in lux based on the full spectrum and IR readings
 *
 * NOTE: This function assumes that the gain and integration time used when
 *       reading the full spectrum and IR readings are the same as when this
 *       function gets called. If gain or integration time have changed, a
 *       fresh sample should be read before calling this function.
 *
 * @param ptr to sensor driver
 * @param Full spectrum (broadband) light reading
 * @param IR light reading
 * @param ptr to the sensor driver config
 *
 * @return 0 on failure, otherwise the converted light level in lux as an
 *         unsigned 32-bit integer.
 */
uint32_t tsl2591_calculate_lux(struct sensor_itf *itf, uint16_t broadband,
  uint16_t ir, struct tsl2591_cfg *cfg);

/**
 * Calculate light level in lux based on the full spectrum and IR readings
 *
 * NOTE: This function assumes that the gain and integration time used when
 *       reading the full spectrum and IR readings are the same as when this
 *       function gets called. If gain or integration time have changed, a
 *       fresh sample should be read before calling this function.
 *
 * @param ptr to sensor driver
 * @param Full spectrum (broadband) light reading
 * @param IR light reading
 * @param ptr to the sensor driver config
 *
 * @return 0 on failure, otherwise the converted light level in lux as a
 *         single precision float.
 */
float tsl2591_calculate_lux_f(struct sensor_itf *itf, uint16_t broadband,
  uint16_t ir, struct tsl2591_cfg *cfg);

#if MYNEWT_VAL(TSL2591_CLI)
int tsl2591_shell_init(void);
#endif

#ifdef ARCH_sim
/**
 * Registers the sim driver with the hal_i2c simulation layer
 *
 * @return 0 on success, non-zero on failure
 */
int tsl2591_sim_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __TSL2591_H__ */
