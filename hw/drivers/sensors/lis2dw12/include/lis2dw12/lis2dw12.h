/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
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

#ifndef __LIS2DW12_H__
#define __LIS2DW12_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif


enum lis2dw12_read_mode {
    LIS2DW12_READ_M_POLL = 0,
    LIS2DW12_READ_M_STREAM = 1,
};

struct lis2dw12_notif_cfg {
    sensor_event_type_t event;
    uint8_t int_num:1;
    uint8_t notif_src:7;
    uint8_t int_cfg;
};

/* Read mode configuration */
struct lis2dw12_read_mode_cfg {
    enum lis2dw12_read_mode mode;
    uint8_t int_num:1;
    uint8_t int_cfg;
};

struct lis2dw12_cfg {

    /* Read mode config */
    struct lis2dw12_read_mode_cfg read_mode;

    /* Notif config */
    struct lis2dw12_notif_cfg *notif_cfg;
    uint8_t max_num_notif;

    /* Notification event context */
    struct sensor_notify_ev_ctx notify_ctx;

    /* Sensor type mask to track enabled sensors */
    sensor_type_t mask;
};


struct lis2dw12 {
    struct os_dev dev;
    struct sensor sensor;
    struct lis2dw12_cfg cfg;
};

/**
 * Provide a continuous stream of accelerometer readings.
 *
 * @param sensor The sensor ptr
 * @param type The sensor type
 * @param read_func The function pointer to invoke for each accelerometer reading.
 * @param read_arg The opaque pointer that will be passed in to the function.
 * @param time_ms If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2dw12_stream_read(struct sensor *sensor,
                         sensor_type_t sensor_type,
                         sensor_data_func_t read_func,
                         void *read_arg,
                         uint32_t time_ms);

/**
 * Do accelerometer polling reads
 *
 * @param sensor The sensor ptr
 * @param sensor_type The sensor type
 * @param data_func The function pointer to invoke for each accelerometer reading.
 * @param data_arg The opaque pointer that will be passed in to the function.
 * @param timeout If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2dw12_poll_read(struct sensor *sensor,
                       sensor_type_t sensor_type,
                       sensor_data_func_t data_func,
                       void *data_arg,
                       uint32_t timeout);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param dev Ptr to the device object associated with this accelerometer
 * @param arg Argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2dw12_init(struct os_dev *dev, void *arg);

/**
 * Configure the sensor
 *
 * @param lis2dw12 Ptr to sensor driver
 * @param cfg Ptr to sensor driver config
 */
int lis2dw12_config(struct lis2dw12 *lis2dw12, struct lis2dw12_cfg *cfg);


#ifdef __cplusplus
}
#endif

#endif /* __LIS2DW12_H__ */
