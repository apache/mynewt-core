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
#include <stats/stats.h>
#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

enum icp101xx_meas {
    ICP101XX_MEAS_LOW_POWER_T_FIRST       = 0,
    ICP101XX_MEAS_LOW_POWER_P_FIRST       = 1,
    ICP101XX_MEAS_NORMAL_T_FIRST          = 2,
    ICP101XX_MEAS_NORMAL_P_FIRST          = 3,
    ICP101XX_MEAS_LOW_NOISE_T_FIRST       = 4,
    ICP101XX_MEAS_LOW_NOISE_P_FIRST       = 5,
    ICP101XX_MEAS_ULTRA_LOW_NOISE_T_FIRST = 6,
    ICP101XX_MEAS_ULTRA_LOW_NOISE_P_FIRST = 7
};

/**
 *  @brief icp101xx sensor configuration structure
 */
struct icp101xx_cfg {
    float sensor_constants[4]; // OTP
    float p_Pa_calib[3];
    float LUT_lower;
    float LUT_upper;
    float quadr_factor;
    float offst_factor;
    uint32_t bc_mask;
    enum icp101xx_meas measurement_mode;
    uint8_t skip_first_data;
};

/* Define the stats section and records */
STATS_SECT_START(icp101xx_stat_section)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

/**
 *  @brief icp101xx sensor device structure
 */
struct icp101xx {
    struct os_dev dev;
    struct sensor sensor;
    struct icp101xx_cfg cfg;
    os_time_t last_read_time;
    /* Variable used to hold stats data */
    STATS_SECT_DECL(icp101xx_stat_section) stats;
};

/**
 *  @brief Initialize the icp101xx
 *         Expects to be called back through os_dev_create().
 *  @param The device object associated with this pressure
 *  @param Argument passed to OS device init, unused
 *  @return     0 on success, negative value on error
 */
int icp101xx_init(struct os_dev *, void *);

/**
 *  @brief Configure the icp101xx
 *  @param icp101xx sensor device
 *  @param icp101xx sensor configuration
 *  @return     0 on success, negative value on error
 */
int icp101xx_config(struct icp101xx *, struct icp101xx_cfg *);

/** @brief return WHOAMI value
 *  @param icp101xx sensor device
 *  @param[out] whoami WHOAMI for device
 *  @return     0 on success, negative value on error
 */
int icp101xx_get_whoami(struct icp101xx *, uint8_t * whoami);

/** @brief Send soft reset
 *  @param icp101xx sensor device
 *  @return     0 on success, negative value on error
 */
int icp101xx_soft_reset(struct icp101xx *);

/** @brief Check and retrieve for new data
 *  @param icp101xx sensor device
 *  @param icp101xx sensor configuration
 *  @param[out] pressure pressure data in Pascal
 *  @param[out] temperature temperature data in Degree Celsius
 *  @return     0 on success, negative value on error
 */
int icp101xx_get_data(struct icp101xx *,  struct icp101xx_cfg *, float * temperature, float * pressure);

#if MYNEWT_VAL(ICP101XX_CLI)
int icp101xx_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __icp101xx_H__ */
