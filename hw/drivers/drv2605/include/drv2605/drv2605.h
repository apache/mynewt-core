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

#ifndef __DRV2605_H__
#define __DRV2605_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif


struct drv2605_cal {
    uint8_t brake_factor;
    uint8_t loop_gain;
    uint8_t lra_sample_time;
    uint8_t lra_blanking_time;
    uint8_t lra_idiss_time;
    uint8_t auto_cal_time;
    uint8_t lra_zc_det_time;
};

enum drv2605_power_mode {
    DRV2605_POWER_STANDBY = 0x00, // en pin high, standby bit high
    DRV2605_POWER_ACTIVE, //en pin high, standby bit low
    DRV2605_POWER_OFF //en pin low
};

enum drv2605_op_mode {
    DRV2605_OP_ROM = 0x00,
    DRV2605_OP_PWM,
    DRV2605_OP_ANALOG,
    DRV2605_OP_RTP,
    DRV2605_OP_DIAGNOSTIC,
    DRV2605_OP_CALIBRATION,
    DRV2605_OP_RESET
};

enum drv2605_motor_type {
    DRV2605_MOTOR_LRA = 0x00,
    DRV2605_MOTOR_ERM = 0x01
};

struct drv2605_cfg {
    enum drv2605_op_mode op_mode;
    enum drv2605_motor_type motor_type;
    struct drv2605_cal cal;
};

struct drv2605 {
    struct os_dev dev;
    struct sensor sensor;
    struct drv2605_cfg cfg;
};


/**
 * Initialize the drv2605. This is normally used as an os_dev_create initialization function
 *
 * @param dev  Pointer to the drv2605_dev device descriptor
 * @param arg  Pointer to the sensor interface
 *
 * @return 0 on success, non-zero on failure
 */
int
drv2605_init(struct os_dev *dev, void *arg);


/**
 * Set up  the drv2605 with the configuration parameters given
 *
 * @param dev  Pointer to the drv2605_dev device descriptor
 * @param cfg  Pointer to the drv2605_cfg settings for mode, type, calibration
 *
 * @return 0 on success, non-zero on failure
 */
int
drv2605_config(struct drv2605 *drv2605, struct drv2605_cfg *cfg);


#if MYNEWT_VAL(DRV2605_CLI)
int
drv2605_shell_init(void);
#endif


/**
 * Get a best effort defaults for the drv2605_cal
 *
 * @param Pointer to the drv2605_cal struct
 * @return 0 on success, non-zero on failure
 */
int
drv2605_default_cal(struct drv2605_cal *cal);

/**
 * Get chip ID from the sensor
 *
 * @param The sensor interface
 * @param Pointer to the variable to fill up chip ID in
 * @return 0 on success, non-zero on failure
 */
int
drv2605_get_chip_id(struct sensor_itf *itf, uint8_t *id);

/**
 * Get chip ID from the sensor
 *
 * @param The sensor interface
 * @param Pointer to the buffer of rom library selections
 * @param Size of the rom buffer (max 8)
 * @return 0 on success, non-zero on failure
 */
int
drv2605_load_rom(struct sensor_itf *itf, uint8_t *buffer, size_t length);

/**
 * Load value for rtp playback to device
 *
 * @param The sensor interface
 * @param Value to load
 * @return 0 on success, non-zero on failure
 */
int
drv2605_load_rtp(struct sensor_itf *itf, uint8_t value);

/**
 * Trigger preloaded rom selections
 *
 * @param The sensor interface
 * @return 0 on success, non-zero on failure
 */
int
drv2605_trigger_rom(struct sensor_itf *itf);

/**
 * Get rom playback status from device
 *
 * @param The sensor interface
 * @param Pointer to the variable to fill up status in
 * @return 0 on success, non-zero on failure
 */
int
drv2605_rom_busy(struct sensor_itf *itf, bool *status);

/**
 * Set the current power mode from device.
 *
 * @param The sensor interface
 * @param drv2605_power_mode to send to device
 * @return 0 on success, non-zero on failure
 */
int
drv2605_set_power_mode(struct sensor_itf *itf, enum drv2605_power_mode power_mode);

/**
 * Get the current power mode from device
 *
 * @param The sensor interface
 * @param Pointer to drv2605_power_mode enum
 * @return 0 on success, non-zero on failure
 */
int
drv2605_get_power_mode(struct sensor_itf *itf, enum drv2605_power_mode *power_mode);

#ifdef __cplusplus
}
#endif

#endif /* __DRV2605_H__ */
