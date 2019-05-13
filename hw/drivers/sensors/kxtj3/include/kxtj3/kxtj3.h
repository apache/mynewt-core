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
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#endif

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

/* KXTJ3 Wake-up ODR */
enum kxtj3_wuf_odr {
    KXTJ3_WUF_ODR_0P781HZ = 0,
    KXTJ3_WUF_ODR_1P563HZ = 1,
    KXTJ3_WUF_ODR_3P125HZ = 2,
    KXTJ3_WUF_ODR_6P25HZ  = 3,
    KXTJ3_WUF_ODR_12P5HZ  = 4,
    KXTJ3_WUF_ODR_25HZ    = 5,
    KXTJ3_WUF_ODR_50HZ    = 6,
    KXTJ3_WUF_ODR_100HZ   = 7,
};

/* KXTJ3 wake-up functionality config */
struct kxtj3_wuf_cfg {
    enum kxtj3_wuf_odr odr;
    float threshold; /* ms/2 */
    float delay; /* seconds */
};

struct kxtj3_cfg {
    enum kxtj3_oper_mode oper_mode;
    enum kxtj3_perf_mode perf_mode;
    enum kxtj3_grange grange;
    enum kxtj3_odr odr;

    /* Wake-up config */
    struct kxtj3_wuf_cfg wuf;

    /* interrupt config */
    uint8_t int_enable;
    uint8_t int_polarity;
    uint8_t int_latch;

    uint32_t sensors_mask;
};

/* Used to track interrupt state to wake any present waiters */
struct kxtj3_int {
    /* Synchronize access to this structure */
    os_sr_t lock;
    /* Sleep waiting for an interrupt to occur */
    struct os_sem wait;
    /* Is the interrupt currently active */
    bool active;
    /* Is there a waiter currently sleeping */
    bool asleep;
    /* Configured interrupts */
    struct sensor_int *ints;
};

/* Int enabled flags */
#define KXTJ3_INT_WUFE 0x2

/* Private per driver data */
struct kxtj3_pdd {
    struct kxtj3_int *interrupt;
    struct sensor_notify_ev_ctx notify_ctx;
    uint8_t int_enabled_bits;
};

struct kxtj3 {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_i2c_node i2c_node;
#else
    struct os_dev dev;
#endif
    struct sensor sensor;
    struct kxtj3_cfg cfg;
    struct kxtj3_int intr;
    struct kxtj3_pdd pdd;
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


#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Create I2C bus node for KXTJ3 sensor
 *
 * @param node        Bus node
 * @param name        Device name
 * @param i2c_cfg     I2C node configuration
 * @param sensor_itf  Sensors interface
 *
 * @return 0 on success, non-zero on failure
 */
int
kxtj3_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                            const struct bus_i2c_node_cfg *i2c_cfg,
                            struct sensor_itf *sensor_itf);
#endif

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

