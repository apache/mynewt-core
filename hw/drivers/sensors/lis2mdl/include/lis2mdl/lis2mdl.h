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

#ifndef __SENSOR_LIS2MDL_H__
#define __SENSOR_LIS2MDL_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

enum lis2mdl_rate {
    LIS2MDL_MAG_RATE_10,
    LIS2MDL_MAG_RATE_20,
    LIS2MDL_MAG_RATE_50,
    LIS2MDL_MAG_RATE_100
};

enum lis2mdl_mode {
    LIS2MDL_CONTINUOUS_MODE,
    LIS2MDL_SINGLE_MODE,
    LIS2MDL_IDLE0_MODE,
    LIS2MDL_IDLE1_MODE
};

struct lis2mdl_cfg {
    enum lis2mdl_rate rate;
    enum lis2mdl_mode mode;
    sensor_type_t mask;
};

struct lis2mdl {
    struct os_dev dev;
    struct sensor sensor;
    struct lis2mdl_cfg cfg;
    os_time_t last_read_time;
};

int lis2mdl_init(struct os_dev *, void *);
int lis2mdl_config(struct lis2mdl *, struct lis2mdl_cfg *);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_LIS2MDL_H__ */
