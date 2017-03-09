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

#ifndef __SENSOR_COLOR_H__
#define __SENSOR_COLOR_H__

#include "os/os.h"
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Data representing a singular read from a color sensor
 */
struct sensor_color_data {
    uint16_t scd_r;         /* Red data   */
    uint16_t scd_g;         /* Green data */
    uint16_t scd_b;         /* Blue data  */
    uint16_t scd_c;         /* Clear data */
    uint16_t scd_lux;       /* Lux data   */
    uint16_t scd_colortemp; /* Color temp */
} __attribute__((packed));

/**
 * Color Sensor data is unused for this field.
 */
#define SENSOR_COLOR_DATA_UNUSED (-1)

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_COLOR_H__ */
