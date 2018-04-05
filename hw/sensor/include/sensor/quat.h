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

#ifndef __SENSOR_QUAT_H__
#define __SENSOR_QUAT_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Data representing a singular read from a quat sensor.
 */
struct sensor_quat_data {
    float sqd_x;
    float sqd_y;
    float sqd_z;
    float sqd_w;
    /* Validity */
    uint8_t sqd_x_is_valid:1;
    uint8_t sqd_y_is_valid:1;
    uint8_t sqd_z_is_valid:1;
    uint8_t sqd_w_is_valid:1;
} __attribute__((packed));

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_ACCEL_H__ */
