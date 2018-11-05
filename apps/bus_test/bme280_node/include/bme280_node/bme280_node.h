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

#ifndef BME280_NODE_H_
#define BME280_NODE_H_

#include <stddef.h>
#include <stdint.h>
#include "os/os_dev.h"
#include "sys/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bme280_node_measurement {
    float temperature;
    float pressure;
    float humidity;
};

int
bme280_node_read(struct os_dev *dev, struct bme280_node_measurement *measurment);

int
bme280_node_i2c_create(const char *name, const struct bus_i2c_node_cfg *cfg);

#ifdef __cplusplus
}
#endif

#endif /* BME280_NODE_H_ */
