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

#ifndef __LIS2DH12_H__
#define __LIS2DH12_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LIS2DH12_OM_LOW_POWER                   0x80
#define LIS2DH12_OM_NORMAL                      0x00
#define LIS2DH12_OM_HIGH_RESOLUTION             0x08
#define LIS2DH12_OM_NOT_ALLOWED                 0x88

#define LIS2DH12_DATA_RATE_0HZ                  0x00
#define LIS2DH12_DATA_RATE_1HZ                  0x10
#define LIS2DH12_DATA_RATE_10HZ                 0x20
#define LIS2DH12_DATA_RATE_25HZ                 0x30
#define LIS2DH12_DATA_RATE_50HZ                 0x40
#define LIS2DH12_DATA_RATE_100HZ                0x50
#define LIS2DH12_DATA_RATE_200HZ                0x60
#define LIS2DH12_DATA_RATE_400HZ                0x70
#define LIS2DH12_DATA_RATE_L_1620HZ             0x80
#define LIS2DH12_DATA_RATE_HN_1344HZ_L_5376HZ   0x90

#define LIS2DH12_ST_MODE_DISABLE                0x00
#define LIS2DH12_ST_MODE_MODE0                  0x02
#define LIS2DH12_ST_MODE_MODE1                  0x04

#define LIS2DH12_HPF_M_NORMAL0                  0x00
#define LIS2DH12_HPF_M_REF                      0x01
#define LIS2DH12_HPF_M_NORMAL1                  0x02
#define LIS2DH12_HPF_M_AIE                      0x03

#define LIS2DH12_HPCF_0                         0x00
#define LIS2DH12_HPCF_1                         0x01
#define LIS2DH12_HPCF_2                         0x02
#define LIS2DH12_HPCF_3                         0x03

#define LIS2DH12_FIFO_M_BYPASS                  0x00
#define LIS2DH12_FIFO_M_FIFO                    0x01
#define LIS2DH12_FIFO_M_STREAM                  0x02
#define LIS2DH12_FIFO_M_STREAM_FIFO             0x03

#define LIS2DH12_INT1_CFG_M_OR                   0x0
#define LIS2DH12_INT1_CFG_M_6DM                  0x1
#define LIS2DH12_INT1_CFG_M_AND                  0x2
#define LIS2DH12_INT1_CFG_M_6PR                  0x3

#define LIS2DH12_INT2_CFG_M_OR                   0x0
#define LIS2DH12_INT2_CFG_M_6DM                  0x1
#define LIS2DH12_INT2_CFG_M_AND                  0x2
#define LIS2DH12_INT2_CFG_M_6PR                  0x3

#define LIS2DH12_FS_2G                          0x00
#define LIS2DH12_FS_4G                          0x10
#define LIS2DH12_FS_8G                          0x20
#define LIS2DH12_FS_16G                         0x30

#define LIS2DH12_CLICK_SRC_IA               (1 << 6)
#define LIS2DH12_CLICK_SRC_DCLICK           (1 << 5)
#define LIS2DH12_CLICK_SRC_SCLICK           (1 << 4)
#define LIS2DH12_CLICK_SRC_SIGN             (1 << 3)
#define LIS2DH12_CLICK_SRC_Z                (1 << 2)
#define LIS2DH12_CLICK_SRC_Y                (1 << 1)
#define LIS2DH12_CLICK_SRC_X                (1 << 0)

#define LIS2DH12_CTRL_REG6_I2_CLICK         (1 << 7)
#define LIS2DH12_CTRL_REG6_I2_IA1           (1 << 6)
#define LIS2DH12_CTRL_REG6_I2_IA2           (1 << 5)
#define LIS2DH12_CTRL_REG6_I2_BOOT          (1 << 4)
#define LIS2DH12_CTRL_REG6_I2_ACT           (1 << 3)
#define LIS2DH12_CTRL_REG6_INT_POLARITY     (1 << 1)

/* int1_src */
#define LIS2DH12_INT1_IA                    (1 << 6)
#define LIS2DH12_INT1_ZH                    (1 << 5)
#define LIS2DH12_INT1_ZL                    (1 << 4)
#define LIS2DH12_INT1_YH                    (1 << 3)
#define LIS2DH12_INT1_YL                    (1 << 2)
#define LIS2DH12_INT1_XH                    (1 << 1)
#define LIS2DH12_INT1_XL                    (1 << 0)

/* int2_src */
#define LIS2DH12_INT2_IA                    (1 << 6)
#define LIS2DH12_INT2_ZH                    (1 << 5)
#define LIS2DH12_INT2_ZL                    (1 << 4)
#define LIS2DH12_INT2_YH                    (1 << 3)
#define LIS2DH12_INT2_YL                    (1 << 2)
#define LIS2DH12_INT2_XH                    (1 << 1)
#define LIS2DH12_INT2_XL                    (1 << 0)

/* int_src */
#define LIS2DH12_NOTIF_SRC_INT1_IA          (LIS2DH12_INT1_IA)
#define LIS2DH12_NOTIF_SRC_INT1_ZH          (LIS2DH12_INT1_ZH)
#define LIS2DH12_NOTIF_SRC_INT1_ZL          (LIS2DH12_INT1_ZL)
#define LIS2DH12_NOTIF_SRC_INT1_YH          (LIS2DH12_INT1_YH)
#define LIS2DH12_NOTIF_SRC_INT1_YL          (LIS2DH12_INT1_YL)
#define LIS2DH12_NOTIF_SRC_INT1_XH          (LIS2DH12_INT1_XH)
#define LIS2DH12_NOTIF_SRC_INT1_XL          (LIS2DH12_INT1_XL)
#define LIS2DH12_NOTIF_SRC_INT2_IA          (LIS2DH12_INT2_IA << 8)
#define LIS2DH12_NOTIF_SRC_INT2_ZH          (LIS2DH12_INT2_ZH << 8)
#define LIS2DH12_NOTIF_SRC_INT2_ZL          (LIS2DH12_INT2_ZL << 8)
#define LIS2DH12_NOTIF_SRC_INT2_YH          (LIS2DH12_INT2_YH << 8)
#define LIS2DH12_NOTIF_SRC_INT2_YL          (LIS2DH12_INT2_YL << 8)
#define LIS2DH12_NOTIF_SRC_INT2_XH          (LIS2DH12_INT2_XH << 8)
#define LIS2DH12_NOTIF_SRC_INT2_XL          (LIS2DH12_INT2_XL << 8)

/* int1 pin config - generate either data ready or in */
#define LIS2DH12_CTRL_REG3_I1_CLICK         (1 << 7)
#define LIS2DH12_CTRL_REG3_I1_IA1           (1 << 6)
#define LIS2DH12_CTRL_REG3_I1_IA2           (1 << 5)
#define LIS2DH12_CTRL_REG3_I1_ZYXDA         (1 << 4)
#define LIS2DH12_CTRL_REG3_I1_WTM           (1 << 2)
#define LIS2DH12_CTRL_REG3_I1_OVERRUN       (1 << 1)

enum lis2dh12_read_mode {
    LIS2DH12_READ_M_POLL = 0,
    LIS2DH12_READ_M_STREAM = 1,
};

struct lis2dh12_notif_cfg {
    sensor_event_type_t event;
    uint8_t int_num;
    uint16_t notif_src;
    uint8_t int_cfg;
};

struct lis2dh12_tap_settings {
    uint8_t en_xs  : 1; // Interrupt on X single tap
    uint8_t en_ys  : 1; // Interrupt on Y single tap
    uint8_t en_zs  : 1; // Interrupt on Z single tap
    uint8_t en_xd  : 1; // Interrupt on X double tap
    uint8_t en_yd  : 1; // Interrupt on Y double tap
    uint8_t en_zd  : 1; // Interrupt on Z double tap
    uint8_t hpf    : 1; // High pass filter enable

    /* ths data is 7 bits, fs = +-2g */
    int8_t click_ths;
    /* shock is maximum time data can be over threshold to register as tap
       LSB = 1/ODR */
    int8_t time_limit;

    /* latency is time between taps in double tap, LSB = 1/ODR */
    uint8_t time_latency;
    /* quiet is time after tap data is just below threshold
       LSB = 1/ODR */
    uint8_t time_window;
};

struct lis2dh12_int_cfg {
    uint8_t cfg;
    uint8_t ths;
    uint8_t dur;
};

/* Read mode configuration */
struct lis2dh12_read_mode_cfg {
    enum lis2dh12_read_mode mode;
    uint8_t int_num:1;
    uint8_t int_cfg;
};

struct lis2dh12_cfg {
    uint8_t lc_rate;
    uint8_t lc_fs;

    uint8_t reference;
    /* Tap config */
    struct lis2dh12_tap_settings tap;

    /* Read mode config */
    struct lis2dh12_read_mode_cfg read_mode;

    /* Notif config */
    struct lis2dh12_notif_cfg *notif_cfg;
    uint8_t max_num_notif;

    struct lis2dh12_int_cfg int_cfg[2];

    uint8_t xen           : 1;
    uint8_t yen           : 1;
    uint8_t zen           : 1;
    uint8_t hp_mode       : 2;
    uint8_t hp_cut_off    : 2;
    uint8_t hp_fds        : 1;
    uint8_t hp_click      : 1;
    uint8_t hp_ia1        : 1;
    uint8_t hp_ia2        : 1;
    uint8_t bdu           : 1;
    uint8_t latch_int1    : 1;
    uint8_t latch_int2    : 1;
    uint8_t d4d_int1      : 1;
    uint8_t d4d_int2      : 1;
    uint8_t int_pp_od     : 1;

    /* Power mode */
    uint8_t power_mode;

    /* Fifo config */
    uint8_t fifo_mode;
    uint8_t fifo_watermark;

    /* Sleep/wakeup settings */
    uint8_t act_ths;
    uint8_t act_dur;

    uint8_t lc_pull_up_disc;
    sensor_type_t lc_s_mask;
};

enum lis2dh12_axis {
    LIS2DH12_AXIS_X = 0,
    LIS2DH12_AXIS_Y,
    LIS2DH12_AXIS_Z,
    LIS2DH12_AXIS_MAX
};

/* Used to track interrupt state to wake any present waiters */
struct lis2dh12_int {
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

/* Private per driver data */
struct lis2dh12_pdd {
    /* Notification event context */
    struct sensor_notify_ev_ctx notify_ctx;
    /* Inetrrupt state */
    struct lis2dh12_int *interrupt;
    /* Interrupt enabled on INT1 and INT2 pin (CTRL_REG3 and CTRL_REG6) */
    uint8_t int_enable[2];
    /* State of pin for sleep/wake up handling */
    uint8_t int2_pin_state;
};

struct lis2dh12 {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    union {
        struct bus_i2c_node i2c_node;
        struct bus_spi_node spi_node;
    };
#else
    struct os_dev dev;
#endif
    struct sensor sensor;
    struct lis2dh12_cfg cfg;
    struct lis2dh12_int intr;
    os_time_t last_read_time;
    struct lis2dh12_pdd pdd;
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bool node_is_spi;
#endif
};

/**
 * Expects to be called back through os_dev_create().
 *
 * @param ptr to the device object associated with this accelerometer
 * @param argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int lis2dh12_init(struct os_dev *dev, void *arg);

/**
 * Sets the full scale selection
 *
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */

int
lis2dh12_set_full_scale(struct sensor_itf *itf, uint8_t fs);

/**
 * Gets the full scale selection
 *
 * @param The sensor interface
 * @param ptr to fs
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_full_scale(struct sensor_itf *itf, uint8_t *fs);

/**
 * Sets the rate
 *
 * @param The sensor interface
 * @param The rate
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dh12_set_rate(struct sensor_itf *itf, uint8_t rate);

/**
 * Gets the current rate
 *
 * @param The sensor interface
 * @param ptr to rate read from the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int lis2dh12_get_rate(struct sensor_itf *itf, uint8_t *rate);

/**
 * Enable channels
 *
 * @param sensor interface
 * @param chan
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_chan_enable(struct sensor_itf *itf, uint8_t chan);

/**
 * Get chip ID
 *
 * @param sensor interface
 * @param ptr to chip id to be filled up
 */
int
lis2dh12_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id);

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int lis2dh12_config(struct lis2dh12 *, struct lis2dh12_cfg *);

/**
 * Calculates the acceleration in m/s^2 from mg
 *
 * @param raw acc value
 * @param float ptr to return calculated value
 * @param scale to calculate against
 */
void
lis2dh12_calc_acc_ms2(int16_t raw_acc, float *facc);

/**
 * Pull up disconnect
 *
 * @param The sensor interface
 * @param disconnect pull up
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_pull_up_disc(struct sensor_itf *itf, uint8_t disconnect);

/**
 * Reset lis2dh12
 *
 * @param The sensor interface
 */
int
lis2dh12_reset(struct sensor_itf *itf);

/**
 * Set reference threshold
 *
 * @param the sensor interface
 * @param threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_ref_thresh(struct sensor_itf *itf, uint8_t ths);

/**
 * Enable interrupt 2
 *
 * @param the sensor interface
 * @param events to enable int for
 */
int
lis2dh12_enable_int2(struct sensor_itf *itf, uint8_t reg);

/**
 * Enable interrupt 1
 *
 * @param the sensor interface
 * @param events to enable int for
 */
int
lis2dh12_enable_int1(struct sensor_itf *itf, uint8_t reg);

/**
 * Set interrupt threshold for int 1
 *
 * @param the sensor interface
 * @param threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_thresh(struct sensor_itf *itf, uint8_t ths);

/**
 * Set interrupt threshold for int 2
 *
 * @param the sensor interface
 * @param threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_thresh(struct sensor_itf *itf, uint8_t ths);

/**
 * Clear interrupt 1
 *
 * @param the sensor interface
 * @param src pointer to return interrupt source in
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_clear_int1(struct sensor_itf *itf, uint8_t *src);

/**
 * Clear interrupt 2
 *
 * @param the sensor interface
 * @param src pointer to return interrupt source in
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_clear_int2(struct sensor_itf *itf, uint8_t *src);

/**
 * Set interrupt pin configuration for interrupt 1
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set interrupt pin configuration for interrupt 2
 *
 * @param the sensor interface
 * @param config
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set interrupt 1 duration
 *
 * @param duration in N/ODR units
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int1_duration(struct sensor_itf *itf, uint8_t dur);

/**
 * Set interrupt 2 duration
 *
 * @param duration in N/ODR units
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_int2_duration(struct sensor_itf *itf, uint8_t dur);

/**
 * Disable interrupt 1
 *
 * @param the sensor interface
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_disable_int1(struct sensor_itf *itf);

/**
 * Disable interrupt 2
 *
 * @param the sensor interface
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_disable_int2(struct sensor_itf *itf);

/**
 * Set high pass filter cfg
 *
 * @param the sensor interface
 * @param filter register settings
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_hpf_cfg(struct sensor_itf *itf, uint8_t reg);

/**
 * Set operating mode
 *
 * @param the sensor interface
 * @param mode CTRL_REG1[3:0]:CTRL_REG4[3:0]
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_op_mode(struct sensor_itf *itf, uint8_t mode);

/**
 * Get operating mode
 *
 * @param the sensor interface
 * @param ptr to mode
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_op_mode(struct sensor_itf *itf, uint8_t *mode);

/**
 * Clear click interrupt
 *
 * @param the sensor interface
 * @param src pointer for active interrupts
 */
int
lis2dh12_clear_click(struct sensor_itf *itf, uint8_t *src);

/**
 * Set click configuration register
 *
 * @param the sensor interface
 * @param value to put in CLICK_CFG register
 */
int
lis2dh12_set_click_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set click threshold value
 *
 * @param the sensor interface
 * @param value click threshold value
 */
int
lis2dh12_set_click_threshold(struct sensor_itf *itf, uint8_t cfg);

/**
 * Set click time limit register
 *
 * This sets maximum time interval that can elapse between the start of
 * the click-detection procedure and when the acceleration falls below
 * thhreshold. 1 LSB = 1/ODR
 *
 * @param the sensor interface
 * @param value to put in TIME_LIMIT register
 */
int
lis2dh12_set_click_time_limit(struct sensor_itf *itf, uint8_t limit);

/**
 * Set click time latency register
 *
 * This function sets time interval that starts after the first click detection
 * where the click-detection procedure is disabled. This is relevant for
 * double click detection only.
 *
 * 1 LSB = 1/ODR
 *
 * @param the sensor interface
 * @param value to put in TIME_LATENCY register
 */
int
lis2dh12_set_click_time_latency(struct sensor_itf *itf, uint8_t latency);

/**
 * Set click time window register
 *
 * This function sets the maximum interval of time that can elapse after end of
 * the latency interval in which click-detection procedure can start.
 * This is relevant for double click detection only.
 *
 * 1 LSB = 1/ODR
 *
 * @param the sensor interface
 * @param value to put in TIME_WINDOW register
 */
int
lis2dh12_set_click_time_window(struct sensor_itf *itf, uint8_t window);

/**
 * Set activity threshold
 *
 * @param the sensor interface
 * @param threshold 0..127 threshold value for activity/sleep detection
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_activity_threshold(struct sensor_itf *itf, uint8_t threshold);

/**
 * Set activity duration
 *
 * @param the sensor interface
 * @param duration Sleep-to-wake, return-to-sleep duration.
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_set_activity_duration(struct sensor_itf *itf, uint8_t duration);

/**
 * Get number of samples in FIFO
 *
 * @param the sensor interface
 * @param Pointer to return number of samples
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_get_fifo_samples(struct sensor_itf *itf, uint8_t *samples);

/**
 * Run Self test on sensor to see if any axis has an error
 *
 * Self test sequence as described in lis2dh12 application note AN5005.
 *
 * @param the sensor interface
 * @param pointer to return test result in (0 on pass, 0x1 Failure on X-Axis,
          0x2 Failure on Y-Axis, 0x4 Failure on Z-Axis)
 *
 * @return 0 on sucess, non-zero on failure
 */
int
lis2dh12_run_self_test(struct sensor_itf *itf, int *result);

#if MYNEWT_VAL(LIS2DH12_CLI)
int lis2dh12_shell_init(void);
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Create I2C bus node for LIS2DH12 sensor
 *
 * @param node        Bus node
 * @param name        Device name
 * @param i2c_cfg     I2C node configuration
 * @param sensor_itf  Sensors interface
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                               const struct bus_i2c_node_cfg *i2c_cfg,
                               struct sensor_itf *sensor_itf);

/**
 * Create SPI bus node for LIS2DH12 sensor
 *
 * @param node        Bus node
 * @param name        Device name
 * @param spi_cfg     SPI node configuration
 * @param sensor_itf  Sensors interface
 *
 * @return 0 on success, non-zero on failure
 */
int
lis2dh12_create_spi_sensor_dev(struct bus_spi_node *node, const char *name,
                               const struct bus_spi_node_cfg *spi_cfg,
                               struct sensor_itf *sensor_itf);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __LIS2DH12_H__ */
