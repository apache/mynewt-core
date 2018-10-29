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

#ifndef __BME860_H__
#define __BME860_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#include "bme680/bme680_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bme680 {
    struct os_dev dev;
    struct sensor sensor;
    struct bme680_cfg cfg;
};

int bme680_init(struct os_dev *dev, void *arg);
int bme680_config(struct bme680 *bme680, struct bme680_cfg *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __BME860_H__ */
