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

#ifndef __LSM6DSL_PRIV_H__
#define __LSM6DSL_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Common defines for Acc and Gyro sensors */
#define LSM6DSL_EN_BIT                  0x01
#define LSM6DSL_DIS_BIT                 0x00

/*
 * Access to embedded sensor hub register bank
 *
 * FUNC_CFG_ACCESS - Enable access to the embedded functions registers
 * SHUB_REG_ACCESS - Enable access to the sensor hub registers
 */
#define LSM6DSL_FUNC_CFG_ACCESS_REG     0x01

#define LSM6DSL_FUNC_CFG_ACCESS_MASK    0x80
#define LSM6DSL_SHUB_REG_ACCESS_MASK    0x20

/*
 * FIFO control register 1
 *
 * FTH_[7:0] - FIFO threshold level bits
 */
#define LSM6DSL_FIFO_CTRL1_REG          0x06
#define LSM6DSL_FTH_0_7_MASK            0xFF

/*
 * FIFO control register 2
 *
 * FTH_[10:8] - FIFO threshold level bits
 * FIFO_TEMP_EN - Enable the temperature data storage in FIFO.
 * TIMER_PEDO_FIFO_DRDY - FIFO write mode.
 * TIMER_PEDO_FIFO_EN - Enable pedometer step counter and timestamp as 4th FIFO data set.
 */
#define LSM6DSL_FIFO_CTRL2_REG          0x07
#define LSM6DSL_FTH_8_10_MASK           0x07
#define LSM6DSL_FIFO_TEMP_EN_MASK       0x08
#define LSM6DSL_TIMER_PEDO_FIFO_DRDY_MASK 0x40
#define LSM6DSL_TIMER_PEDO_FIFO_EN_MASK 0x80

#define LSM6DSL_FIFO_FTH_MASK           0x07ff

/*
 * FIFO control register 3
 *
 * DEC_FIFO_XL[2:0] - Accelerometer FIFO (second data set) decimation setting.
 * DEC_FIFO_GYRO[2:0] - Gyro FIFO (first data set) decimation setting.
 */
#define LSM6DSL_FIFO_CTRL3_REG          0x08
#define LSM6DSL_DEC_FIFO_XL_MASK        0x07
#define LSM6DSL_DEC_FIFO_GYRO_MASK      0x38

/*
 * FIFO control register 4
 *
 * DEC_DS3_FIFO[2:0] - Third FIFO data set decimation setting
 * DEC_DS3_FIFO[2:0] - Fourth FIFO data set decimation setting
 * ONLY_HIGH_DATA    - 8-bit data storage in FIFO
 * STOP_ON_FTH       - Enable FIFO threshold level use
 */
#define LSM6DSL_FIFO_CTRL4_REG          0x09
#define LSM6DSL_DEC_DES3_FIFO_MASK      0x07
#define LSM6DSL_DEC_DES4_FIFO_MASK      0x38
#define LSM6DSL_ONLY_HIGH_DATA_MASK     0x40
#define LSM6DSL_STOP_ON_FTH_MASK        0x80

/*
 * FIFO control register 5
 *
 * FIFO_MODE_[2:0] - FIFO mode selection bits
 * ODR_FIFO_[3:0]  - FIFO ODR selection
 */
#define LSM6DSL_FIFO_CTRL5_REG          0x0A
#define LSM6DSL_FIFO_MODE_MASK          0x07
#define LSM6DSL_ODR_FIFO_MASK           0x78

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
#define LSM6DSL_INT1_CTRL               0x0d

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
#define LSM6DSL_INT2_CTRL               0x0e
#define LSM6DSL_INT_DRDY_XL_MASK        0x01
#define LSM6DSL_INT_DRDY_G_MASK         0x02
#define LSM6DSL_INT1_BOOT_MASK          0x04
#define LSM6DSL_INT2_DRDY_TEMP_MASK     0x04
#define LSM6DSL_INT_FIFO_TH_MASK        0x08
#define LSM6DSL_INT_FIFO_OVR_MASK       0x10
#define LSM6DSL_INT_FIFO_FULL_MASK      0x20
#define LSM6DSL_INT_CNT_BDR_MASK        0x40
#define LSM6DSL_DEN_DRDY_FLAG_MASK      0x80

/* Who Am I */
#define LSM6DSL_WHO_AM_I_REG            0x0f
#define LSM6DSL_WHO_AM_I                0x6a

/*
 * Accelerometer control register 1
 *
 * LPF2_XL_EN  - Accelerometer high-resolution selection
 * FS[1:0]_XL  - Accelerometer full-scale selection
 * ODR_XL[3:0] - Accelerometer ODR selection
 */
#define LSM6DSL_CTRL1_XL_REG            0x10
#define LSM6DSL_BW0_XL_MASK             0x01
#define LSM6DSL_LPF1_BW_SEL_MASK        0x02
#define LSM6DSL_FS_XL_MASK              0x0c
#define LSM6DSL_ODR_XL_MASK             0xf0

/*
 * Gyroscope control register 2
 *
 * FS_125     - Select gyro UI chain full-scale 125 dps
 * FS[1:0]_G  - Gyroscope full-scale selection
 * ODR_G[3:0] - Gyroscope ODR selection
 */
#define LSM6DSL_CTRL2_G_REG             0x11
#define LSM6DSL_FS_125_MASK             0x02
#define LSM6DSL_FS_G_MASK               0x0e
#define LSM6DSL_ODR_G_MASK              0xf0

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
#define LSM6DSL_CTRL3_C_REG             0x12
#define LSM6DSL_SW_RESET_MASK           0x01
#define LSM6DSL_BLE_MASK                0x02
#define LSM6DSL_IF_INC_MASK             0x04
#define LSM6DSL_SIM_MASK                0x08
#define LSM6DSL_PP_OD_MASK              0x10
#define LSM6DSL_H_LACTIVE_MASK          0x20
#define LSM6DSL_BDU_MASK                0x40
#define LSM6DSL_BOOT_MASK               0x80

/*
 * Control register 4
 *
 * LPF1_SEL_G    - Enables gyroscope digital LPF1.
 * I2C_disable   - Disables I2C interface.
 * DRDY_MASK     - Configuration 1 data available enable bit.
 * DEN_DRDY_INT1 - DEN DRDY signal on INT1 pad.
 * INT2_on_INT1  - All interrupt signals available on INT1 pin enable.
 * SLEEP         - Enables gyroscope Sleep mode.
 * DEN_XL_EN     - Extend DEN functionality to accelerometer sensor.
 */
#define LSM6DSL_CTRL4_C_REG             0x13
#define LSM6DSL_LPF1_SEL_G_MASK         0x02
#define LSM6DSL_I2C_DISABLE_MASK        0x04
#define LSM6DSL_DRDY_MASK_MASK          0x08
#define LSM6DSL_DEN_DRDY_INT1_MASK      0x10
#define LSM6DSL_INT2_ON_INT1_MASK       0x20
#define LSM6DSL_SLEEP_MASK              0x40
#define LSM6DSL_DEN_XL_EN_MASK          0x80

/*
 * Control register 5
 *
 * ST[1:0]_XL    - Linear accelerometer sensor self-test enable
 * ST[1:0]_G     - Angular rate sensor self-test enable
 * DEN_LH        - DEN active level configuration
 * ROUNDING[2:0] - Circular burst-mode (rounding) read from the output registers
 */
#define LSM6DSL_CTRL5_C_REG             0x14
#define LSM6DSL_ST_XL_MASK              0x03
#define LSM6DSL_ST_G_MASK               0x0c
#define LSM6DSL_DEN_LH_MASK             0x10
#define LSM6DSL_ROUNDING_MASK           0xE0

#define LSM6DSL_XL_SELF_TEST_POS_SIGN   0x01
#define LSM6DSL_XL_SELF_TEST_NEG_SIGN   0x02
#define LSM6DSL_G_SELF_TEST_POS_SIGN    0x01
#define LSM6DSL_G_SELF_TEST_NEG_SIGN    0x03

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
#define LSM6DSL_CTRL6_C_REG             0x15
#define LSM6DSL_FTYPE_MASK              0x03
#define LSM6DSL_USR_OFF_W_MASK          0x08
#define LSM6DSL_XL_HM_MODE_MASK         0x10
#define LSM6DSL_LVL2_EN_MASK            0x20
#define LSM6DSL_LVL_EN_MASK             0x40
#define LSM6DSL_TRIG_EN_MASK            0x80

/*
 * Control register 7
 *
 * USR_OFF_ON_OUT - Enables accelerometer user offset correction block
 * HPM_G[1:0]     - Gyroscope digital HP filter cutoff selection
 * HP_EN_G        - Enables gyroscope digital high-pass filter.
 *                  The filter is enabled only if the gyro is in HP mode
 * G_HM_MODE      - Disables high-performance operating mode for gyroscope
 * ROUNDING_STATUS - Source register rounding function on WAKE_UP_SRC
 */
#define LSM6DSL_CTRL7_G_REG             0x16
#define LSM6DSL_ROUNDING_STATUS_MASK    0x04
#define LSM6DSL_HPM_G_MASK              0x30
#define LSM6DSL_HP_EN_G_MASK            0x40
#define LSM6DSL_G_HM_MODE_MASK          0x80

/*
 * Control register 8
 *
 * LOW_PASS_ON_6D    - LPF2 on 6D function selection
 * XL_FS_MODE        - Accelerometer full-scale management between UI chain and
 *                     OIS chain
 * HP_SLOPE_XL_EN    - Accelerometer slope filter / high-pass filter selection
 * INPUT_COMPOSITE   - Composite filter input selection
 * HP_REF_MODE_XL    - Enables accelerometer high-pass filter reference mode
 * HPCF_XL_[1:0]     - Accelerometer LPF2 and HP filter configuration and
 *                     cutoff setting
 * LPF2_XL_EN        - Accelerometer low-pass filter LPF2 selection
 */
#define LSM6DSL_CTRL8_XL_REG            0x17
#define LSM6DSL_LOW_PASS_ON_6D_MASK     0x01
#define LSM6DSL_HP_SLOPE_XL_EN_MASK     0x04
#define LSM6DSL_INPUT_COMPOSITE_MASK    0x08
#define LSM6DSL_HP_REF_MODE_XL_MASK     0x10
#define LSM6DSL_HPCF_XL_MASK            0x60
#define LSM6DSL_LPF2_XL_EN_MASK         0x80

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
#define LSM6DSL_CTRL9_XL_REG            0x18
#define LSM6DSL_SOFT_EN_MASK            0x04
#define LSM6DSL_DEN_XL_G_MASK           0x10
#define LSM6DSL_DEN_Z_MASK              0x20
#define LSM6DSL_DEN_Y_MASK              0x40
#define LSM6DSL_DEN_X_MASK              0x80
#define LSM6DSL_DEN_ALL_MASK            (LSM6DSL_DEN_X_MASK | \
                                         LSM6DSL_DEN_Y_MASK | \
                                         LSM6DSL_DEN_Z_MASK)

/*
 * Control register 10
 *
 * SIGN_MOTION_EN - Enable significant motion detection function
 * PEDO_RST       - Reset pedometer step counter
 * FUNC_EN        - Enable embedded functionalities (pedometer, tilt, significant motion
 *                  detection, sensor hub and ironing)
 * TILT_EN        - Enable tilt calculation
 * PEDO_EN        - Enable pedometer algorithm
 * TIMER_EN       - Enable timestamp count
 * WRIST_TILT_EN  - Enable wrist tilt algorithm
 */
#define LSM6DSL_CTRL10_C_REG            0x19
#define LSM6DSL_SIGN_MOTION_EN_MASK     0x01
#define LSM6DSL_PEDO_RST_MASK           0x02
#define LSM6DSL_FUNC_EN_MASK            0x04
#define LSM6DSL_TILT_EN_MASK            0x08
#define LSM6DSL_PEDO_EN_MASK            0x10
#define LSM6DSL_TIMER_EN_MASK           0x20
#define LSM6DSL_WRIST_TILT_EN_MASK      0x80


/*
 * Wake-up interrupt source register
 *
 * Z_WU        - Wakeup event detection status on Z-axis
 * Y_WU        - Wakeup event detection status on Y-axis
 * X_WU        - Wakeup event detection status on X-axis
 * WU_IA       - Wakeup event detection status
 * SLEEP_STATE_IA - Sleep status bit
 * FF_IA       - Free-fall event status
 */
#define LSM6DSL_WAKE_UP_SRC_REG         0x1b
#define LSM6DSL_Z_WU_MASK               0x01
#define LSM6DSL_Y_WU_MASK               0x02
#define LSM6DSL_X_WU_MASK               0x04
#define LSM6DSL_WU_IA_MASK              0x08
#define LSM6DSL_SLEEP_STATE_IA_MASK     0x10
#define LSM6DSL_FF_IA_MASK              0x20

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
#define LSM6DSL_TAP_SRC_REG             0x1c
#define LSM6DSL_Z_TAP_MASK              0x01
#define LSM6DSL_Y_TAP_MASK              0x02
#define LSM6DSL_X_TAP_MASK              0x04
#define LSM6DSL_TAP_SIGN_MASK           0x08
#define LSM6DSL_DOUBLE_TAP_MASK         0x10
#define LSM6DSL_SINGLE_TAP_MASK         0x20
#define LSM6DSL_TAP_IA_MASK             0x40

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
#define LSM6DSL_D6D_SRC_REG             0x1d
#define LSM6DSL_XL_MASK                 0x01
#define LSM6DSL_XH_MASK                 0x02
#define LSM6DSL_YL_MASK                 0x04
#define LSM6DSL_YH_MASK                 0x08
#define LSM6DSL_ZL_MASK                 0x10
#define LSM6DSL_ZH_MASK                 0x20
#define LSM6DSL_D6D_IA_MASK             0x40
#define LSM6DSL_DEN_DRDY_MASK           0x80

/*
 * Status register
 *
 * XLDA - Accelerometer new data available
 * GDA  - Gyroscope new data available
 * TDA  - Temperature new data available
 */
#define LSM6DSL_STATUS_REG              0x1e
#define LSM6DSL_STS_XLDA_UP_MASK        0x01
#define LSM6DSL_STS_GDA_UP_MASK         0x02
#define LSM6DSL_STS_TDA_UP_MASK         0x04

/*
 * Temperature data output register
 *
 * L and H registers together express a 16-bit word in two’s complement
 */
#define LSM6DSL_OUT_TEMP_L_REG          0x20
#define LSM6DSL_OUT_TEMP_H_REG          0x21

/*
 * Angular rate sensor pitch axis (X) angular rate output register
 *
 * The value is expressed as a 16-bit word in two’s complement
 */
#define LSM6DSL_OUTX_L_G_REG            0x22
#define LSM6DSL_OUTX_H_G_REG            0x23

/*
 * Linear acceleration sensor X-axis output register
 *
 * The value is expressed as a 16-bit word in two’s complement.
 */
#define LSM6DSL_OUTX_L_XL_REG           0x28
#define LSM6DSL_OUTX_H_XL_REG           0x29

/*
 * FIFO status register 1
 *
 * DIFF_FIFO_[7:0] - Number of unread words (16-bit axes) stored in FIFO
 */
#define LSM6DSL_FIFO_STATUS1_REG        0x3a
//#define LSM6DSL_FIFO_DIFF_MASK          0x03ff

/*
 * FIFO status register 2
 *
 * DIFF_FIFO_[9:8]  - Number of unread words (16-bit axes) stored in FIFO
 * FIFO_OVR_LATCHED - Latched FIFO overrun status
 * COUNTER_BDR_IA   - Counter BDR reaches the threshold
 * FIFO_FULL_IA     - Smart FIFO full status
 * FIFO_OVR_IA      - FIFO overrun status
 * FIFO_WTM_IA      - FIFO watermark status
 */
#define LSM6DSL_FIFO_STATUS2_REG        0x3b
#define LSM6DSL_DIFF_FIFO_MASK          0x07
#define LSM6DSL_FIFO_EMPTY_MASK         0x10
#define LSM6DSL_FIFO_FULL_SMART_MASK    0x20
#define LSM6DSL_OVER_RUN_MASK           0x40
#define LSM6DSL_WaterM_MASK             0x80

/*
 * FIFO status register 3
 *
 * FIFO_PATTERN_[7:0]  - Number of unread words (16-bit axes) stored in FIFO
 */
#define LSM6DSL_FIFO_STATUS3_REG        0x3c

/*
 * FIFO status register 3
 *
 * FIFO_PATTERN_[9:8]  - Number of unread words (16-bit axes) stored in FIFO
 */
#define LSM6DSL_FIFO_STATUS4_REG        0x3d

/*
 * FIFO data out registers
 */
#define LSM6DSL_FIFO_DATA_OUT_L_REG     0x3e
#define LSM6DSL_FIFO_DATA_OUT_H_REG     0x3f

/*
 * Timestamp first data output register
 *
 * The value is expressed as a 32-bit word and the bit resolution is 25 μs or
 * 6.4ms depending on (WAKE_UP_DUR:TIMER_HR).
 */
#define LSM6DSL_TIMESTAMP0_REG          0x40
#define LSM6DSL_TIMESTAMP1_REG          0x41
#define LSM6DSL_TIMESTAMP2_REG          0x42

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
 * INACT_EN            - Enable inactivity function
 *                        00: disabled
 *                        01: sets accelerometer ODR to 12.5 Hz (low-power mode),
 *                            gyro does not change
 *                        10: sets accelerometer ODR to 12.5 Hz (low-power mode),
 *                            gyro to sleep mode
 *                        11: sets accelerometer ODR to 12.5 Hz (low-power mode),
 *                            gyro to power-down mode
 * INTERRUPTS_ENABLE   - Enable basic interrupts (6D/4D, free-fall, wake-up,
 *                       tap, inactivity).
 * SLEEP_STS_ON_INT    - [SLEEP_STATUS_ON_INT] Activity/inactivity interrupt
 *                       mode configuration
 * INT_CLR_ON_READ     - This bit allows immediately clearing the latched
 *                       interrupts of an event detection upon the read of the
 *                       corresponding status register. It must be set to 1
 *                       together with LIR
 */
#define LSM6DSL_TAP_CFG_REG             0x58
#define LSM6DSL_LIR_MASK                0x01
#define LSM6DSL_TAP_Z_EN_MASK           0x02
#define LSM6DSL_TAP_Y_EN_MASK           0x04
#define LSM6DSL_TAP_X_EN_MASK           0x08
#define LSM6DSL_SLOPE_FDS_MASK          0x10
#define LSM6DSL_INACT_EN_MASK           0x60
#define LSM6DSL_INTERRUPTS_ENABLE_MASK  0x80

#define LSM6DSL_TAP_XYZ_EN_MASK         0x0E

#define LSM6DSL_SLEEP_STS_ON_INT_MASK   0x20
#define LSM6DSL_INT_CLR_ON_READ_MASK    0x40

/*
 * Portrait/landscape position and tap function threshold register
 *
 * TAP_THS_Z_[4:0] - Z-axis tap recognition threshold 1 LSB = FS_XL / (2^5)
 * SIXD_THS[1:0]   - Threshold for 4D/6D function
 * D4D_EN          - 4D orientation detection enable. Z-axis position detection
 *                   is disabled
 */
#define LSM6DSL_TAP_THS_6D_REG          0x59
#define LSM6DSL_TAP_THS_MASK            0x1f
#define LSM6DSL_SIXD_THS_MASK           0x60
#define LSM6DSL_D4D_EN_MASK             0x80

/*
 * Tap recognition function setting register
 *
 * SHOCK[1:0] - Maximum duration of overthreshold event
 * QUIET[1:0] - Expected quiet time after a tap detection
 * DUR[3:0]   - Duration of maximum time gap for double tap recognition
 */
#define LSM6DSL_INT_DUR2_REG            0x5a
#define LSM6DSL_SHOCK_MASK              0x03
#define LSM6DSL_QUIET_MASK              0x0c
#define LSM6DSL_DUR_MASK                0xf0

/*
 * Single/double-tap selection and wake-up configuration
 *
 * WK_THS[5:0]       - Threshold for wakeup
 * SINGLE_DOUBLE_TAP - Single/double-tap event enable
 */
#define LSM6DSL_WAKE_UP_THS_REG         0x5b
#define LSM6DSL_WK_THS_MASK             0x3f
#define LSM6DSL_SINGLE_DOUBLE_TAP_MASK  0x80

/*
 * Free-fall, wakeup and sleep mode functions duration setting register
 *
 * SLEEP_DUR[3:0] - Duration to go in sleep mode
 * TIMER_HR       - Timestamp register resolution setting
 *                  (0: 1LSB = 6.4 ms; 1: 1LSB = 25 μs)
 * WAKE_DUR[1:0]  - Wakeup duration event (in ODR)
 * FF_DUR5        - Free fall duration event (bit 5)
 */
#define LSM6DSL_WAKE_UP_DUR_REG         0x5c
#define LSM6DSL_SLEEP_DUR_MASK          0x0f
#define LSM6DSL_TIMER_HR_MASK           0x10
#define LSM6DSL_WAKE_DUR_MASK           0x60
#define LSM6DSL_FF_DUR5_MASK            0x80

/*
 * Free-fall function duration setting register
 *
 * FF_DUR[4:0] - Free-fall duration event
 * FF_THS[2:0] - Free fall threshold setting
 */
#define LSM6DSL_FREE_FALL_REG           0x5d
#define LSM6DSL_FF_THS_MASK             0x07
#define LSM6DSL_FF_DUR_MASK             0xf8

/*
 * Functions routing on INT1 register
 *
 * INT1_TIMER - Routing of end counter event of timer on INT1
 * INT1_TILT - Routing of tilt event on INT1
 * INT1_6D - Routing of 6D event on INT1
 * INT1_DOUBLE_TAP - Routing of TAP event on INT1
 * INT1_FF - Routing of Free-Fall event on INT1
 * INT1_WU - Routing of Wake-up event on INT1
 * INT1_SINGLE_TAP - Routing of Single-Tap event on INT1
 * INT1_INACT_STATE - Routing on INT1 of inactivity mode
 */
#define LSM6DSL_MD1_CFG_REG             0x5e
#define LSM6DSL_INT1_TIMER_MASK         0x01
#define LSM6DSL_INT1_TILT_MASK          0x02
#define LSM6DSL_INT1_6D_MASK            0x04
#define LSM6DSL_INT1_DOUBLE_TAP_MASK    0x08
#define LSM6DSL_INT1_FF_MASK            0x10
#define LSM6DSL_INT1_WU_MASK            0x20
#define LSM6DSL_INT1_SINGLE_TAP_MASK    0x40
#define LSM6DSL_INT1_INACT_STATE_MASK   0x80

/*
 * Functions routing on INT2 register
 *
 * INT2_IRON - Routing of soft-iron/hard-iron algorithm end event on INT2
 * INT2_TILT - Routing of tilt event on INT2
 * INT2_6D - Routing of 6D event on INT2
 * INT2_DOUBLE_TAP - Routing of TAP event on INT2
 * INT2_FF - Routing of Free-Fall event on INT2
 * INT2_WU - Routing of Wake-up event on INT2
 * INT2_SINGLE_TAP - Routing of Single-Tap event on INT2
 * INT2_INACT_STATE - Routing on INT2 of inactivity mode
 */
#define LSM6DSL_MD2_CFG_REG             0x5f
#define LSM6DSL_INT2_IRON_MASK          0x01
#define LSM6DSL_INT2_TILT_MASK          0x02
#define LSM6DSL_INT2_6D_MASK            0x04
#define LSM6DSL_INT2_DOUBLE_TAP_MASK    0x08
#define LSM6DSL_INT2_FF_MASK            0x10
#define LSM6DSL_INT2_WU_MASK            0x20
#define LSM6DSL_INT2_SINGLE_TAP_MASK    0x40
#define LSM6DSL_INT2_INACT_STATE_MASK   0x80

#define LSM6DSL_X_OFS_USR_REG           0x73
#define LSM6DSL_Y_OFS_USR_REG           0x74
#define LSM6DSL_Z_OFS_USR_REG           0x75

/* SensorHub registers */
#define LSM6DSL_SENSORHUB1_REG          0x2e
#define LSM6DSL_SENSORHUB2_REG          0x2f
#define LSM6DSL_SENSORHUB3_REG          0x30
#define LSM6DSL_SENSORHUB4_REG          0x31
#define LSM6DSL_SENSORHUB5_REG          0x32
#define LSM6DSL_SENSORHUB6_REG          0x33
#define LSM6DSL_SENSORHUB7_REG          0x34
#define LSM6DSL_SENSORHUB8_REG          0x35
#define LSM6DSL_SENSORHUB9_REG          0x36
#define LSM6DSL_SENSORHUB10_REG         0x37
#define LSM6DSL_SENSORHUB11_REG         0x38
#define LSM6DSL_SENSORHUB12_REG         0x39
#define LSM6DSL_SENSORHUB13_REG         0x4d
#define LSM6DSL_SENSORHUB14_REG         0x4e
#define LSM6DSL_SENSORHUB15_REG         0x4f
#define LSM6DSL_SENSORHUB16_REG         0x50
#define LSM6DSL_SENSORHUB17_REG         0x51
#define LSM6DSL_SENSORHUB18_REG         0x52

/*
 * FUNC_SRC1 register
 */
#define LSM6DSL_FUNC_SRC1_REG             0x53
#define LSM6DSL_SENSORHUB_END_OP_MASK     0x01
#define LSM6DSL_SI_END_OP_MASK            0x02
#define LSM6DSL_HI_FAI_MASKL              0x04
#define LSM6DSL_STEP_OVERFLOW_MASK        0x08
#define LSM6DSL_STEP_DETECTED_MASK        0x10
#define LSM6DSL_TILT_IA_MASK              0x20
#define LSM6DSL_SIGN_MOTION_IA_MASK       0x40
#define LSM6DSL_STEP_COUNT_DELTA_IA_MASK  0x80

/*
 * FUNC_SRC2 register
 */
#define LSM6DSL_FUNC_SRC2_REG             0x54
#define LSM6DSL_WRIST_TILT_IA_MASK        0x01

/* Bank A registers */
#define LSM6DSL_SLV0_ADD_REG            0x02
#define LSM6DSL_SLV0_SUBADD_REG         0x03
#define LSM6DSL_SLV0_CONFIG_REG         0x04
#define LSM6DSL_SLV1_ADD_REG            0x05
#define LSM6DSL_SLV1_SUBADD_REG         0x06
#define LSM6DSL_SLV1_CONFIG_REG         0x07
#define LSM6DSL_SLV2_ADD_REG            0x08
#define LSM6DSL_SLV2_SUBADD_REG         0x09
#define LSM6DSL_SLV2_CONFIG_REG         0x0a
#define LSM6DSL_SLV3_ADD_REG            0x0b
#define LSM6DSL_SLV3_SUBADD_REG         0x0c
#define LSM6DSL_SLV3_CONFIG_REG         0x0d
#define LSM6DSL_DATAWRITE_SRC_MODE_SUB_SLV0_REG 0x0e
#define LSM6DSL_CONFIG_PEDO_THS_MIN_REG 0x0f
#define LSM6DSL_SM_THS_REG              0x13
#define LSM6DSL_PEDO_DEB_REG_REG        0x14
#define LSM6DSL_STEP_COUNT_DELTA_REG    0x15
#define LSM6DSL_MAG_SI_XX_REG           0x24
#define LSM6DSL_MAG_SI_XY_REG           0x25
#define LSM6DSL_MAG_SI_XZ_REG           0x26
#define LSM6DSL_MAG_SI_YX_REG           0x27
#define LSM6DSL_MAG_SI_YY_REG           0x28
#define LSM6DSL_MAG_SI_YZ_REG           0x29
#define LSM6DSL_MAG_SI_ZX_REG           0x2a
#define LSM6DSL_MAG_SI_ZY_REG           0x2b
#define LSM6DSL_MAG_SI_ZZ_REG           0x2c
#define LSM6DSL_MAG_OFFX_L_REG          0x2d
#define LSM6DSL_MAG_OFFX_H_REG          0x2e
#define LSM6DSL_MAG_OFFY_L_REG          0x2f
#define LSM6DSL_MAG_OFFY_H_REG          0x30
#define LSM6DSL_MAG_OFFZ_L_REG          0x31
#define LSM6DSL_MAG_OFFZ_H_REG          0x32

/* Bank 2 registers */

#define LSM6DSL_A_WRIST_TILT_LAT_REG    0x50
#define LSM6DSL_A_WRIST_TILT_THS_REG    0x54
#define LSM6DSL_A_WRIST_TILT_MASK_REG   0x59

#define LSM6DSL_A_WRIST_TILT_IA_REG     0x55
#define LSM6DSL_A_WRIST_TILT_XPOS_MASK  0x80
#define LSM6DSL_A_WRIST_TILT_XNEG_MASK  0x40
#define LSM6DSL_A_WRIST_TILT_YPOS_MASK  0x20
#define LSM6DSL_A_WRIST_TILT_YNEG_MASK  0x10
#define LSM6DSL_A_WRIST_TILT_ZPOS_MASK  0x08
#define LSM6DSL_A_WRIST_TILT_ZNEG_MASK  0x04

#define LSM6DSL_MAX_FIFO_DEPTH          2048

/* Self Test output converted in LSB */
#define LSM6DSL_XL_ST_MIN               819
#define LSM6DSL_XL_ST_MAX               27868
#define LSM6DSL_G_ST_MIN                2285
#define LSM6DSL_G_ST_MAX                9142

#define LSM6DSL_GET_OUT_REG(_type) \
    (_type & SENSOR_TYPE_ACCELEROMETER) ? LSM6DSL_OUTX_L_XL_REG : \
    LSM6DSL_OUTX_L_G_REG;

/* Set Read operation bit in SPI communication */
#define LSM6DSL_SPI_READ_CMD_BIT(_reg)  (_reg |= 0x80)

/* Max time to wait for interrupt */
#define LSM6DSL_MAX_INT_WAIT (4 * OS_TICKS_PER_SEC)

/* Shift data with mask */
#define LSM6DSL_SHIFT_DATA_MASK(data, mask)   \
        ((data << __builtin_ctz(mask)) & mask)
#define LSM6DSL_DESHIFT_DATA_MASK(data, mask) \
        ((data & mask) >> __builtin_ctz(mask))

struct lsm6dsl;

struct lsm6dsl_cfg_regs1 {
    union {
        uint8_t regs[29];
        struct {
            uint8_t func_cfg_access;
            uint8_t reserved1[2];
            uint8_t sensor_sync_time_frame;
            uint8_t sensor_sync_res_ratio;
            uint8_t fifo_ctrl1;
            uint8_t fifo_ctrl2;
            uint8_t fifo_ctrl3;
            uint8_t fifo_ctrl4;
            uint8_t fifo_ctrl5;
            uint8_t drdy_pulse_cfg_g;
            uint8_t reserved2[1];
            uint8_t int1_ctrl;
            uint8_t int2_ctrl;
            uint8_t who_am_i;
            uint8_t ctrl1_xl;
            uint8_t ctrl2_g;
            uint8_t ctrl3_c;
            uint8_t ctrl4_c;
            uint8_t ctrl5_c;
            uint8_t ctrl6_c;
            uint8_t ctrl7_g;
            uint8_t ctrl8_xl;
            uint8_t ctrl9_xl;
            uint8_t ctrl10_c;
            uint8_t master_config;
            uint8_t wauke_up_src;
            uint8_t tap_src;
            uint8_t d6d_src;
        };
    };
};

struct lsm6dsl_cfg_regs2 {
    union {
        uint8_t regs[8];
        struct {
            uint8_t tap_cfg;
            uint8_t tap_ths_6d;
            uint8_t int_dur2;
            uint8_t wake_up_ths;
            uint8_t wake_up_dur;
            uint8_t free_fall;
            uint8_t md1_cfg;
            uint8_t md2_cfg;
        };
    };
};

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DSL_PRIV_H_ */
