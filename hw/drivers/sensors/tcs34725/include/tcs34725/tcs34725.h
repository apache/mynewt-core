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

#ifndef __TCS34725_H__
#define __TCS34725_H__

#define TCS34725_INTEGRATIONTIME_2_4MS  0xFF   /* 2.4ms - 1 cycle    - Max Count: 1024  */
#define TCS34725_INTEGRATIONTIME_24MS   0xF6   /* 24ms  - 10 cycles  - Max Count: 10240 */
#define TCS34725_INTEGRATIONTIME_50MS   0xEB   /* 50ms  - 20 cycles  - Max Count: 20480 */
#define TCS34725_INTEGRATIONTIME_101MS  0xD5   /* 101ms - 42 cycles  - Max Count: 43008 */
#define TCS34725_INTEGRATIONTIME_154MS  0xC0   /* 154ms - 64 cycles  - Max Count: 65535 */
#define TCS34725_INTEGRATIONTIME_700MS  0x00   /* 700ms - 256 cycles - Max Count: 65535 */

#define TCS34725_GAIN_1X                0x00   /* No gain  */
#define TCS34725_GAIN_4X                0x01   /* 4x gain  */
#define TCS34725_GAIN_16X               0x02   /* 16x gain */
#define TCS34725_GAIN_60X               0x03   /* 60x gain */

#include <os/os.h>
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tcs34725_cfg {
    uint8_t gain;
    uint8_t integration_time;
};

struct tcs34725 {
    struct os_dev dev;
    struct sensor sensor;
    struct tcs34725_cfg cfg;
    os_time_t last_read_time;
};

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accellerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
tcs34725_init(struct os_dev *dev, void *arg);

/**
 * Configure the sensor
 *
 * @param ptr to the sensor
 * @param ptr to sensor config
 * @return 0 on success, non-zero on failure
 */
int
tcs34725_config(struct tcs34725 *tcs34725, struct tcs34725_cfg *cfg);

#if MYNEWT_VAL(TCS34725_CLI)
int tcs34725_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
