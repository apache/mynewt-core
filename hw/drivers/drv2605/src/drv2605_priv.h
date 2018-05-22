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

#ifndef __DRV2605_PRIV_H__
#define __DRV2605_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>


#define DRV2605_STATUS_ADDR                                     0x00

#define DRV2605_STATUS_DEVICE_ID_POS                            5
#define DRV2605_STATUS_DEVICE_ID_MASK                           (0x7 << DRV2605_STATUS_DEVICE_ID_POS)

#define DRV2605_STATUS_DEVICE_ID_2605                           (3)
#define DRV2605_STATUS_DEVICE_ID_2604                           (4)
#define DRV2605_STATUS_DEVICE_ID_2604L                          (6)
#define DRV2605_STATUS_DEVICE_ID_2605L                          (7)

#define DRV2605_LOW_VOLTAGE_ID                                  0x07


#define DRV2605_STATUS_DIAG_RESULT_POS                          3
#define DRV2605_STATUS_DIAG_RESULT_SUCCESS                      ((0) << DRV2605_STATUS_DIAG_RESULT_POS)
#define DRV2605_STATUS_DIAG_RESULT_FAIL                         ((1) << DRV2605_STATUS_DIAG_RESULT_POS)

// FB_STS is reserved on the drv2605l and therefore to be ingored
#define DRV2605_STATUS_FB_STS_POS                               2
#define DRV2605_STATUS_FB_STS                                   ((1) << DRV2605_STATUS_FB_STS_POS)

#define DRV2605_STATUS_OVER_TEMP_POS                            1
#define DRV2605_STATUS_OVER_TEMP                                ((1) << DRV2605_STATUS_OVER_TEMP_POS)

#define DRV2605_STATUS_OC_DETECT_POS                            0
#define DRV2605_STATUS_OC_DETECT                                ((1) << DRV2605_STATUS_OC_DETECT_POS)


#define DRV2605_MODE_ADDR                                       0x01

#define DRV2605_MODE_DEV_RESET_POS                              7
#define DRV2605_MODE_RESET                                      ((1) << DRV2605_MODE_DEV_RESET_POS)

#define DRV2605_MODE_STANDBY_POS                                6
#define DRV2605_MODE_STANDBY_MASK                               ((1) << DRV2605_MODE_STANDBY_POS)
#define DRV2605_MODE_ACTIVE                                     ((0) << DRV2605_MODE_STANDBY_POS)
#define DRV2605_MODE_STANDBY                                    ((1) << DRV2605_MODE_STANDBY_POS)

#define DRV2605_MODE_MODE_POS                                   0
#define DRV2605_MODE_MODE_MASK                                   ((7) << DRV2605_MODE_MODE_POS)
#define DRV2605_MODE_INTERNAL_TRIGGER                           ((0) << DRV2605_MODE_MODE_POS)
#define DRV2605_MODE_EXTERNAL_TRIGGER_EDGE                      ((1) << DRV2605_MODE_MODE_POS)
#define DRV2605_MODE_EXTERNAL_TRIGGER_LEVEL                     ((2) << DRV2605_MODE_MODE_POS)
#define DRV2605_MODE_PWM_ANALOG_INPUT                           ((3) << DRV2605_MODE_MODE_POS)
#define DRV2605_MODE_AUDIO_TO_VIBE                              ((4) << DRV2605_MODE_MODE_POS)
#define DRV2605_MODE_RTP                                        ((5) << DRV2605_MODE_MODE_POS)
#define DRV2605_MODE_DIAGNOSTICS                                ((6) << DRV2605_MODE_MODE_POS)
#define DRV2605_MODE_AUTO_CALIBRATION                           ((7) << DRV2605_MODE_MODE_POS)

#define DRV2605_REAL_TIME_PLAYBACK_INPUT_ADDR                   0x02

#define DRV2605_WAVEFORM_CONTROL_ADDR                           0x03
#define DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_POS                0
#define DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_EMPTY              ((0x00) << DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_POS)
#define DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_A                  ((0x01) << DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_POS)
#define DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_B                  ((0x02) << DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_POS)
#define DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_C                  ((0x03) << DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_POS)
#define DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_D                  ((0x04) << DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_POS)
#define DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_E                  ((0x05) << DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_POS)
#define DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_LRA                ((0x06) << DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_POS)

#define DRV2605_WAVEFORM_CONTROL_HI_Z_POS                       4

#define DRV2605_WAVEFORM_SEQUENCER_ADDR                         0x04

#define DRV2605_GO_ADDR                                         0x0C
#define DRV2605_GO_GO_POS                                       0
#define DRV2605_GO_GO                                           ((1) << DRV2605_GO_GO_POS)


#define DRV2605_OVERDRIVE_TIME_OFFSET_ADDR                      0x0D
#define DRV2605_SUSTAIN_TIME_OFFSET_POSITIVE_ADDR               0x0E
#define DRV2605_SUSTAIN_TIME_OFFSET_NEGATIVE_ADDR               0x0F
#define DRV2605_BRAKE_TIME_OFFSET_ADDR                          0x10

#define DRV2605_AUDIO_TO_VIBE_CONTROL_ADDR                      0x11
#define DRV2605_AUDIO_TO_VIBE_CONTROL_ATH_PEAK_TIME_POS         2
#define DRV2605_AUDIO_TO_VIBE_CONTROL_ATH_FILTER_POS            0

#define DRV2605_AUDIO_TO_VIBE_MINIMUM_INPUT_LEVEL_ADDR          0x12
#define DRV2605_AUDIO_TO_VIBE_MAXIMUM_INPUT_LEVEL_ADDR          0x13
#define DRV2605_AUDIO_TO_DRIVE_MINIMUM_OUTPUT_DRIVE_ADDR        0x14
#define DRV2605_AUDIO_TO_DRIVE_MAXIMUM_OUTPUT_DRIVE_ADDR        0x15
#define DRV2605_RATED_VOLTAGE_ADDR                              0x16
#define DRV2605_OVERDRIVE_CLAMP_VOLTAGE_ADDR                    0x17
#define DRV2605_AUTO_CALIBRATION_COMPENSATION_RESULT_ADDR       0x18
#define DRV2605_AUTO_CALIBRATION_BACK_EMF_RESULT_ADDR           0x19

#define DRV2605_FEEDBACK_CONTROL_ADDR                           0x1A

#define DRV2605_FEEDBACK_CONTROL_N_ERM_LRA_POS                  7
#define DRV2605_FEEDBACK_CONTROL_N_ERM                          ((0) << DRV2605_FEEDBACK_CONTROL_N_ERM_LRA_POS)
#define DRV2605_FEEDBACK_CONTROL_N_LRA                          ((1) << DRV2605_FEEDBACK_CONTROL_N_ERM_LRA_POS)

#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS            4
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_MAX            7
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_MASK           ((DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_MAX) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_1X             ((0) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_2X             ((1) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_3X             ((2) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_4X             ((3) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_6X             ((4) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_8X             ((5) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_16X            ((6) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)
#define DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_DISBLED        ((7) << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS)


#define DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_POS                  2
#define DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_MAX                  3
#define DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_MASK                 ((DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_MAX) << DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_POS)

#define DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_LOW                  ((0) << DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_POS)
#define DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_MEDIUM               ((1) << DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_POS)
#define DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_HIGH                 ((2) << DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_POS)
#define DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_VERY_HIGH            ((3) << DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_POS)


#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_POS                  0
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_MAX                  3
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_LRA_3P75X            ((0) << DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_POS)
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_LRA_7P5X             ((1) << DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_POS)
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_LRA_15X              ((2) << DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_POS)
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_LRA_22X              ((3) << DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_POS)
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_ERM_0P255X           DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_LRA_3P75X
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_ERM_0P7875X          DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_LRA_7P5X
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_ERM_1P365X           DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_LRA_15X
#define DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_ERM_3P0X             DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_LRA_22X

#define DRV2605_CONTROL1_ADDR                                   0x1B
#define DRV2605_CONTROL1_STARTUP_BOOST_POS                      7
#define DRV2605_CONTROL1_STARTUP_BOOST_DISABLE                  ((0) << DRV2605_CONTROL1_STARTUP_BOOST_POS)
#define DRV2605_CONTROL1_STARTUP_BOOST_ENABLE                   ((1) << DRV2605_CONTROL1_STARTUP_BOOST_POS)

#define DRV2605_CONTROL1_AC_COUPLE_POS                          5
#define DRV2605_CONTROL1_DRIVE_TIME_POS                         0
#define DRV2605_CONTROL1_DRIVE_TIME_MAX                         31

#define DRV2605_CONTROL2_ADDR                                   0x1C
#define DRV2605_CONTROL2_BIDIR_INPUT_POS                        7
#define DRV2605_CONTROL2_BIDIR_INPUT_UNI                        ((0) << DRV2605_CONTROL2_BIDIR_INPUT_POS)
#define DRV2605_CONTROL2_BIDIR_INPUT_BI                         ((1) << DRV2605_CONTROL2_BIDIR_INPUT_POS)

#define DRV2605_CONTROL2_BRAKE_STABILIZER_POS                   6
#define DRV2605_CONTROL2_BRAKE_STABILIZER_ENABLE                ((1) << DRV2605_CONTROL2_BRAKE_STABILIZER_POS)
#define DRV2605_CONTROL2_BRAKE_STABILIZER_DISABLE               ((0) << DRV2605_CONTROL2_BRAKE_STABILIZER_POS)

#define DRV2605_CONTROL2_SAMPLE_TIME_POS                        4
#define DRV2605_CONTROL2_SAMPLE_TIME_MAX                        3
#define DRV2605_CONTROL2_SAMPLE_TIME_MASK                       ((DRV2605_CONTROL2_SAMPLE_TIME_MAX) << DRV2605_CONTROL2_SAMPLE_TIME_POS)

#define DRV2605_CONTROL2_SAMPLE_TIME_150                        ((0) << DRV2605_CONTROL2_SAMPLE_TIME_POS)
#define DRV2605_CONTROL2_SAMPLE_TIME_200                        ((1) << DRV2605_CONTROL2_SAMPLE_TIME_POS)
#define DRV2605_CONTROL2_SAMPLE_TIME_250                        ((2) << DRV2605_CONTROL2_SAMPLE_TIME_POS)
#define DRV2605_CONTROL2_SAMPLE_TIME_300                        ((3) << DRV2605_CONTROL2_SAMPLE_TIME_POS)

#define DRV2605_BLANKING_TIME_MAX                               15
#define DRV2605_CONTROL2_BLANKING_TIME_LSB_POS                  2

#define DRV2605_IDISS_TIME_MAX                                  15

#define DRV2605_CONTROL2_IDISS_TIME_LSB_POS                     0


#define DRV2605_CONTROL3_ADDR                                   0x1D
#define DRV2605_CONTROL3_NG_THRESH_POS                          6
#define DRV2605_CONTROL3_NG_THRESH_DISABLED                     ((0) << DRV2605_CONTROL3_NG_THRESH_POS)
#define DRV2605_CONTROL3_NG_THRESH_2PERCENT                     ((1) << DRV2605_CONTROL3_NG_THRESH_POS)
#define DRV2605_CONTROL3_NG_THRESH_4PERCENT                     ((2) << DRV2605_CONTROL3_NG_THRESH_POS)
#define DRV2605_CONTROL3_NG_THRESH_8PERCENT                     ((3) << DRV2605_CONTROL3_NG_THRESH_POS)


#define DRV2605_CONTROL3_ERM_OPEN_LOOP_POS                      5
#define DRV2605_CONTROL3_ERM_OPEN_LOOP_DISABLED                 ((0) << DRV2605_CONTROL3_ERM_OPEN_LOOP_POS)
#define DRV2605_CONTROL3_ERM_OPEN_LOOP_ENABLED                  ((1) << DRV2605_CONTROL3_ERM_OPEN_LOOP_POS)

#define DRV2605_CONTROL3_SUPPLY_COMP_DIS_POS                    4
#define DRV2605_CONTROL3_SUPPLY_COMP_DIS_ENABLED                ((0) << DRV2605_CONTROL3_SUPPLY_COMP_DIS_POS)
#define DRV2605_CONTROL3_SUPPLY_COMP_DIS_DISABLED               ((1) << DRV2605_CONTROL3_SUPPLY_COMP_DIS_POS)

#define DRV2605_CONTROL3_DATA_FORMAT_RTP_POS                    3
#define DRV2605_CONTROL3_DATA_FORMAT_RTP_SIGNED                 ((0) << DRV2605_CONTROL3_DATA_FORMAT_RTP_POS)
#define DRV2605_CONTROL3_DATA_FORMAT_RTP_UNSIGNED               ((1) << DRV2605_CONTROL3_DATA_FORMAT_RTP_POS)

#define DRV2605_CONTROL3_LRA_DRIVE_MODE_POS                     2
#define DRV2605_CONTROL3_LRA_DRIVE_MODE_ONCE                    ((0) << DRV2605_CONTROL3_LRA_DRIVE_MODE_POS)
#define DRV2605_CONTROL3_LRA_DRIVE_MODE_TWICE                   ((1) << DRV2605_CONTROL3_LRA_DRIVE_MODE_POS)

#define DRV2605_CONTROL3_N_PWM_ANALOG_POS                       1
#define DRV2605_CONTROL3_N_PWM_ANALOG_MASK                      ((1) << DRV2605_CONTROL3_N_PWM_ANALOG_POS)
#define DRV2605_CONTROL3_N_PWM_ANALOG_PWM                       ((0) << DRV2605_CONTROL3_N_PWM_ANALOG_POS)
#define DRV2605_CONTROL3_N_PWM_ANALOG_ANALOG                    ((1) << DRV2605_CONTROL3_N_PWM_ANALOG_POS)

#define DRV2605_CONTROL3_LRA_OPEN_LOOP_POS                      0
#define DRV2605_CONTROL3_LRA_OPEN_LOOP_AUTO                     ((0) << DRV2605_CONTROL3_LRA_OPEN_LOOP_POS)
#define DRV2605_CONTROL3_LRA_OPEN_LOOP_OPEN                     ((1) << DRV2605_CONTROL3_LRA_OPEN_LOOP_POS)
#define DRV2605_CONTROL3_LRA_OPEN_LOOP_CLOSED                   DRV2605_CONTROL3_LRA_OPEN_LOOP_AUTO

#define DRV2605_CONTROL4_ADDR                                   0x1E

#define DRV2605_CONTROL4_ZC_DET_TIME_POS                        6
#define DRV2605_CONTROL4_ZC_DET_TIME_MAX                        3
#define DRV2605_CONTROL4_ZC_DET_TIME_100                        ((0) << DRV2605_CONTROL4_ZC_DET_TIME_POS)
#define DRV2605_CONTROL4_ZC_DET_TIME_200                        ((1) << DRV2605_CONTROL4_ZC_DET_TIME_POS)
#define DRV2605_CONTROL4_ZC_DET_TIME_300                        ((2) << DRV2605_CONTROL4_ZC_DET_TIME_POS)
#define DRV2605_CONTROL4_ZC_DET_TIME_390                        ((3) << DRV2605_CONTROL4_ZC_DET_TIME_POS)

#define DRV2605_CONTROL4_AUTO_CAL_TIME_POS                      4
#define DRV2605_CONTROL4_AUTO_CAL_TIME_MAX                      3
#define DRV2605_CONTROL4_AUTO_CAL_TIME_150                      ((0) << DRV2605_CONTROL4_AUTO_CAL_TIME_POS)
#define DRV2605_CONTROL4_AUTO_CAL_TIME_250                      ((1) << DRV2605_CONTROL4_AUTO_CAL_TIME_POS)
#define DRV2605_CONTROL4_AUTO_CAL_TIME_500                      ((2) << DRV2605_CONTROL4_AUTO_CAL_TIME_POS)
#define DRV2605_CONTROL4_AUTO_CAL_TIME_1000                     ((3) << DRV2605_CONTROL4_AUTO_CAL_TIME_POS)

#define DRV2605_CONTROL4_OTP_STATUS_POS                         2
#define DRV2605_CONTROL4_OTP_PROGRAM_POS                        0

#define DRV2605_CONTROL5_ADDR                                   0x1F
#define DRV2605_CONTROL5_AUTO_OL_CNT_POS                        6
#define DRV2605_CONTROL5_AUTO_OL_CNT_3                          ((0) << DRV2605_CONTROL5_AUTO_OL_CNT_POS)
#define DRV2605_CONTROL5_AUTO_OL_CNT_4                          ((1) << DRV2605_CONTROL5_AUTO_OL_CNT_POS)
#define DRV2605_CONTROL5_AUTO_OL_CNT_5                          ((2) << DRV2605_CONTROL5_AUTO_OL_CNT_POS)
#define DRV2605_CONTROL5_AUTO_OL_CNT_6                          ((3) << DRV2605_CONTROL5_AUTO_OL_CNT_POS)

#define DRV2605_CONTROL5_LRA_AUTO_OPEN_LOOP_POS                 5
#define DRV2605_CONTROL5_LRA_AUTO_OPEN_LOOP_NEVER               ((0) << DRV2605_CONTROL5_LRA_AUTO_OPEN_LOOP_POS)
#define DRV2605_CONTROL5_LRA_AUTO_OPEN_LOOP_AUTO                ((1) << DRV2605_CONTROL5_LRA_AUTO_OPEN_LOOP_POS)

#define DRV2605_CONTROL5_PLAYBACK_INTERVAL_POS                  4
#define DRV2605_CONTROL5_PLAYBACK_INTERVAL_5                    ((0) << DRV2605_CONTROL5_PLAYBACK_INTERVAL_POS)
#define DRV2605_CONTROL5_PLAYBACK_INTERVAL_1                    ((1) << DRV2605_CONTROL5_PLAYBACK_INTERVAL_POS)

#define DRV2605_CONTROL5_BLANKING_TIME_MSB_POS                  2
#define DRV2605_CONTROL5_IDISS_TIME_MSB_POS                     0

#define DRV2605_LRA_OPEN_LOOP_PERIOD_ADDR                       0x20

#define DRV2605_VBAT_VOLTAGE_MONITOR_ADDR                       0x21
#define DRV2605_LRA_RESONANCE_PERIOD_ADDR                       0x22

struct sensor_itf;

/**
 * Reads a single byte from the specified register
 *
 * @param The sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);


/**
 * Read data from the sensor of variable length (MAX: 8 bytes)
 *
 * @param The Sensor interface
 * @param Register to read from
 * @param Buffer to read into
 * @param Length of the buffer
 *
 * @return 0 on success and non-zero on failure
 */
int
drv2605_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
               uint8_t len);

/**
 * Writes a single byte to the specified register
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);

/**
 * Writes a multiple bytes to the specified register
 *
 * @param The sensor interface
 * @param The register address to write to
 * @param The data buffer to write from
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_writelen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __DRV2605_PRIV_H__ */

