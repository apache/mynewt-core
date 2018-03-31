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

#ifndef __SENSOR_LIGHT_H__
#define __SENSOR_LIGHT_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Data representing a singular read from a luminosity sensor.
 * All data is in lux
 */
struct sensor_light_data {
    uint16_t sld_full;
    uint16_t sld_ir;
    uint32_t sld_lux;
    /* Validity */
    uint8_t sld_full_is_valid:1;
    uint8_t sld_ir_is_valid:1;
    uint8_t sld_lux_is_valid:1;
} __attribute__((packed));

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_LIGHT_H__ */
