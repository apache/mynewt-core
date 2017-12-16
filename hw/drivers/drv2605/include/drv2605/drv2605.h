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

#include <os/os.h>
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif


struct drv2605_cal {
    // (b) FB_BRAKE_FACTOR[2:0] — A value of 2 is valid for most actuators. DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_3X
    uint8_t brake_factor;

    // (c) LOOP_GAIN[1:0] — A value of 2 is valid for most actuators. DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_HIGH
    uint8_t loop_gain;

    // (h) SAMPLE_TIME[1:0] — A value of 3 is valid for most actuators. DRV2605_CONTROL2_SAMPLE_TIME_300
    uint8_t lra_sample_time;

    // (i) BLANKING_TIME[3:0] — A value of 1 is valid for most actuators. DRV2605_CONTROL2_BLANKING_TIME_LRA_15
    uint8_t lra_blanking_time;

    // (j) IDISS_TIME[3:0] — A value of 1 is valid for most actuators. DRV2605_CONTROL2_IDISS_TIME_LRA_25
    uint8_t lra_idiss_time;

    // (f) AUTO_CAL_TIME[1:0] — A value of 3 is valid for most actuators. DRV2605_CONTROL4_AUTO_CAL_TIME_1000
    uint8_t auto_cal_time;

    // (k) ZC_DET_TIME[1:0] — A value of 0 is valid for most actuators. DRV2605_CONTROL4_ZC_DET_TIME_100
    uint8_t lra_zc_det_time;
};

struct drv2605 {
    struct os_dev dev;
    struct sensor sensor;
};

/**
 * Initialize the drv2605. This function is normally called by sysinit.
 *
 * @param dev  Pointer to the drv2605_dev device descriptor
 */
int
drv2605_init(struct os_dev *dev, void *arg);

int
drv2605_config(struct drv2605 *drv2605);

int
drv2605_internal_trigger(struct sensor_itf *itf);

int
drv2605_auto_calibrate(struct sensor_itf *itf, struct drv2605_cal *cal);

int
drv2605_send_defaults(struct sensor_itf *itf);

int
drv2605_reset(struct sensor_itf *itf);

int
drv2605_diagnostics(struct sensor_itf *itf);

int
drv2605_load(struct sensor_itf *itf, uint8_t *buffer, size_t length);


#if MYNEWT_VAL(DRV2605_CLI)
int drv2605_shell_init(void);
#endif



/**
 * Get chip ID from the sensor
 *
 * @param The sensor interface
 * @param Pointer to the variable to fill up chip ID in
 * @return 0 on success, non-zero on failure
 */
int
drv2605_get_chip_id(struct sensor_itf *itf, uint8_t *id);


#ifdef __cplusplus
}
#endif

#endif /* __DRV2605_H__ */
