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

#ifndef __SIM_ACCEL_H__
#define __SIM_ACCEL_H__

#include "os/os.h"
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sim_accel_cfg {
    uint8_t sac_nr_samples;
    uint8_t sac_nr_axises;
    uint16_t sac_sample_itvl;
    sensor_type_t sac_mask;
};

struct sim_accel {
    struct os_dev sa_dev;
    struct sensor sa_sensor;
    struct sim_accel_cfg sa_cfg;
    os_time_t sa_last_read_time;
};

int sim_accel_init(struct os_dev *, void *);
int sim_accel_config(struct sim_accel *, struct sim_accel_cfg *);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_SIM_H__ */
