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


#ifndef __icp101xx_H__
#define __icp101xx_H__

#include <stdint.h>
#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

struct icp101xx_cfg {
    float sensor_constants[4]; // OTP
    float p_Pa_calib[3];
    float LUT_lower;
    float LUT_upper;
    float quadr_factor;
    float offst_factor;
    uint32_t bc_mask;
    uint16_t measurement_mode;
    uint8_t skip_first_data;
};

struct icp101xx {
    struct os_dev dev;
    struct sensor sensor;
    struct icp101xx_cfg cfg;
    os_time_t last_read_time;
};

/**
 *  @brief Initialize the icp101xx
 *         Expects to be called back through os_dev_create().
 *  @param The device object associated with this pressure
 *  @param Argument passed to OS device init, unused
 *  @return     0 on success, negative value on error
 */
int icp101xx_init(struct os_dev *dev, void *arg);

/**
 *  @brief icp101xx sensor configuration
 *  @return     0 on success, negative value on error
 */
int icp101xx_config(struct icp101xx *, struct icp101xx_cfg *);

/** @brief return WHOAMI value
 *  @param[out] whoami WHOAMI for device
 *  @return     0 on success, negative value on error
 */
int icp101xx_get_whoami(struct sensor_itf *itf, uint8_t * whoami);

/** @brief Send soft reset
 *  @return     0 on success, negative value on error
 */
int icp101xx_soft_reset(struct sensor_itf *itf);

/** @brief Check and retrieve for new data
 *  @param[out] pressure pressure data in Pascal
 *  @param[out] temperature temperature data in Degree Celsius
 *  @return     0 on success, negative value on error
 */
int icp101xx_get_data(struct sensor_itf *itf,  struct icp101xx_cfg *, float * temperature, float * pressure);

#if MYNEWT_VAL(ICP101XX_CLI)
int icp101xx_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __icp101xx_H__ */
