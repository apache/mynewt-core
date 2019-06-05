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

#ifndef __LSM6DSO_PRIV_H__
#define __LSM6DSO_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Common defines for Acc and Gyro sensors */
#define LSM6DSO_EN_BIT			0x01
#define LSM6DSO_DIS_BIT			0x00

/*
 * Access to embedded sensor hub register bank
 *
 * FUNC_CFG_ACCESS - Enable access to the embedded functions registers
 * SHUB_REG_ACCESS - Enable access to the sensor hub registers
 */
#define LSM6DSO_FUNC_CFG_ACC_ADDR	0x01
#define LSM6DSO_FUNC_CFG_ACCESS_MASK	0x80
#define LSM6DSO_SHUB_REG_ACCESS_MASK	0x40

/* FIFO decimator registers and bitmask */
#define LSM6DSO_FIFO_CTRL1_ADDR		0x07
#define LSM6DSO_FIFO_CTRL2_ADDR		0x08
#define LSM6DSO_FIFO_WTM_MASK		0x01ff

/*
 * FIFO control register 3
 *
 * BDR_GY_[3:0] - Selects Batching Data Rate for gyroscope data.
 * BDR_XL_[3:0] - Selects Batching Data Rate for accelerometer data.
 */
#define LSM6DSO_FIFO_CTRL3_ADDR		0x09
#define LSM6DSO_FIFO_BDR_XL_MASK	0x0f
#define LSM6DSO_FIFO_BDR_GY_MASK	0xf0

/*
 * FIFO control register 4
 *
 * FIFO_MODE[2:0]     - FIFO mode selection
 * ODR_T_BATCH_[1:0]  - Selects batching data rate for temperature data
 * DEC_TS_BATCH_[1:0] - Selects decimation for timestamp batching in FIFO
 */
#define LSM6DSO_FIFO_CTRL4_ADDR		0x0a
#define LSM6DSO_FIFO_MODE_MASK		0x07
#define LSM6DSO_FIFO_ODR_T_BATCH_MASK	0x30
#define LSM6DSO_FIFO_DEC_TS_BATCH__MASK	0xc0

/*
 * INT1 pin control register
 *
 * Each bit in this register enables a signal to be carried out on INT1
 * The output of the pin will be the OR combination of the signals selected here
 * and in MD1_CFG
 *
 * INT1_DRDY_XL   - Enables accelerometer data-ready interrupt on INT1 pin
 * INT1_DRDY_G    - Enables gyroscope data-ready interrupt on INT1 pin
 * INT1_BOOT      - Enables boot status on INT1 pin
 * INT1_FIFO_TH   - Enables FIFO threshold interrupt on INT1 pin
 * INT1_FIFO_OVR  - Enables FIFO overrun interrupt on INT1 pin
 * INT1_FIFO_FULL - Enables FIFO full flag interrupt on INT1 pin
 * INT1_CNT_BDR   - Enables COUNTER_BDA_IA on INT1 pin
 * DEN_DRDY_flag  - Send DEN_DRDY to INT1 pin
 */
#define LSM6DSO_INT1_CTRL		0x0d

/*
 * INT2 pin control register
 *
 * Each bit in this register enables a signal to be carried out on INT2
 * The output of the pin will be the OR combination of the signals selected here
 * and in MD2_CFG
 *
 * INT2_DRDY_XL   - Enables accelerometer data-ready interrupt on INT2 pin
 * INT2_DRDY_G    - Enables gyroscope data-ready interrupt on INT2 pin
 * INT2_DRDY_TEMP - Enables temperature sensor data-ready interrupt on INT2 pin
 * INT2_FIFO_TH   - Enables FIFO threshold interrupt on INT2 pin
 * INT2_FIFO_OVR  - Enables FIFO overrun interrupt on INT2 pin
 * INT2_FIFO_FULL - Enables FIFO full flag interrupt on INT2 pin
 * INT2_CNT_BDR   - Enables COUNTER_BDA_IA on INT2 pin
 */
#define LSM6DSO_INT2_CTRL		0x0e
#define LSM6DSO_INT_DRDY_XL_MASK	0x01
#define LSM6DSO_INT_DRDY_G_MASK		0x02
#define LSM6DSO_INT1_BOOT_MASK		0x04
#define LSM6DSO_INT2_DRDY_TEMP_MASK	0x04
#define LSM6DSO_INT_FIFO_TH_MASK	0x08
#define LSM6DSO_INT_FIFO_OVR_MASK	0x10
#define LSM6DSO_INT_FIFO_FULL_MASK	0x20
#define LSM6DSO_INT_CNT_BDR_MASK	0x40
#define LSM6DSO_DEN_DRDY_FLAG_MASK	0x80

/* Who Am I */
#define LSM6DSO_WHO_AM_I_REG		0x0f
#define LSM6DSO_WHO_AM_I		0x6c

/*
 * Accelerometer control register 1
 *
 * LPF2_XL_EN  - Accelerometer high-resolution selection
 * FS[1:0]_XL  - Accelerometer full-scale selection
 * ODR_XL[3:0] - Accelerometer ODR selection
 */
#define LSM6DSO_CTRL1_XL_ADDR		0x10
#define LSM6DSO_LPF2_XL_EN_MASK		0x02
#define LSM6DSO_FS_XL_MASK		0x0c
#define LSM6DSO_ODR_XL_MASK		0xf0

/*
 * Gyroscope control register 2
 *
 * FS_125     - Select gyro UI chain full-scale 125 dps
 * FS[1:0]_G  - Gyroscope full-scale selection
 * ODR_G[3:0] - Gyroscope ODR selection
 */
#define LSM6DSO_CTRL2_G_ADDR		0x11
#define LSM6DSO_FS_125_MASK		0x02
#define LSM6DSO_FS_G_MASK		0x0c
#define LSM6DSO_ODR_G_MASK		0xf0

/*
 * Control register 3
 *
 * SW_RESET  - Software reset
 * IF_INC    - Register address automatically incremented during a multiple byte
 *             access with a serial interface (I2C or SPI).
 * SIM       - SPI serial interface Mode selection
 * PP_OD     - Push-pull/open-drain selection on INT1 and INT2 pins
 * H_LACTIVE - Interrupt activation level
 * BDU       - Block Data Update
 * BOOT      - Reboots memory content
 */
#define LSM6DSO_CTRL3_C_ADDR		0x12
#define LSM6DSO_SW_RESET_MASK		0x01
#define LSM6DSO_IF_INC_MASK		0x04
#define LSM6DSO_SIM_MASK		0x08
#define LSM6DSO_PP_OD_MASK		0x10
#define LSM6DSO_H_L_ACTIVE_MASK		0x20
#define LSM6DSO_BDU_MASK		0x40
#define LSM6DSO_BOOT_MASK		0x80

/*
 * Control register 4
 *
 * LPF1_SEL_G    - Enables gyroscope digital LPF1 if auxiliary SPI is disabled
 * I2C_disable   - Disables I2C interface
 * DRDY_MASK     - Enables data available
 * INT2_on_INT1  - All interrupt signals available on INT1 pin enable
 * SLEEP_G       - Enables gyroscope Sleep mode
 */
#define LSM6DSO_CTRL4_C_ADDR		0x13
#define LSM6DSO_LPF1_SEL_G_MASK		0x02
#define LSM6DSO_I2C_DISABLE_MASK	0x04
#define LSM6DSO_DRDY_MASK_MASK		0x08
#define LSM6DSO_INT2_ON_INT1_MASK	0x20
#define LSM6DSO_SLEEP_G_MASK		0x40

/*
 * Control register 5
 *
 * ST[1:0]_XL    - Linear accelerometer sensor self-test enable
 * ST[1:0]_G     - Angular rate sensor self-test enable
 * ROUNDING[1:0] - Circular burst-mode (rounding) read from the output registers
 * XL_ULP_EN     - Accelerometer ultra-low-power mode enable
 */
#define LSM6DSO_CTRL5_C_ADDR		0x14
#define LSM6DSO_ST_XL_MASK		0x03
#define LSM6DSO_ST_G_MASK		0x0c
#define LSM6DSO_ROUNDING_MASK		0x06
#define LSM6DSO_XL_ULP_EN_MASK		0x80

#define LSM6DSO_XL_SELF_TEST_POS_SIGN	0x01
#define LSM6DSO_XL_SELF_TEST_NEG_SIGN	0x02
#define LSM6DSO_G_SELF_TEST_POS_SIGN	0x01
#define LSM6DSO_G_SELF_TEST_NEG_SIGN	0x03

/*
 * Control register 6
 *
 * FTYPE[2:0] - Gyroscope's low-pass filter (LPF1) bandwidth selection
 * USR_OFF_W  - Weight of XL user offset bits
 * XL_HM_MODE - High-performance operating mode disable for accelerometer
 * LVL2_EN    - DEN level-sensitive latched enable
 * LVL1_EN    - DEN data level-sensitive latched enable
 * TRIG_EN    - DEN data edge-sensitive latched enable
 */
#define LSM6DSO_CTRL6_C_ADDR		0x15
#define LSM6DSO_FTYPE_MASK		0x07
#define LSM6DSO_USR_OFF_W_MASK		0x08
#define LSM6DSO_XL_HM_MODE_MASK		0x10
#define LSM6DSO_LVL2_EN_MASK		0x20
#define LSM6DSO_LVL1_EN_MASK		0x40
#define LSM6DSO_TRIG_EN_MASK		0x80

/*
 * Control register 7
 *
 * USR_OFF_ON_OUT - Enables accelerometer user offset correction block
 * HPM_G[1:0]     - Gyroscope digital HP filter cutoff selection
 * HP_EN_G        - Enables gyroscope digital high-pass filter.
 *                  The filter is enabled only if the gyro is in HP mode
 * G_HM_MODE      - Disables high-performance operating mode for gyroscope
 */
#define LSM6DSO_CTRL7_G_ADDR		0x16
#define LSM6DSO_USR_OFF_ON_OUT_MASK	0x02
#define LSM6DSO_HPM_G_MASK		0x30
#define LSM6DSO_HP_EN_G_MASK		0x40
#define LSM6DSO_G_HM_MODE_MASK		0x80

/*
 * Control register 8
 *
 * LOW_PASS_ON_6D    - LPF2 on 6D function selection
 * XL_FS_MODE        - Accelerometer full-scale management between UI chain and
 *                     OIS chain
 * HP_SLOPE_XL_EN    - Accelerometer slope filter / high-pass filter selection
 * FASTSETTL_MODE_XL - Enables accelerometer LPF2 and HPF fast-settling mode
 * HP_REF_MODE_XL    - Enables accelerometer high-pass filter reference mode
 * HPCF_XL_[2:0]     - Accelerometer LPF2 and HP filter configuration and
 *                     cutoff setting
 */
#define LSM6DSO_CTRL8_XL_ADDR		0x17
#define LSM6DSO_LOW_PASS_ON_6D_MASK	0x01
#define LSM6DSO_HP_SLOPE_XL_EN_MASK	0x04
#define LSM6DSO_FASTSETTL_MODE_XL_MASK	0x08
#define LSM6DSO_HP_REF_MODE_XL_MASK	0x10
#define LSM6DSO_HPCF_XL_MASK		0xe0

/*
 * Control register 9
 *
 * I3C_disable - Disables MIPI I3C SM communication protocol
 * DEN_LH      - DEN active level configuration
 * DEN_XL_EN   - Extends DEN functionality to accelerometer sensor
 * DEN_XL_G    - DEN stamping sensor selection
 * DEN_Z       - DEN value stored in LSB of Z-axis
 * DEN_Y       - DEN value stored in LSB of Y-axis
 * DEN_X       - DEN value stored in LSB of X-axis
 */
#define LSM6DSO_CTRL9_XL_ADDR		0x18
#define LSM6DSO_I3C_DISABLE_MASK	0x02
#define LSM6DSO_DEN_Z_MASK		0x20
#define LSM6DSO_DEN_Y_MASK		0x40
#define LSM6DSO_DEN_X_MASK		0x80
#define LSM6DSO_DEN_ALL_MASK		(LSM6DSO_DEN_X_MASK | \
                                         LSM6DSO_DEN_Y_MASK | \
                                         LSM6DSO_DEN_Z_MASK)

/*
 * Control register 10
 *
 * TIMESTAMP_EN - Enables timestamp counte
 */
#define LSM6DSO_CTRL10_C_ADDR		0x19
#define LSM6DSO_TIMESTAMP_EN_MASK	0x20

/*
 * Source register for all interrupts
 *
 * FF_IA              - Free-fall event status
 * WU_IA              - Wake-up event status
 * SINGLE_TAP         - Single-tap event status
 * DOUBLE_TAP         - Double-tap event status
 * D6D_IA             - Interrupt active for change in position of portrait,
 *                      landscape, face-up, face-down
 * SLEEP_CHANGE_IA    - Detects change event in activity/inactivity status
 * TIMESTAMP_ENDCOUNT - Alerts timestamp overflow within 6.4 ms
 */
#define LSM6DSO_ALL_INT_SRC_ADDR	0x1a
#define LSM6DSO_FF_IA_MASK		0x01
#define LSM6DSO_WU_IA_MASK		0x02
#define LSM6DSO_SINGLE_TAP_MASK		0x04
#define LSM6DSO_DOUBLE_TAP_MASK		0x08
#define LSM6DSO_D6D_IA_MASK		0x10
#define LSM6DSO_SLEEP_CHANGE_IA_MASK	0x20
#define LSM6DSO_TIMESTAMP_ENDCOUNT_MASK	0x80

/*
 * Wake-up interrupt source register
 *
 * Z_WU        - Wakeup event detection status on Z-axis
 * Y_WU        - Wakeup event detection status on Y-axis
 * X_WU        - Wakeup event detection status on X-axis
 * WU_IA       - Wakeup event detection status
 * SLEEP_STATE - Sleep status bit
 */
#define LSM6DSO_WAKE_UP_SRC_ADDR	0x1b
#define LSM6DSO_Z_WU_MASK		0x01
#define LSM6DSO_Y_WU_MASK		0x02
#define LSM6DSO_X_WU_MASK		0x04
#define LSM6DSO_SLEEP_STATE_MASK	0x10

/*
 * Tap source register
 *
 * Z_TAP      - Tap event detection status on Z-axis
 * Y_TAP      - Tap event detection status on Y-axis
 * X_TAP      - Tap event detection status on X-axis
 * TAP_SIGN   - Sign of acceleration detected by tap event
 * DOUBLE_TAP - Double-tap event detection status
 * SINGLE_TAP - Single-tap event status
 * TAP_IA     - Tap event detection status
 */
#define LSM6DSO_TAP_SRC_ADDR		0x1c
#define LSM6DSO_Z_TAP_MASK		0x01
#define LSM6DSO_Y_TAP_MASK		0x02
#define LSM6DSO_X_TAP_MASK		0x04
#define LSM6DSO_TAP_SIGN_MASK		0x08
#define LSM6DSO_TAP_IA_MASK		0x40

/*
 * Portrait, landscape, face-up and face-down source register
 *
 * XL       - X-axis low event (under threshold)
 * XH       - X-axis high event (over threshold)
 * YL       - Y-axis low event (under threshold)
 * YH       - Y-axis high event (over threshold)
 * ZL       - Z-axis low event (under threshold)
 * ZH       - Z-axis high event (over threshold)
 * D6D_IA   - Interrupt active for change position portrait, landscape,
 *            face-up, face-down
 * DEN_DRDY - DEN data-ready signal
 */
#define LSM6DSO_D6D_SRC_ADDR		0x1d
#define LSM6DSO_XL_MASK			0x01
#define LSM6DSO_XH_MASK			0x02
#define LSM6DSO_YL_MASK			0x04
#define LSM6DSO_YH_MASK			0x08
#define LSM6DSO_ZL_MASK			0x10
#define LSM6DSO_ZH_MASK			0x20
#define LSM6DSO_DEN_DRDY_MASK		0x80

/*
 * Status register
 *
 * XLDA - Accelerometer new data available
 * GDA  - Gyroscope new data available
 * TDA  - Temperature new data available
 */
#define LSM6DSO_STATUS_REG		0x1e
#define LSM6DSO_STS_XLDA_UP_MASK	0x01
#define LSM6DSO_STS_GDA_UP_MASK		0x02
#define LSM6DSO_STS_TDA_UP_MASK		0x04

/*
 * Temperature data output register
 *
 * L and H registers together express a 16-bit word in two’s complement
 */
#define LSM6DSO_OUT_TEMP_L_ADDR		0x20

/*
 * Angular rate sensor pitch axis (X) angular rate output register
 *
 * The value is expressed as a 16-bit word in two’s complement
 */
#define LSM6DSO_OUTX_L_G_ADDR		0x22

/*
 * Linear acceleration sensor X-axis output register
 *
 * The value is expressed as a 16-bit word in two’s complement.
 */
#define LSM6DSO_OUTX_L_XL_ADDR		0x28

/*
 * FIFO status register 1
 *
 * DIFF_FIFO_[7:0] - Number of unread sensor data (TAG + 6 bytes) stored in FIFO
 */
#define LSM6DSO_FIFO_STS1_ADDR		0x3a

/*
 * FIFO status register 2
 *
 * DIFF_FIFO_[9:8]  - Number of unread sensor data (TAG + 6 bytes) stored in FIFO
 * FIFO_OVR_LATCHED - Latched FIFO overrun status
 * COUNTER_BDR_IA   - Counter BDR reaches the threshold
 * FIFO_FULL_IA     - Smart FIFO full status
 * FIFO_OVR_IA      - FIFO overrun status
 * FIFO_WTM_IA      - FIFO watermark status
 */
#define LSM6DSO_FIFO_STS2_ADDR		0x3b
#define LSM6DSO_FIFO_DIFF_MASK		0x03ff
#define LSM6DSO_FIFO_FULL_IA_MASK	0x2000
#define LSM6DSO_FIFO_OVR_IA_MASK	0x4000
#define LSM6DSO_FIFO_WTM_IA_MASK	0x8000

/*
 * Timestamp first data output register
 *
 * The value is expressed as a 32-bit word and the bit resolution is 25 μs.
 */
#define LSM6DSO_TIMESTAMP0_ADDR		0x40

/*
 * Activity/inactivity functions, configuration of filtering, and tap
 * recognition functions
 *
 * LIR                 - Latched Interrupt
 * TAP_Z_EN            - Enable Z direction in tap recognition
 * TAP_Y_EN            - Enable Y direction in tap recognition
 * TAP_X_EN            - Enable X direction in tap recognition
 * SLOPE_FDS           - HPF or SLOPE filter selection on wake-up and
 *                       Activity/Inactivity functions
 * SLEEP_STS_ON_INT    - [SLEEP_STATUS_ON_INT] Activity/inactivity interrupt
 *                       mode configuration
 * INT_CLR_ON_READ     - This bit allows immediately clearing the latched
 *                       interrupts of an event detection upon the read of the
 *                       corresponding status register. It must be set to 1
 *                       together with LIR
 */
#define LSM6DSO_TAP_CFG0_ADDR		0x56
#define LSM6DSO_LIR_MASK		0x01
#define LSM6DSO_TAP_Z_EN_MASK		0x02
#define LSM6DSO_TAP_Y_EN_MASK		0x04
#define LSM6DSO_TAP_X_EN_MASK		0x08
#define LSM6DSO_SLOPE_FDS_MASK		0x10
#define LSM6DSO_SLEEP_STS_ON_INT_MASK	0x20
#define LSM6DSO_INT_CLR_ON_READ_MASK	0x40

/*
 * Tap configuration register
 *
 * TAP_THS_X_[4:0]    - X-axis tap recognition threshold 1 LSB = FS_XL / (2^5)
 * TAP_PRIORITY_[2:0] - Selection of axis priority for TAP detection
 */
#define LSM6DSO_TAP_CFG1_ADDR		0x57
#define LSM6DSO_TAP_THS_X_MASK		0x1f
#define LSM6DSO_TAP_PRIORITY_MASK	0xe0

/*
 * Enables interrupt and inactivity functions, and tap recognition functions
 *
 * TAP_THS_Y_[4:0]   - Y-axis tap recognition threshold 1 LSB = FS_XL / (2^5)
 * INACT_EN[1:0]     - Enable activity/inactivity (sleep) function
 * INTERRUPTS_ENABLE - Enable basic interrupts (6D/4D, free-fall, wake-up, tap,
 *                     inactivity)
 */
#define LSM6DSO_TAP_CFG2_ADDR		0x58
#define LSM6DSO_TAP_THS_Y_MASK		0x1f
#define LSM6DSO_INACT_EN_MASK		0x60
#define LSM6DSO_INTERRUPTS_ENABLE_MASK	0x80

/*
 * Portrait/landscape position and tap function threshold register
 *
 * TAP_THS_Z_[4:0] - Z-axis tap recognition threshold 1 LSB = FS_XL / (2^5)
 * SIXD_THS[1:0]   - Threshold for 4D/6D function
 * D4D_EN          - 4D orientation detection enable. Z-axis position detection
 *                   is disabled
 */
#define LSM6DSO_TAP_THS_6D_ADDR		0x59
#define LSM6DSO_TAP_THS_Z_MASK		0x1f
#define LSM6DSO_SIXD_THS_MASK		0x60
#define LSM6DSO_D4D_EN_MASK		0x80

/*
 * Tap recognition function setting register
 *
 * SHOCK[1:0] - Maximum duration of overthreshold event
 * QUIET[1:0] - Expected quiet time after a tap detection
 * DUR[3:0]   - Duration of maximum time gap for double tap recognition
 */
#define LSM6DSO_INT_DUR2_ADDR		0x5a
#define LSM6DSO_SHOCK_MASK		0x03
#define LSM6DSO_QUIET_MASK		0x0c
#define LSM6DSO_DUR_MASK		0xf0

/*
 * Single/double-tap selection and wake-up configuration
 *
 * WK_THS[5:0]       - Threshold for wakeup
 * USR_OFF_ON_WU     - Drives the low-pass filtered data with user offset
 *                     correction (instead of high-pass filtered data) to the
 *                     wakeup function.
 * SINGLE_DOUBLE_TAP - Single/double-tap event enable
 */
#define LSM6DSO_WAKE_UP_THS_ADDR	0x5b
#define LSM6DSO_WK_THS_MASK		0x3f
#define LSM6DSO_USR_OFF_ON_WU_MASK	0x40
#define LSM6DSO_SINGLE_DOUBLE_TAP_MASK	0x80

/*
 * Free-fall, wakeup and sleep mode functions duration setting register
 *
 * SLEEP_DUR[3:0] - Duration to go in sleep mode
 * WAKE_THS_W     - Weight of LSB of wakeup threshold
 * WAKE_DUR[1:0]  - Wakeup duration event (in ODR)
 * FF_DUR5        - Free fall duration event (bit 5)
 */
#define LSM6DSO_WAKE_UP_DUR_ADDR	0x5c
#define LSM6DSO_SLEEP_DUR_MASK		0x0f
#define LSM6DSO_WAKE_THS_W_MASK		0x10
#define LSM6DSO_WAKE_DUR_MASK		0x60
#define LSM6DSO_FF_DUR5_MASK		0x80

/*
 * Functions routing on INT1 register
 *
 * FF_DUR[4:0] - Free-fall duration event
 * FF_THS[2:0] - Free fall threshold setting
 */
#define LSM6DSO_FREE_FALL_ADDR		0x5d
#define LSM6DSO_FF_THS_MASK		0x07
#define LSM6DSO_FF_DUR_MASK		0xf8

/*
 * Free-fall function duration setting register
 *
 * INT1_SHUB - Routing of sensor hub communication concluded event on INT1
 * INT1_EMB_FUNC - Routing of embedded functions event on INT1
 * INT1_6D - Routing of 6D event on INT1
 * INT1_DOUBLE_TAP - Routing of TAP event on INT1
 * INT1_FF - Routing of Free-Fall event on INT1
 * INT1_WU - Routing of Wake-up event on INT1
 * INT1_SINGLE_TAP - Routing of Single-Tap event on INT1
 * INT1_SLEEP_CHANGE - Routing of activity/inactivity recognition event on INT1
 */
#define LSM6DSO_MD1_CFG_ADDR		0x5e
#define LSM6DSO_INT1_SHUB_MASK		0x01
#define LSM6DSO_INT1_EMB_FUNC_MASK	0x02
#define LSM6DSO_INT1_6D_MASK		0x04
#define LSM6DSO_INT1_DOUBLE_TAP_MASK	0x08
#define LSM6DSO_INT1_FF_MASK		0x10
#define LSM6DSO_INT1_WU_MASK		0x20
#define LSM6DSO_INT1_SINGLE_TAP_MASK	0x40
#define LSM6DSO_INT1_SLEEP_CHANGE_MASK	0x80

/*
 * Free-fall function duration setting register
 *
 * INT2_TIMESTAMP - Enables routing on INT2 pin of the alert for timestamp
 *                  overflow within 6.4 ms
 * INT2_EMB_FUNC - Routing of embedded functions event on INT2
 * INT2_6D - Routing of 6D event on INT2
 * INT2_DOUBLE_TAP - Routing of TAP event on INT2
 * INT2_FF - Routing of Free-Fall event on INT2
 * INT2_WU - Routing of Wake-up event on INT2
 * INT2_SINGLE_TAP - Routing of Single-Tap event on INT2
 * INT2_SLEEP_CHANGE - Routing of activity/inactivity recognition event on INT2
 */
#define LSM6DSO_MD2_CFG_ADDR		0x5f
#define LSM6DSO_INT2_TIMESTAMP_MASK	0x01
#define LSM6DSO_INT2_EMB_FUNC_MASK	0x02
#define LSM6DSO_INT2_6D_MASK		0x04
#define LSM6DSO_INT2_DOUBLE_TAP_MASK	0x08
#define LSM6DSO_INT2_FF_MASK		0x10
#define LSM6DSO_INT2_WU_MASK		0x20
#define LSM6DSO_INT2_SINGLE_TAP_MASK	0x40
#define LSM6DSO_INT2_SLEEP_CHANGE_MASK	0x80

#define LSM6DSO_X_OFS_USR_ADDR		0x73
#define LSM6DSO_Y_OFS_USR_ADDR		0x74
#define LSM6DSO_Z_OFS_USR_ADDR		0x75

/* Out FIFO data register */
#define LSM6DSO_FIFO_DATA_ADDR_TAG	0x78
#define LSM6DSO_FIFO_TAG_MASK		0xf8

/* Registers in SensorHub page */
#define LSM6DSO_SENSORHUB1_REG		0x02
#define LSM6DSO_MASTER_CFG_ADDR		0x14
#define LSM6DSO_MASTER_ON		0x04
#define LSM6DSO_PASS_THROUGH_MODE	0x10
#define LSM6DSO_START_CONFIG		0x20
#define LSM6DSO_WRITE_ONCE		0x40
#define LSM6DSO_RST_MASTER_REGS		0x80

#define LSM6DSO_SLV0_ADD		0x15
#define LSM6DSO_SLV0_RD_BIT		0x01

#define LSM6DSO_SLV0_SUBADD_ADDR	0x16

#define LSM6DSO_SLV0_CONFIG_ADDR	0x17
#define LSM6DSO_SLV0_NUM_OPS_MASK	0x07
#define LSM6DSO_SLV0_BATCH_EXT_MASK	0x08
#define LSM6DSO_SLV0_ODR_MASK		0xc0

/* Define FIFO data pattern, tag and len */
#define LSM6DSO_SAMPLE_SIZE		6
#define LSM6DSO_TS_SAMPLE_SIZE		4
#define LSM6DSO_TAG_SIZE		1
#define LSM6DSO_FIFO_SAMPLE_SIZE	(LSM6DSO_SAMPLE_SIZE + LSM6DSO_TAG_SIZE)
#define LSM6DSO_MAX_FIFO_DEPTH		512

/* Sensors senstivity */
#define LSM6DSO_XL_SENSITIVITY_2G	61
#define LSM6DSO_XL_SENSITIVITY_4G	122
#define LSM6DSO_XL_SENSITIVITY_8G	244
#define LSM6DSO_XL_SENSITIVITY_16G	488
#define LSM6DSO_G_SENSITIVITY_250DPS	8750
#define LSM6DSO_G_SENSITIVITY_500DPS	17500
#define LSM6DSO_G_SENSITIVITY_1000DPS	35000
#define LSM6DSO_G_SENSITIVITY_2000DPS	70000

/* Self Test output converted in LSB */
#define LSM6DSO_XL_ST_MIN		819
#define LSM6DSO_XL_ST_MAX		27868
#define LSM6DSO_G_ST_MIN		2285
#define LSM6DSO_G_ST_MAX		9142

enum lsm6dso_tag_fifo {
	LSM6DSO_GYRO_TAG = 0x01,
	LSM6DSO_ACC_TAG,
	LSM6DSO_TEMPERATURE_TAG,
	LSM6DSO_TIMESTAMP_TAG,
	LSM6DSO_EXT0_TAG = 0x0e,
	LSM6DSO_EXT1_TAG,
	LSM6DSO_EXT2_TAG,
	LSM6DSO_EXT3_TAG,
	LSM6DSO_STEP_CPUNTER_TAG,
};

#define LSM6DSO_GET_OUT_REG(_type)	\
	(_type & SENSOR_TYPE_ACCELEROMETER) ? LSM6DSO_OUTX_L_XL_ADDR : \
	LSM6DSO_OUTX_L_G_ADDR;

/* Set Read operation bit in SPI communication */
#define LSM6DSO_SPI_READ_CMD_BIT(_reg)	(_reg |= 0x80)

/* Max time to wait for interrupt */
#define LSM6DSO_MAX_INT_WAIT (4 * OS_TICKS_PER_SEC)

/* Shift data with mask */
#define LSM6DSO_SHIFT_DATA_MASK(data, mask)   \
        ((data << __builtin_ctz(mask)) & mask)
#define LSM6DSO_DESHIFT_DATA_MASK(data, mask) \
        ((data & mask) >> __builtin_ctz(mask))

/* Some helpful macros */
#define LSM6DSO_ARRAY_SIZE(_x)		sizeof(_x)/sizeof(*_x)

int lsm6dso_writelen(struct sensor_itf *itf, uint8_t addr,
                     uint8_t *payload, uint8_t len);
int lsm6dso_readlen(struct sensor_itf *itf, uint8_t addr,
                    uint8_t *payload, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DSO_PRIV_H_ */
