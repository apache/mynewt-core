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

#ifndef __SENSOR_ADXL345_H__
#define __SENSOR_ADXL345_H__

#include "os/os.h"
#include "os/os_dev.h"
#include "sensor/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

enum adxl345_accel_range {
    ADXL345_ACCEL_RANGE_2   = 0, /* +/- 2g  */
    ADXL345_ACCEL_RANGE_4   = 1, /* +/- 4g  */
    ADXL345_ACCEL_RANGE_8   = 2, /* +/- 8g  */
    ADXL345_ACCEL_RANGE_16  = 3, /* +/- 16g */
};

enum adxl345_power_mode {
    ADXL345_POWER_STANDBY  = 0,
    ADXL345_POWER_SLEEP    = 1,
    ADXL345_POWER_MEASURE  = 2
};

enum adxl345_sample_rate {
    ADXL345_RATE_0_1_HZ   = 0,  /* 0.1Hz */
    ADXL345_RATE_0_2_HZ   = 1,  /* 0.2Hz */
    ADXL345_RATE_0_39_HZ  = 2,  /* 0.39Hz */
    ADXL345_RATE_0_78_HZ  = 3,  /* 0.78Hz */
    ADXL345_RATE_1_56_HZ  = 4,  /* 1.56Hz */
    ADXL345_RATE_3_13_HZ  = 5,  /* 3.13Hz */
    ADXL345_RATE_6_25_HZ  = 6,  /* 6.25Hz */
    ADXL345_RATE_12_5_HZ  = 7,  /* 12.5Hz */
    ADXL345_RATE_25_HZ    = 8,  /* 25Hz */
    ADXL345_RATE_50_HZ    = 9,  /* 50Hz */
    ADXL345_RATE_100_HZ   = 10, /* 1001Hz */
    ADXL345_RATE_200_HZ   = 11, /* 200Hz */
    ADXL345_RATE_400_HZ   = 12, /* 400Hz */
    ADXL345_RATE_800_HZ   = 13, /* 800Hz */
    ADXL345_RATE_1600_HZ  = 14, /* 1600Hz */
    ADXL345_RATE_3200_HZ  = 15  /* 3200Hz */
};
  
struct adxl345_tap_settings {
    uint8_t threshold; /* threshold compared with data to identify a tap event
                              16g range, 62.5 mg/LSB */
    uint8_t duration; /* maximum time data is above threshold to be a tap event
                             625us/LSB */
    uint8_t latency; /* time to wait after a tap before window starts for detecting 
                        a double tap. 1.25ms/LSB */
    uint8_t window; /* length of time in which a double tap can be triggered, this
                       time starts after the latency time expires. 1.25ms/LSB */

    uint8_t x_enable:1; /* enables tap/double tap detection on X axis */
    uint8_t y_enable:1; /* enables tap/double tap detection on Y axis */
    uint8_t z_enable:1; /* enables tap/double tap detection on Z axis */

    uint8_t suppress:1; /* if enabled suppresses double tap detection if acceleration
                         greater than threshold is present between taps */

};

struct adxl345_cfg {
    enum adxl345_power_mode power_mode;
    uint8_t low_power_enable;
    
    enum adxl345_accel_range accel_range;
    enum adxl345_sample_rate sample_rate; 

    /* offsets used to calibrate data */
    int8_t offset_x; 
    int8_t offset_y;
    int8_t offset_z;

    /* configuration of tap/double tap detection interrupts */
    struct adxl345_tap_settings tap_cfg;

    /* configuration of freefall detection interrupt */
    uint8_t freefall_threshold;
    uint8_t freefall_time;
    
    sensor_type_t mask;
};

/* Used to track interrupt state to wake any present waiters */
struct adxl345_int {
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

/* Device private data */
struct adxl345_private_driver_data {
    struct sensor_notify_ev_ctx notify_ctx;
    struct sensor_read_ev_ctx read_ctx;
    uint8_t registered_mask;

    uint8_t int_num;
    uint8_t int_route;
    uint8_t int_enable;
};

    
struct adxl345 {
    struct os_dev dev;
    struct sensor sensor;
    struct adxl345_cfg cfg;

    struct adxl345_private_driver_data pdd;  
};

int adxl345_set_power_mode(struct sensor_itf *itf, enum adxl345_power_mode state);
int adxl345_get_power_mode(struct sensor_itf *itf, enum adxl345_power_mode *state);

int adxl345_set_low_power_enable(struct sensor_itf *itf, uint8_t enable);
int adxl345_get_low_power_enable(struct sensor_itf *itf, uint8_t *enable);

int adxl345_set_accel_range(struct sensor_itf *itf,
                            enum adxl345_accel_range range);
int adxl345_get_accel_range(struct sensor_itf *itf,
                            enum adxl345_accel_range *range);

int adxl345_set_offsets(struct sensor_itf *itf, int8_t offset_x,
                        int8_t offset_y, int8_t offset_z);
int adxl345_get_offsets(struct sensor_itf *itf, int8_t *offset_x,
                        int8_t *offset_y, int8_t *offset_z);

int adxl345_set_tap_settings(struct sensor_itf *itf, struct adxl345_tap_settings settings);
int adxl345_get_tap_settings(struct sensor_itf *itf, struct adxl345_tap_settings *settings);

int adxl345_set_active_threshold(struct sensor_itf *itf, uint8_t threshold);
int adxl345_get_active_threshold(struct sensor_itf *itf, uint8_t *threshold);

int adxl345_set_inactive_settings(struct sensor_itf *itf, uint8_t threshold, uint8_t time);
int adxl345_get_inactive_settings(struct sensor_itf *itf, uint8_t *threshold, uint8_t *time);

int adxl345_set_freefall_settings(struct sensor_itf *itf, uint8_t threshold, uint8_t time);
int adxl345_get_freefall_settings(struct sensor_itf *itf, uint8_t *threshold, uint8_t *time);

int adxl345_set_sample_rate(struct sensor_itf *itf, enum adxl345_sample_rate rate);
int adxl345_get_sample_rate(struct sensor_itf *itf, enum adxl345_sample_rate *rate);

int adxl345_init(struct os_dev *, void *);
int adxl345_config(struct adxl345 *, struct adxl345_cfg *);

int adxl345_get_accel_data(struct sensor_itf *itf, struct sensor_accel_data *sad);

#if MYNEWT_VAL(ADXL345_CLI)
int adxl345_shell_init(void);
#endif

    
#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_ADXL345_H__ */
