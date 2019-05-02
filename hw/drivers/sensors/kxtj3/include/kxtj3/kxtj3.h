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

#ifndef __KXTJ3_H__
#define __KXTJ3_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* KXTJ3 operating modes */
enum kxtj3_oper_mode {
    KXTJ3_OPER_MODE_STANDBY   = 0,
    KXTJ3_OPER_MODE_OPERATING = 1,
};

/* KXTJ3 performance mode */
enum kxtj3_perf_mode {
/* Low Power mode available only for ODR <= 200Hz */
    KXTJ3_PERF_MODE_LOW_POWER_8BIT = 0,
    KXTJ3_PERF_MODE_HIGH_RES_12BIT = 1,
/* 14bit data available only for 8/16G */
    KXTJ3_PERF_MODE_HIGH_RES_14BIT = 2,
};

/* KXTJ3 acceleration range */
enum kxtj3_grange {
    KXTJ3_GRANGE_2G  = 2,
    KXTJ3_GRANGE_4G  = 4,
    KXTJ3_GRANGE_8G  = 8,
    KXTJ3_GRANGE_16G = 16,
};

/* KXTJ3 ODR */
enum kxtj3_odr {
    KXTJ3_ODR_0P781HZ = 0,
    KXTJ3_ODR_1P563HZ = 1,
    KXTJ3_ODR_3P125HZ = 2,
    KXTJ3_ODR_6P25HZ  = 3,
    KXTJ3_ODR_12P5HZ  = 4,
    KXTJ3_ODR_25HZ    = 5,
    KXTJ3_ODR_50HZ    = 6,
    KXTJ3_ODR_100HZ   = 7,
    KXTJ3_ODR_200HZ   = 8,
    KXTJ3_ODR_400HZ   = 9,
    KXTJ3_ODR_800HZ   = 10,
    KXTJ3_ODR_1600HZ  = 11,
};

struct kxtj3_cfg {
    enum kxtj3_oper_mode oper_mode;
    enum kxtj3_perf_mode perf_mode;
    enum kxtj3_grange grange;
    enum kxtj3_odr odr;
    uint32_t sensors_mask;
};

struct kxtj3 {
    struct os_dev dev;
    struct sensor sensor;
    struct kxtj3_cfg cfg;
};

/**
 * Initialize the kxtj3. This function is normally called by sysinit.
 *
 * @param dev  Pointer to the kxtj3_dev device descriptor
 *
 * @return 0 on success, non-zero on failure.
 */
int kxtj3_init(struct os_dev *dev, void *arg);

/**
 * Sets sensor device configuration.
 *
 * @param  kxtj3      Pointer to the kxtj3 struct
 * @param  kxtj3_cfg  Pointer to the kxtj3 configuration
 *
 * @return 0 on success, non-zero on failure.
 */
int kxtj3_config(struct kxtj3 *kxtj3, struct kxtj3_cfg *kxtj3_cfg);


#if MYNEWT_VAL(KXTJ3_CLI)
/**
 * Initialize the KXTJ3 shell extensions.
 *
 * @return 0 on success, non-zero on failure.
 */
int
kxtj3_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __KXTJ3_H__ */

