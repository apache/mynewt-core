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


#ifndef __MS5837_H__
#define __MS5837_H__

#include <os/os.h>
#include "os/os_dev.h"
#include "sensor/sensor.h"

#define MS5837_I2C_ADDRESS		0x76

#ifdef __cplusplus
extern "C" {
#endif

struct ms5837_cfg {
    uint8_t bc_mode;
    sensor_type_t bc_s_mask;
};

struct ms5837 {
    struct os_dev dev;
    struct sensor sensor;
    struct ms5837_cfg cfg;
    os_time_t last_read_time;
};

/**
 * Initialize the ms5837.
 *
 * @param dev  Pointer to the ms5837_dev device descriptor
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
ms5837_init(struct os_dev *dev, void *arg);

/**
 * Gets temperature
 *
 * @param The sensor interface
 * @param temperature
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
ms5837_get_temperature(struct sensor_itf *itf, int32_t *temp);

/**
 * Gets pressure
 *
 * @param The sensor interface
 * @param pressure
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
ms5837_get_pressure(struct sensor_itf *itf, int32_t *press);

/**
 * Resets the MS5837 chip
 *
 * @param The sensor interface
 * @return 0 on success, non-zero on failure
 */
int
ms5837_reset(struct sensor_itf *itf);

/**
 * Configure MS5837 sensor
 *
 * @param Sensor device MS5837 structure
 * @param Sensor device MS5837 config
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
ms5837_config(struct ms5837 *ms5837, struct ms5837_cfg *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __MS5837_H__ */
