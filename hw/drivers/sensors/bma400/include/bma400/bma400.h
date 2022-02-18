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

#ifndef __BMA400_H__
#define __BMA400_H__

#include <os/os.h>
#include <os/os_dev.h>
#include <sensor/sensor.h>
#include <sensor/accel.h>
#include <sensor/temperature.h>
#include <hal/hal_gpio.h>
#include "../../src/bma400_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bme400_notif_cfg {
    sensor_event_type_t event;
    uint8_t int_num;
    uint16_t notif_src;
    uint8_t int_cfg;
};

struct bma400_int_pin_cfg {
    int16_t int1_host_pin;
    int16_t int2_host_pin;
    /* INT1 is active high */
    uint8_t int1_level : 1;
    /* INT2 is active high */
    uint8_t int2_level : 1;
    /* INT1 is open drain */
    uint8_t int1_od : 1;
    /* INT2 is open drain */
    uint8_t int2_od : 1;
    /* latching mode */
    uint8_t latch_int : 1;
};

/* Range of acceleration measurements */
typedef enum {
    BMA400_G_RANGE_2G               = 0,
    BMA400_G_RANGE_4G               = 1,
    BMA400_G_RANGE_8G               = 2,
    BMA400_G_RANGE_16G              = 3,
} bma400_g_range_t;

/* Power mode for the device */
typedef enum {
    BMA400_POWER_MODE_SLEEP         = 0,
    BMA400_POWER_MODE_LOW           = 1,
    BMA400_POWER_MODE_NORMAL        = 2,
} bma400_power_mode_t;

/* Filter 1 bandwidth */
typedef enum {
    /* 0.48 x ODR */
    BMA400_FILT1_BW_0,
    /* 0.24 x ODR */
    BMA400_FILT1_BW_1,
} bma400_filt1_bandwidth_t;

/* Oversampling ration for low power mode */
typedef enum {
    /* 0.4 x ODR */
    BMA400_FILT1_BW_0_4_x_ODR       = 0,
    /* 0.2 x ODR */
    BMA400_FILT1_BW_0_2_x_ODR       = 1,
} bma400_osr_lp_t;

/* Oversampling */
typedef enum {
    BMA400_OSR_0  = 0,
    BMA400_OSR_LOWEST_ACCURACY      = BMA400_OSR_0,
    BMA400_OSR_LOWEST_POWER         = BMA400_OSR_0,
    BMA400_OSR_1  = 1,
    BMA400_OSR_2  = 2,
    BMA400_OSR_3  = 3,
    BMA400_OSR_HIGHEST_ACCURACY     = BMA400_OSR_3,
    BMA400_OSR_HIGHEST_POWER        = BMA400_OSR_3,
} bma400_oversampling_t;

/* How often acceleration measurements are taken */
typedef enum {
    BMA400_ODR_12_5_HZ              = 0x5,
    BMA400_ODR_25_HZ                = 0x6,
    BMA400_ODR_50_HZ                = 0x7,
    BMA400_ODR_100_HZ               = 0x8,
    BMA400_ODR_200_HZ               = 0x9,
    BMA400_ODR_400_HZ               = 0xA,
    BMA400_ODR_800_HZ               = 0xB,
} bma400_odr_t;

typedef enum {
    BMA400_TAP_AXIS_Z,
    BMA400_TAP_AXIS_Y,
    BMA400_TAP_AXIS_X,
} bma400_tap_axis_t;

/*
 * Maximum time between upper and lower peak of a tap, in data samples
 * this time depends on the mechanics of the device tapped onto
 */
typedef enum {
    BMA400_TAP_TICS_TH_6,
    BMA400_TAP_TICS_TH_9,
    BMA400_TAP_TICS_TH_12,
    BMA400_TAP_TICS_TH_18,
} bma400_tap_tics_th_t;

/*
 * Minimum quiet time before and after double tap, in data samples
 * This time also defines the longest time interval between two taps so that
 * they are considered as double tap
 */
typedef enum {
    BMA400_TAP_60_SAMPLES,
    BMA400_TAP_80_SAMPLES,
    BMA400_TAP_100_SAMPLES,
    BMA400_TAP_120_SAMPLES,
} bma400_tap_quite_t;

/*
 * Minimum time between the two taps of a double tap, in data samples.
 */
typedef enum {
    BMA400_D_TAP_4_SAMPLES,
    BMA400_D_TAP_8_SAMPLES,
    BMA400_D_TAP_12_SAMPLES,
    BMA400_D_TAP_16_SAMPLES,
} bma400_d_tap_quite_t;

typedef enum {
    BMA400_TAP_SENSITIVITY_0,
    BMA400_TAP_SENSITIVITY_HIGHEST = BMA400_TAP_SENSITIVITY_0,
    BMA400_TAP_SENSITIVITY_1,
    BMA400_TAP_SENSITIVITY_2,
    BMA400_TAP_SENSITIVITY_3,
    BMA400_TAP_SENSITIVITY_4,
    BMA400_TAP_SENSITIVITY_5,
    BMA400_TAP_SENSITIVITY_6,
    BMA400_TAP_SENSITIVITY_7,
    BMA400_TAP_SENSITIVITY_LOWEST = BMA400_TAP_SENSITIVITY_7,
} bma400_tap_sensitivity_t;

typedef enum {
    BMA400_NO_INT_PIN,
    BMA400_INT1_PIN,
    BMA400_INT2_PIN,
} bma400_int_num_t;

/* Settings for the double/single tap interrupt */
struct bma400_tap_cfg {
    /* sensitivity of the tap algorith */
    bma400_tap_sensitivity_t tap_sensitivity;

    bma400_tap_axis_t sel_axis;
    /*
     * Maximum time between upper and lower peak of a tap, in data samples
     * this time depends on the mechanics of the device tapped onto
     */
    bma400_tap_tics_th_t tics_th;
    /*
     * Minimum quiet time before and after double tap, in data samples
     * This time also defines the longest time interval between two taps so that
     * they are considered as double tap
     */
    bma400_tap_quite_t quite;
    /* Minimum time between the two taps of a double tap, in data samples */
    bma400_d_tap_quite_t quite_dt;

    bma400_int_num_t int_num;
};

typedef enum {
    /* 12.5Hz to 800 Hz */
    BMA400_ACC_FILT1,
    /* 100Hz */
    BMA400_ACC_FILT2,
} bma400_acc_filt_t;

typedef enum {
    BMA400_ORIENT_REFU_MANUAL,
    BMA400_ORIENT_REFU_ONE_TIME_2,
    BMA400_ORIENT_REFU_ONE_TIME_LP,
} bma400_orient_refu_t;

typedef enum {
    BMA400_ORIENT_DATA_SRC_FILT2,
    BMA400_ORIENT_DATA_SRC_FILT_LP,
} bma400_orient_data_src_t;

typedef enum {
    /* Variable ODR filter */
    BMA400_DATA_SRC_FILT1,
    /* 100 Hz output data rate filter */
    BMA400_DATA_SRC_FILT2,
    /* 100 Hz output data rate filter, 1Hz bandwidth */
    BMA400_DATA_SRC_FILT_LP,
} bma400_data_src_t;

struct bma400_acc_cfg {
    bma400_filt1_bandwidth_t filt1_bw;
    bma400_osr_lp_t osr_lp;
    bma400_power_mode_t power_mode_conf;
    bma400_g_range_t acc_range;
    bma400_oversampling_t osr;
    bma400_odr_t acc_odr;
    bma400_data_src_t data_src_reg;
};

struct bma400_orient_cfg {
    uint8_t orient_x_en : 1;
    uint8_t orient_y_en : 1;
    uint8_t orient_z_en : 1;
    /* reference update mode for orientation changed interrupt */
    bma400_orient_refu_t orient_refu;
    /* data source selection for orientation changed interrupt evaluation */
    bma400_orient_data_src_t orient_data_src;
    /* threshold configuration for orientation changed interrupt 8mg/lsb resolution */
    uint8_t orient_thres;
    /* duration for (stable) new orientation before interrupt is triggered
       duration is a multiple of the number of data samples processed (ODR=100HZ) from
       the selected filter */
    uint8_t orient_dur;
    uint16_t int_orient_refx;
    uint16_t int_orient_refy;
    uint16_t int_orient_refz;
    bma400_int_num_t int_num;
};

typedef enum {
    BMA400_ACTIVITY_DATA_SRC_FILT1,
    BMA400_ACTIVITY_DATA_SRC_FILT2,
} bma400_activity_data_src_t;

typedef enum {
    BMA400_ACTIVITY_32_POINTS,
    BMA400_ACTIVITY_64_POINTS,
    BMA400_ACTIVITY_128_POINTS,
    BMA400_ACTIVITY_256_POINTS,
    BMA400_ACTIVITY_512_POINTS,
} bma400_activity_data_points_t;

struct bma400_activity_cfg {
    /* threshold configuration for activity changed interrupt: 8mg/g resolution */
    uint8_t actch_thres;
    uint8_t actch_x_en : 1;
    uint8_t actch_y_en : 1;
    uint8_t actch_z_en : 1;
    bma400_activity_data_src_t actch_data_src;
    /* number of points for evaluation of the activity */
    bma400_activity_data_points_t actch_npts;

    bma400_int_num_t int_num;
    /* User selectable event type for activity interrupt */
    sensor_event_type_t event_type;
};

/* Array of step counter configuration registers values for wrist (default) */
extern const uint8_t BMA400_STEP_COUNTER_WRIST_CONFIG[];
/* Array of step counter configuration registers values for non-wrist application */
extern const uint8_t BMA400_STEP_COUNTER_NON_WRIST_CONFIG[];

struct bma400_step_cfg {
    const uint8_t *step_counter_config;
    bma400_int_num_t int_num;
};

typedef enum {
    BMA400_AUTOLOWPOW_TIMEOUT_DISABLE,
    BMA400_AUTOLOWPOW_TIMEOUT_1,
    BMA400_AUTOLOWPOW_TIMEOUT_2,
} bma400_autolowpow_timeout_t;

typedef enum {
    /* manual update (reference registers are updated by external MCU) */
    BMA400_WKUP_REFU_MANUAL,
    /* one time automated update before going into low power mode */
    BMA400_WKUP_REFU_ONETIME,
    /* every time after data conversion */
    BMA400_WKUP_REFU_EVERYTIME,
} bma400_wkup_refu_t;

struct bma400_autolowpow_cfg {
    /*
     * auto-low-power timeout threshold (in 2.5ms units)
     * 0-4095 (0-10.237s)
     */
    uint16_t timeout_threshold;
    bma400_autolowpow_timeout_t timeout;
    /* data ready as source for auto-low-power condition */
    uint8_t drdy_lowpow_trig : 1;
    /* generic interrupt 1 as source for auto-low-power condition */
    uint8_t trig_gen1 : 1;
};

struct bma400_autowakeup_cfg {
    /*
     * auto-wake-up timeout threshold (in 2.5ms units)
     * 0-4095 (0-10.237s)
     */
    /* wake-up timeout threshold */
    uint16_t timeout_threshold;
    /* wake-up timeout as source for auto-wake-up condition */
    uint8_t wkup_timeout: 1;
    /* wake-up interrupt as source for auto-wake-up condition */
    uint8_t wkup_int: 1;
};

struct bma400_wakeup_cfg {
    bma400_wkup_refu_t wkup_refu;
    /* number of data samples used for interrupt condition evaluation */
    uint8_t num_of_samples;
    /* enable wake-up interrupt for x channel */
    uint8_t wkup_x_en : 1;
    /* enable wake-up interrupt for y channel */
    uint8_t wkup_y_en : 1;
    /* enable wake-up interrupt for z channel */
    uint8_t wkup_z_en : 1;

    uint8_t int_wkup_thres;
    int8_t int_wkup_refx;
    int8_t int_wkup_refy;
    int8_t int_wkup_refz;

    bma400_int_num_t int_num;
};

typedef enum {
    BMA400_HYST_NO_HYST,
    BMA400_HYST_24mg,
    BMA400_HYST_48mg,
    BMA400_HYST_96mg,
} bma400_gen_act_hyst_t;

typedef enum {
    BMA400_GEN_ACT_REFU_MANUAL,
    BMA400_GEN_ACT_REFU_ONETIME,
    BMA400_GEN_ACT_REFU_EVERYTIME,
    BMA400_GEN_ACT_REFU_EVERYTIME_LP,
} bma400_gen_act_refu_t;

typedef enum {
    BMA400_GEN_DATA_SRC_FILT1,
    BMA400_GEN_DATA_SRC_FILT2,
} bma400_gen_data_src_t;

typedef enum {
    BMA400_GEN_COMB_SEL_OR,
    BMA400_GEN_COMB_SEL_AND,
} bma400_gen_comb_sel_t;

typedef enum {
    BMA400_GEN_CRITERION_INACTIVITY,
    BMA400_GEN_CRITERION_BELOW_THRESHOLD = BMA400_GEN_CRITERION_INACTIVITY,
    BMA400_GEN_CRITERION_ACTIVITY,
    BMA400_GEN_CRITERION_ABOVE_THRESHOLD = BMA400_GEN_CRITERION_ACTIVITY,
} bma400_gen_criterion_sel_t;

typedef enum {
    BMA400_GEN_INT_1,
    BMA400_GEN_INT_2,
} bma400_get_int_t;

struct bma400_gen_int_cfg {
    bool gen_act_z_en;
    bool gen_act_y_en;
    bool gen_act_x_en;
    bma400_gen_data_src_t gen_data_src;
    bma400_gen_act_refu_t gen_act_refu;
    bma400_gen_act_hyst_t gen_act_hyst;
    bma400_gen_comb_sel_t gen_comb_sel;
    bma400_gen_criterion_sel_t gen_criterion_sel;
    uint8_t get_int_thres;
    uint16_t get_int_dur;
    uint16_t get_int_th_refx;
    uint16_t get_int_th_refy;
    uint16_t get_int_th_refz;

    bma400_int_num_t int_num;

    /* User selectable event type for general interrupt */
    sensor_event_type_t event_type;
};

struct bma400_fifo_cfg {
    uint8_t fifo_z_en : 1;
    uint8_t fifo_y_en : 1;
    uint8_t fifo_x_en : 1;
    uint8_t fifo_8bit_en : 1;
    uint8_t fifo_time_en : 1;
    uint8_t fifo_stop_on_full : 1;
    uint8_t auto_flush : 1;
    uint8_t fifo_read_disable : 1;

    uint8_t fifo_data_src : 1;
    uint16_t watermark;

    bma400_int_num_t int_num;
};

struct bma400_create_dev_cfg {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bool node_is_spi;
    union {
#if MYNEWT_VAL(BMA400_SPI_SUPPORT)
        struct bus_spi_node_cfg spi_cfg;
#endif
#if MYNEWT_VAL(BMA400_I2C_SUPPORT)
        struct bus_i2c_node_cfg i2c_cfg;
#endif
    };
#else
    struct sensor_itf itf;
#endif
};

/* Default configuration values to use with the device */
struct bma400_cfg {

    bool stream_read_mode;

    /* Accelerometer configuration */
    struct bma400_acc_cfg acc_cfg;
    /* Interrupt configuration */
    struct bma400_int_pin_cfg int_pin_cfg;

    /* Tap (double & single) event configuration */
    struct bma400_tap_cfg tap_cfg;
    /* Orientation detection configuration */
    struct bma400_orient_cfg orient_cfg;
    /* Auto low power configuration */
    struct bma400_autolowpow_cfg autolowpow_cfg;
    /* Auto wakeup configuration */
    struct bma400_autowakeup_cfg autowakeup_cfg;
    /* Wakeup configuration */
    struct bma400_wakeup_cfg wakeup_cfg;
    /* Fifo configuration */
    struct bma400_fifo_cfg fifo_cfg;
    /* Activity detection configuration */
    struct bma400_activity_cfg activity_cfg;
    /* Step counter configuration */
    struct bma400_step_cfg step_cfg;
    /* General interrupt config */
    struct bma400_gen_int_cfg gen_int_cfg[2];

    /* Applicable sensor types supported */
    sensor_type_t sensor_mask;
};

/* Used to track interrupt state to wake any present waiters */
struct bma400_int {
    /* Sleep waiting for an interrupt to occur */
    struct os_sem wait;
    /* Is the interrupt currently active */
    bool active;
    /* Is there a waiter currently sleeping */
    bool asleep;
    /* Configured interrupts */
    struct sensor_int ints[2];
    hal_gpio_irq_trig_t armed_trigger[2];
};

/* Device private data */
struct bma400_private_driver_data {
    struct bma400_int *interrupt;
    struct sensor_notify_ev_ctx notify_ctx;
    uint8_t registered_mask;
    sensor_event_type_t allowed_events;

    uint8_t int_num;
    uint8_t int_ref_cnt;

    /* Shadow copy of registers */
    struct bma400_reg_cache cache;
    uint8_t transact;
    uint8_t woke;

    /* Active interrupt state */
    struct bma400_int intr;
};

/* The device itself */
struct bma400 {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    union {
        struct bus_i2c_node i2c_node;
        struct bus_spi_node spi_node;
    };
    bool node_is_spi;
#else
    /* Underlying OS device */
    struct os_dev dev;
#endif
    /* The sensor infrastructure */
    struct sensor sensor;

    /* Default configuration values */
    struct bma400_cfg cfg;

    /* Private driver data */
    struct bma400_private_driver_data pdd;
};

/**
 * Perform a self test of the device and report on its health.
 *
 * @param The device object.
 * @param The result of the self-test: false if passed, true if failed.
 *
 * @return 0 on success, non-zero on failure.
 */
int bma400_self_test(struct bma400 *bma400, bool *self_test_fail);

/* Get an accelerometer measurement for a single axis */
int bma400_get_axis_accel(struct bma400 *bma400,
                         bma400_axis_t axis,
                         float *accel_data);

/* Get a temperature measurement */
int bma400_get_temp(struct bma400 *bma400, float *temp_c);

/* Get the active status of all interrupts */
int bma400_get_int_status(struct bma400 *bma400,
                          struct bma400_int_status *int_status);

/* Get the size of the FIFO */
int bma400_get_fifo_count(struct bma400 *bma400, uint16_t *fifo_bytes);

/* Get/Set the accelerometer range */
int bma400_get_g_range(struct bma400 *bma400, bma400_g_range_t *g_range);
int bma400_set_g_range(struct bma400 *bma400, bma400_g_range_t g_range);

int bma400_set_filt1_bandwidth(struct bma400 *bma400, bma400_filt1_bandwidth_t bandwidth);

int bma400_set_fifo_cfg(struct bma400 *bma400, struct bma400_fifo_cfg *cfg);
int bma400_set_orient_cfg(struct bma400 *bma400, struct bma400_orient_cfg *cfg);

int bma400_get_step_counter(struct bma400 *bma400, uint32_t *counter);

int bma400_read_fifo(struct bma400 *bma400, uint16_t *fifo_count, struct sensor_accel_data *sad);

int bma400_stream_read(struct sensor *sensor,
                       sensor_type_t sensor_type,
                       sensor_data_func_t data_func,
                       void *read_arg,
                       uint32_t time_ms);

int bma400_get_accel(struct bma400 *bma400, float accel_data[3]);

int bma400_set_power_mode(struct bma400 *bma400, bma400_power_mode_t mode);
int bma400_get_power_mode(struct bma400 *bma400, bma400_power_mode_t *power_mode);

int bma400_set_odr(struct bma400 *bma400, bma400_odr_t odr);
int bma400_set_data_src(struct bma400 *bma400, bma400_data_src_t src);

int bma400_set_acc_cfg(struct bma400 *bma400, struct bma400_acc_cfg *cfg);

/**
 * Configure the sensor.
 *
 * @param bma400 the device object
 * @param cfg    sensor config
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma400_config(struct bma400 *bma400, struct bma400_cfg *cfg);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param dev the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma400_init(struct os_dev *dev, void *arg);

/**
 * Create bma400 device
 *
 * @param bma400 device object to initialize
 * @param name   name of the device
 * @param cfg    device configuration
 *
 * @return 0 on success, non-zero on failure
 */
int bma400_create_dev(struct bma400 *bma400, const char *name,
                      const struct bma400_create_dev_cfg *cfg);

#if MYNEWT_VAL(BMA400_CLI)
/**
 * Initialize the BMA400 shell extensions.
 *
 * @return 0 on success, non-zero on failure.
 */
int
bma400_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
