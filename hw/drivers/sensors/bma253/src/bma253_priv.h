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

#ifndef __BMA253_PRIV_H__
#define __BMA253_PRIV_H__

#include "bma253/bma253.h"

#ifdef __cplusplus
#extern "C" {
#endif

/*
 * Full register map:
 */

#define REG_ADDR_BGW_CHIPID    0x00    /*    r            */
/* RESERVED */                         /*                 */
#define REG_ADDR_ACCD_X_LSB    0x02    /*    r            */
#define REG_ADDR_ACCD_X_MSB    0x03    /*    r            */
#define REG_ADDR_ACCD_Y_LSB    0x04    /*    r            */
#define REG_ADDR_ACCD_Y_MSB    0x05    /*    r            */
#define REG_ADDR_ACCD_Z_LSB    0x06    /*    r            */
#define REG_ADDR_ACCD_Z_MSB    0x07    /*    r            */
#define REG_ADDR_ACCD_TEMP     0x08    /*    r            */
#define REG_ADDR_INT_STATUS_0  0x09    /*    r            */
#define REG_ADDR_INT_STATUS_1  0x0A    /*    r            */
#define REG_ADDR_INT_STATUS_2  0x0B    /*    r            */
#define REG_ADDR_INT_STATUS_3  0x0C    /*    r            */
/* RESERVED */                         /*                 */
#define REG_ADDR_FIFO_STATUS   0x0E    /*    r            */
#define REG_ADDR_PMU_RANGE     0x0F    /*    rw           */
#define REG_ADDR_PMU_BW        0x10    /*    rw           */
#define REG_ADDR_PMU_LPW       0x11    /*    rw           */
#define REG_ADDR_PMU_LOW_POWER 0x12    /*    rw           */
#define REG_ADDR_ACCD_HBW      0x13    /*    rw           */
#define REG_ADDR_BGW_SOFTRESET 0x14    /*     w           */
/* RESERVED */                         /*                 */
#define REG_ADDR_INT_EN_0      0x16    /*    rw           */
#define REG_ADDR_INT_EN_1      0x17    /*    rw           */
#define REG_ADDR_INT_EN_2      0x18    /*    rw           */
#define REG_ADDR_INT_MAP_0     0x19    /*    rw           */
#define REG_ADDR_INT_MAP_1     0x1A    /*    rw           */
#define REG_ADDR_INT_MAP_2     0x1B    /*    rw           */
/* RESERVED */                         /*                 */
/* RESERVED */                         /*                 */
#define REG_ADDR_INT_SRC       0x1E    /*    rw           */
/* RESERVED */                         /*                 */
#define REG_ADDR_INT_OUT_CTRL  0x20    /*    rw           */
#define REG_ADDR_INT_RST_LATCH 0x21    /*    rw           */
#define REG_ADDR_INT_0         0x22    /*    rw           */
#define REG_ADDR_INT_1         0x23    /*    rw           */
#define REG_ADDR_INT_2         0x24    /*    rw           */
#define REG_ADDR_INT_3         0x25    /*    rw           */
#define REG_ADDR_INT_4         0x26    /*    rw           */
#define REG_ADDR_INT_5         0x27    /*    rw           */
#define REG_ADDR_INT_6         0x28    /*    rw           */
#define REG_ADDR_INT_7         0x29    /*    rw           */
#define REG_ADDR_INT_8         0x2A    /*    rw           */
#define REG_ADDR_INT_9         0x2B    /*    rw           */
#define REG_ADDR_INT_A         0x2C    /*    rw           */
#define REG_ADDR_INT_B         0x2D    /*    rw           */
#define REG_ADDR_INT_C         0x2E    /*    rw           */
#define REG_ADDR_INT_D         0x2F    /*    rw           */
#define REG_ADDR_FIFO_CONFIG_0 0x30    /*    rw           */
/* RESERVED */                         /*                 */
#define REG_ADDR_PMU_SELF_TEST 0x32    /*    rw           */
#define REG_ADDR_TRIM_NVM_CTRL 0x33    /*    rw           */
#define REG_ADDR_BGW_SPI3_WDT  0x34    /*    rw           */
/* RESERVED */                         /*                 */
#define REG_ADDR_OFC_CTRL      0x36    /*    rw           */
#define REG_ADDR_OFC_SETTING   0x37    /*    rw           */
#define REG_ADDR_OFC_OFFSET_X  0x38    /*    rw    nvm    */
#define REG_ADDR_OFC_OFFSET_Y  0x39    /*    rw    nvm    */
#define REG_ADDR_OFC_OFFSET_Z  0x3A    /*    rw    nvm    */
#define REG_ADDR_TRIM_GP0      0x3B    /*    rw    nvm    */
#define REG_ADDR_TRIM_GP1      0x3C    /*    rw    nvm    */
/* RESERVED */                         /*                 */
#define REG_ADDR_FIFO_CONFIG_1 0x3E    /*    rw           */
#define REG_ADDR_FIFO_DATA     0x3F    /*    r            */

/* Magical value that the chip always reports as its ID */
#define REG_VALUE_CHIP_ID    0xFA
/* Magical value that is used to initiate a full reset */
#define REG_VALUE_SOFT_RESET 0xB6

/* Get the chip ID */
int
bma253_get_chip_id(const struct bma253 * bma253,
                   uint8_t * chip_id);

/* All three axis types */
enum axis {
    AXIS_X   = 0,
    AXIS_Y   = 1,
    AXIS_Z   = 2,
    AXIS_ALL = 3,
};

/* A single accelerometer measurement for one axis */
struct accel_data {
    float accel_g;
    bool new_data;
};

/* Get an accelerometer measurement for a single axis */
int
bma253_get_accel(const struct bma253 * bma253,
                 enum bma253_g_range g_range,
                 enum axis axis,
                 struct accel_data * accel_data);

/* Get a temperature measurement */
int
bma253_get_temp(const struct bma253 * bma253,
                float * temp_c);

/* Which direction in an axis was this interrupt triggered on */
enum axis_trigger_sign {
    AXIS_TRIGGER_SIGN_POS = 0,
    AXIS_TRIGGER_SIGN_NEG = 1,
};

/* Which axis was this interrupt triggered on */
struct axis_trigger {
    enum axis_trigger_sign sign;
    enum axis axis;
    bool axis_known;
};

/* Active status of all interrupts */
struct int_status {
    bool flat_int_active;
    bool orient_int_active;
    bool s_tap_int_active;
    bool d_tap_int_active;
    bool slow_no_mot_int_active;
    bool slope_int_active;
    bool high_g_int_active;
    bool low_g_int_active;
    bool data_int_active;
    bool fifo_wmark_int_active;
    bool fifo_full_int_active;
    struct axis_trigger tap_trigger;
    struct axis_trigger slope_trigger;
    bool device_is_flat;
    bool device_is_down;
    enum bma253_orient_xy device_orientation;
    struct axis_trigger high_g_trigger;
};

/* Get the active status of all interrupts */
int
bma253_get_int_status(const struct bma253 * bma253,
                      struct int_status * int_status);

/* Get the status and size of the FIFO */
int
bma253_get_fifo_status(const struct bma253 * bma253,
                       bool * overrun,
                       uint8_t * frame_counter);

/* Get/Set the accelerometer range */
int
bma253_get_g_range(const struct bma253 * bma253,
                   enum bma253_g_range * g_range);
int
bma253_set_g_range(const struct bma253 * bma253,
                   enum bma253_g_range g_range);

/* Get/Set the filter output bandwidth */
int
bma253_get_filter_bandwidth(const struct bma253 * bma253,
                            enum bma253_filter_bandwidth * filter_bandwidth);
int
bma253_set_filter_bandwidth(const struct bma253 * bma253,
                            enum bma253_filter_bandwidth filter_bandwidth);

/* Whether the sleep timer is locked to events or to time */
enum sleep_timer {
    SLEEP_TIMER_EVENT_DRIVEN         = 0,
    SLEEP_TIMER_EQUIDISTANT_SAMPLING = 1,
};

/* Power settings of the device */
struct power_settings {
    enum bma253_power_mode power_mode;
    enum bma253_sleep_duration sleep_duration;
    enum sleep_timer sleep_timer;
};

/* Get/Set the power settings of the device */
int
bma253_get_power_settings(const struct bma253 * bma253,
                          struct power_settings * power_settings);
int
bma253_set_power_settings(const struct bma253 * bma253,
                          const struct power_settings * power_settings);

/* Get/Set the data register settings */
int
bma253_get_data_acquisition(const struct bma253 * bma253,
                            bool * unfiltered_reg_data,
                            bool * disable_reg_shadow);
int
bma253_set_data_acquisition(const struct bma253 * bma253,
                            bool unfiltered_reg_data,
                            bool disable_reg_shadow);

/* Kick off a full soft reset of the device */
int
bma253_set_softreset(const struct bma253 * bma253);

/* Enable settings of all interupts */
struct int_enable {
    bool flat_int_enable;
    bool orient_int_enable;
    bool s_tap_int_enable;
    bool d_tap_int_enable;
    bool slope_z_int_enable;
    bool slope_y_int_enable;
    bool slope_x_int_enable;
    bool fifo_wmark_int_enable;
    bool fifo_full_int_enable;
    bool data_int_enable;
    bool low_g_int_enable;
    bool high_g_z_int_enable;
    bool high_g_y_int_enable;
    bool high_g_x_int_enable;
    bool no_motion_select;
    bool slow_no_mot_z_int_enable;
    bool slow_no_mot_y_int_enable;
    bool slow_no_mot_x_int_enable;
};

/* Get/Set the enable settings of all interrupts */
int
bma253_get_int_enable(const struct bma253 * bma253,
                      struct int_enable * int_enable);
int
bma253_set_int_enable(const struct bma253 * bma253,
                      const struct int_enable * int_enable);

/* Which physical device pin is a given interrupt routed to */
enum int_route {
    INT_ROUTE_NONE  = 0,
    INT_ROUTE_PIN_1 = 1,
    INT_ROUTE_PIN_2 = 2,
    INT_ROUTE_BOTH  = 3,
};

enum bma253_int_num {
    INT1_PIN,
    INT2_PIN
};

/* Pin routing settings of all interrupts */
struct int_routes {
    enum int_route flat_int_route;
    enum int_route orient_int_route;
    enum int_route s_tap_int_route;
    enum int_route d_tap_int_route;
    enum int_route slow_no_mot_int_route;
    enum int_route slope_int_route;
    enum int_route high_g_int_route;
    enum int_route low_g_int_route;
    enum int_route fifo_wmark_int_route;
    enum int_route fifo_full_int_route;
    enum int_route data_int_route;
};

/* Get/Set the pin routing settings of all interrupts */
int
bma253_get_int_routes(const struct bma253 * bma253,
                      struct int_routes * int_routes);
int
bma253_set_int_routes(const struct bma253 * bma253,
                      const struct int_routes * int_routes);

/* Whether each interrupt uses filtered or unfiltered data */
struct int_filters {
    bool unfiltered_data_int;
    bool unfiltered_tap_int;
    bool unfiltered_slow_no_mot_int;
    bool unfiltered_slope_int;
    bool unfiltered_high_g_int;
    bool unfiltered_low_g_int;
};

/* Get/Set the filtered data settings of all interrupts */
int
bma253_get_int_filters(const struct bma253 * bma253,
                       struct int_filters * int_filters);
int
bma253_set_int_filters(const struct bma253 * bma253,
                       const struct int_filters * int_filters);

/* Drive mode of the interrupt pins */
enum int_pin_output {
    INT_PIN_OUTPUT_PUSH_PULL  = 0,
    INT_PIN_OUTPUT_OPEN_DRAIN = 1,
};

/* Active mode of the interrupt pins */
enum int_pin_active {
    INT_PIN_ACTIVE_LOW  = 0,
    INT_PIN_ACTIVE_HIGH = 1,
};

/* Electrical settings of both interrupt pins */
struct int_pin_electrical {
    enum int_pin_output pin1_output;
    enum int_pin_active pin1_active;
    enum int_pin_output pin2_output;
    enum int_pin_active pin2_active;
};

/* Get/Set the electrical settings of both interrupt pins */
int
bma253_get_int_pin_electrical(const struct bma253 * bma253,
                              struct int_pin_electrical * electrical);
int
bma253_set_int_pin_electrical(const struct bma253 * bma253,
                              const struct int_pin_electrical * electrical);

/* Length of time that an interrupt condition should be latched active */
enum int_latch {
    INT_LATCH_NON_LATCHED       = 0,
    INT_LATCH_LATCHED           = 1,
    INT_LATCH_TEMPORARY_250_US  = 2,
    INT_LATCH_TEMPORARY_500_US  = 3,
    INT_LATCH_TEMPORARY_1_MS    = 4,
    INT_LATCH_TEMPORARY_12_5_MS = 5,
    INT_LATCH_TEMPORARY_25_MS   = 6,
    INT_LATCH_TEMPORARY_50_MS   = 7,
    INT_LATCH_TEMPORARY_250_MS  = 8,
    INT_LATCH_TEMPORARY_500_MS  = 9,
    INT_LATCH_TEMPORARY_1_S     = 10,
    INT_LATCH_TEMPORARY_2_S     = 11,
    INT_LATCH_TEMPORARY_4_S     = 12,
    INT_LATCH_TEMPORARY_8_S     = 13,
};

/* Get/Set the interrupt condition latch time */
int
bma253_get_int_latch(const struct bma253 * bma253,
                     enum int_latch * int_latch);
int
bma253_set_int_latch(const struct bma253 * bma253,
                     bool reset_ints,
                     enum int_latch int_latch);

/* Settings for the low-g interrupt */
struct low_g_int_cfg {
    uint16_t delay_ms;
    float thresh_g;
    float hyster_g;
    bool axis_summing;
};

/* Get/Set the low-g interrupt settings */
int
bma253_get_low_g_int_cfg(const struct bma253 * bma253,
                         struct low_g_int_cfg * low_g_int_cfg);
int
bma253_set_low_g_int_cfg(const struct bma253 * bma253,
                         const struct low_g_int_cfg * low_g_int_cfg);

/* Settings for the high-g interrupt */
struct high_g_int_cfg {
    float hyster_g;
    uint16_t delay_ms;
    float thresh_g;
};

/* Get/Set the high-g interrupt settings */
int
bma253_get_high_g_int_cfg(const struct bma253 * bma253,
                          enum bma253_g_range g_range,
                          struct high_g_int_cfg * high_g_int_cfg);
int
bma253_set_high_g_int_cfg(const struct bma253 * bma253,
                          enum bma253_g_range g_range,
                          const struct high_g_int_cfg * high_g_int_cfg);

/* Settings for the slow/no-motion interrupt */
struct slow_no_mot_int_cfg {
    uint16_t duration_p_or_s;
    float thresh_g;
};

/* Get/Set the slow/no-motion interrupt settings */
int
bma253_get_slow_no_mot_int_cfg(const struct bma253 * bma253,
                               bool no_motion_select,
                               enum bma253_g_range g_range,
                               struct slow_no_mot_int_cfg * slow_no_mot_int_cfg);
int
bma253_set_slow_no_mot_int_cfg(const struct bma253 * bma253,
                               bool no_motion_select,
                               enum bma253_g_range g_range,
                               const struct slow_no_mot_int_cfg * slow_no_mot_int_cfg);

/* Settings for the slope interrupt */
struct slope_int_cfg {
    uint8_t duration_p;
    float thresh_g;
};

/* Get/Set the slope interrupt settings */
int
bma253_get_slope_int_cfg(const struct bma253 * bma253,
                         enum bma253_g_range g_range,
                         struct slope_int_cfg * slope_int_cfg);
int
bma253_set_slope_int_cfg(const struct bma253 * bma253,
                         enum bma253_g_range g_range,
                         const struct slope_int_cfg * slope_int_cfg);

/* Settings for the double/single tap interrupt */
struct tap_int_cfg {
    enum bma253_tap_quiet tap_quiet;
    enum bma253_tap_shock tap_shock;
    enum bma253_d_tap_window d_tap_window;
    enum bma253_tap_wake_samples tap_wake_samples;
    float thresh_g;
};

/* Get/Set the double/single tap interrupt settings */
int
bma253_get_tap_int_cfg(const struct bma253 * bma253,
                       enum bma253_g_range g_range,
                       struct tap_int_cfg * tap_int_cfg);
int
bma253_set_tap_int_cfg(const struct bma253 * bma253,
                       enum bma253_g_range g_range,
                       const struct tap_int_cfg * tap_int_cfg);

/* Settings for the orientation interrupt */
struct orient_int_cfg {
    float hyster_g;
    enum bma253_orient_blocking orient_blocking;
    enum bma253_orient_mode orient_mode;
    bool signal_up_dn;
    uint8_t blocking_angle;
};

/* Get/Set the orientation interrupt settings */
int
bma253_get_orient_int_cfg(const struct bma253 * bma253,
                          struct orient_int_cfg * orient_int_cfg);
int
bma253_set_orient_int_cfg(const struct bma253 * bma253,
                          const struct orient_int_cfg * orient_int_cfg);

/* Hold time for flat condition */
enum flat_hold {
    FLAT_HOLD_0_MS    = 0,
    FLAT_HOLD_512_MS  = 1,
    FLAT_HOLD_1024_MS = 2,
    FLAT_HOLD_2048_MS = 3,
};

/* Settings for the flat interrupt */
struct flat_int_cfg {
    uint8_t flat_angle;
    enum flat_hold flat_hold;
    uint8_t flat_hyster;
    bool hyster_enable;
};

/* Get/Set the flat interrupt settings */
int
bma253_get_flat_int_cfg(const struct bma253 * bma253,
                        struct flat_int_cfg * flat_int_cfg);
int
bma253_set_flat_int_cfg(const struct bma253 * bma253,
                        const struct flat_int_cfg * flat_int_cfg);

/* Get/Set the FIFO watermark level */
int
bma253_get_fifo_wmark_level(const struct bma253 * bma253,
                            uint8_t * wmark_level);
int
bma253_set_fifo_wmark_level(const struct bma253 * bma253,
                            uint8_t wmark_level);

/* Amplitude of a self-test induced acceleration */
enum self_test_ampl {
    SELF_TEST_AMPL_HIGH = 0,
    SELF_TEST_AMPL_LOW  = 1,
};

/* Direction of a self-test induced acceleration */
enum self_test_sign {
    SELF_TEST_SIGN_NEGATIVE = 0,
    SELF_TEST_SIGN_POSITIVE = 1,
};

/* Settings for the self-test functionality */
struct self_test_cfg {
    enum self_test_ampl self_test_ampl;
    enum self_test_sign self_test_sign;
    enum axis self_test_axis;
    bool self_test_enabled;
};

/* Get/Set the self-test settings */
int
bma253_get_self_test_cfg(const struct bma253 * bma253,
                         struct self_test_cfg * self_test_cfg);
int
bma253_set_self_test_cfg(const struct bma253 * bma253,
                         const struct self_test_cfg * self_test_cfg);

/* Get/Set the NVM reset/write control values */
int
bma253_get_nvm_control(const struct bma253 * bma253,
                       uint8_t * remaining_cycles,
                       bool * load_from_nvm,
                       bool * nvm_is_ready,
                       bool * nvm_unlocked);
int
bma253_set_nvm_control(const struct bma253 * bma253,
                       bool load_from_nvm,
                       bool store_into_nvm,
                       bool nvm_unlocked);

/* Length of time before the I2C watchdog fires */
enum i2c_watchdog {
    I2C_WATCHDOG_DISABLED = 0,
    I2C_WATCHDOG_1_MS     = 1,
    I2C_WATCHDOG_50_MS    = 2,
};

/* Get/Set the I2C watchdog settings */
int
bma253_get_i2c_watchdog(const struct bma253 * bma253,
                        enum i2c_watchdog * i2c_watchdog);
int
bma253_set_i2c_watchdog(const struct bma253 * bma253,
                        enum i2c_watchdog i2c_watchdog);

/* Offset compensation settings used in slow compensation mode */
struct slow_ofc_cfg {
    bool ofc_z_enabled;
    bool ofc_y_enabled;
    bool ofc_x_enabled;
    bool high_bw_cut_off;
};

/* Get/Set the fast & slow offset compensation mode settings, and reset all
 * offset compensation values back to NVM defaults */
int
bma253_get_fast_ofc_cfg(const struct bma253 * bma253,
                        bool * fast_ofc_ready,
                        enum bma253_offset_comp_target * ofc_target_z,
                        enum bma253_offset_comp_target * ofc_target_y,
                        enum bma253_offset_comp_target * ofc_target_x);
int
bma253_set_fast_ofc_cfg(const struct bma253 * bma253,
                        enum axis fast_ofc_axis,
                        enum bma253_offset_comp_target fast_ofc_target,
                        bool trigger_fast_ofc);
int
bma253_get_slow_ofc_cfg(const struct bma253 * bma253,
                        struct slow_ofc_cfg * slow_ofc_cfg);
int
bma253_set_slow_ofc_cfg(const struct bma253 * bma253,
                        const struct slow_ofc_cfg * slow_ofc_cfg);
int
bma253_set_ofc_reset(const struct bma253 * bma253);

/* Get/Set the offset compensation value for a specific axis */
int
bma253_get_ofc_offset(const struct bma253 * bma253,
                      enum axis axis,
                      float * offset_g);
int
bma253_set_ofc_offset(const struct bma253 * bma253,
                      enum axis axis,
                      float offset_g);

/* General purpose non-volatile data registers */
enum saved_data_addr {
    SAVED_DATA_ADDR_0 = 0,
    SAVED_DATA_ADDR_1 = 1,
};

/* Get/Set the data stored in general purpose non-volatile registers */
int
bma253_get_saved_data(const struct bma253 * bma253,
                      enum saved_data_addr saved_data_addr,
                      uint8_t * saved_data_val);
int
bma253_set_saved_data(const struct bma253 * bma253,
                      enum saved_data_addr saved_data_addr,
                      uint8_t saved_data_val);

/* Mode that the FIFO is running in */
enum fifo_mode {
    FIFO_MODE_BYPASS = 0,
    FIFO_MODE_FIFO   = 1,
    FIFO_MODE_STREAM = 2,
};

/* Measurements for which axis to capture into the FIFO */
enum fifo_data {
    FIFO_DATA_X_AND_Y_AND_Z = 0,
    FIFO_DATA_X_ONLY        = 1,
    FIFO_DATA_Y_ONLY        = 2,
    FIFO_DATA_Z_ONLY        = 3,
};

/* FIFO capture and behavior settings */
struct fifo_cfg {
    enum fifo_mode fifo_mode;
    enum fifo_data fifo_data;
};

/* Get/Set the FIFO capture and behavior settings */
int
bma253_get_fifo_cfg(const struct bma253 * bma253,
                    struct fifo_cfg * fifo_cfg);
int
bma253_set_fifo_cfg(const struct bma253 * bma253,
                    const struct fifo_cfg * fifo_cfg);

/* Read a single multi-axis data frame from the FIFO */
int
bma253_get_fifo(const struct bma253 * bma253,
                enum bma253_g_range g_range,
                enum fifo_data fifo_data,
                struct accel_data * accel_data);

#ifdef __cplusplus
}
#endif

#endif
