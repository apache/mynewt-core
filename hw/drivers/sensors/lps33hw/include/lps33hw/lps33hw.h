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

#ifndef __SENSOR_LPS33HW_H__
#define __SENSOR_LPS33HW_H__

#include "os/os.h"
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LPS33HW_I2C_ADDR (0x5C)

#define LPS33HW_INT_LEVEL (0x80)
#define LPS33HW_INT_OPEN (0x40)
#define LPS33HW_INT_LATCH_EN (0x20)
#define LPS33HW_INT_RD_CLEAR (0x10)

enum lps33hw_output_data_rates {
    LPS33HW_ONE_SHOT            = 0x00,
    LPS33HW_1KHZ                = 0x01,
    LPS33HW_10KHZ               = 0x02,
    LPS33HW_25KHZ               = 0x03,
    LPS33HW_50KHZ               = 0x04,
    LPS33HW_75KHZ               = 0x05,
};

enum lps33hw_low_pass_config {
    LPS33HW_LPF_DISABLED        = 0x00, /* Bandwidth = data rate / 2 */
    LPS33HW_LPF_ENABLED_LOW_BW  = 0x02, /* Bandwidth = data rate / 9 */
    LPS33HW_LPF_ENABLED_HIGH_BW = 0x03, /* Bandwidth = data rate / 20 */
};

struct lps33hw_int_cfg {
    unsigned int pressure_low : 1;
    unsigned int pressure_high : 1;
    unsigned int enabled : 1;
    unsigned int active_low : 1;
    unsigned int open_drain : 1;
};

struct lps33hw_cfg {
    sensor_type_t mask;
    struct lps33hw_int_cfg int_cfg;
    enum lps33hw_output_data_rates data_rate;
    enum lps33hw_low_pass_config lpf;
};

struct lps33hw {
    struct os_dev dev;
    struct sensor sensor;
    struct lps33hw_cfg cfg;
    os_time_t last_read_time;
};

int lps33hw_config_interrupt(struct sensor_itf *itf,
    struct lps33hw_int_cfg cfg);
int lps33hw_set_lpf(struct sensor_itf *itf,
    enum lps33hw_low_pass_config lpf);

int lps33hw_init(struct os_dev *, void *);
int lps33hw_config(struct lps33hw *, struct lps33hw_cfg *);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_LPS33HW_H__ */
