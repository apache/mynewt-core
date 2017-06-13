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

/* Page id register definition */
#define BNO055_PAGE_ID_ADDR                                     0X07

/* PAGE0 REGISTER DEFINITION START*/
#define BNO055_CHIP_ID_ADDR                                     0x00
#define BNO055_ACCEL_REV_ID_ADDR                                0x01
#define BNO055_MAG_REV_ID_ADDR                                  0x02
#define BNO055_GYRO_REV_ID_ADDR                                 0x03
#define BNO055_SW_REV_ID_LSB_ADDR                               0x04
#define BNO055_SW_REV_ID_MSB_ADDR                               0x05
#define BNO055_BL_REV_ID_ADDR                                   0X06
/* Accel data register */
#define BNO055_ACCEL_DATA_X_LSB_ADDR                            0X08
#define BNO055_ACCEL_DATA_X_MSB_ADDR                            0X09
#define BNO055_ACCEL_DATA_Y_LSB_ADDR                            0X0A
#define BNO055_ACCEL_DATA_Y_MSB_ADDR                            0X0B
#define BNO055_ACCEL_DATA_Z_LSB_ADDR                            0X0C
#define BNO055_ACCEL_DATA_Z_MSB_ADDR                            0X0D

/* Mag data register */
#define BNO055_MAG_DATA_X_LSB_ADDR                              0X0E
#define BNO055_MAG_DATA_X_MSB_ADDR                              0X0F
#define BNO055_MAG_DATA_Y_LSB_ADDR                              0X10
#define BNO055_MAG_DATA_Y_MSB_ADDR                              0X11
#define BNO055_MAG_DATA_Z_LSB_ADDR                              0X12
#define BNO055_MAG_DATA_Z_MSB_ADDR                              0X13

/* Gyro data registers */
#define BNO055_GYRO_DATA_X_LSB_ADDR                             0X14
#define BNO055_GYRO_DATA_X_MSB_ADDR                             0X15
#define BNO055_GYRO_DATA_Y_LSB_ADDR                             0X16
#define BNO055_GYRO_DATA_Y_MSB_ADDR                             0X17
#define BNO055_GYRO_DATA_Z_LSB_ADDR                             0X18
#define BNO055_GYRO_DATA_Z_MSB_ADDR                             0X19

/* Euler data registers */
#define BNO055_EULER_H_LSB_ADDR                                 0X1A
#define BNO055_EULER_H_MSB_ADDR                                 0X1B
#define BNO055_EULER_R_LSB_ADDR                                 0X1C
#define BNO055_EULER_R_MSB_ADDR                                 0X1D
#define BNO055_EULER_P_LSB_ADDR                                 0X1E
#define BNO055_EULER_P_MSB_ADDR                                 0X1F

/* Quaternion data registers */
#define BNO055_QUATERNION_DATA_W_LSB_ADDR                       0X20
#define BNO055_QUATERNION_DATA_W_MSB_ADDR                       0X21
#define BNO055_QUATERNION_DATA_X_LSB_ADDR                       0X22
#define BNO055_QUATERNION_DATA_X_MSB_ADDR                       0X23
#define BNO055_QUATERNION_DATA_Y_LSB_ADDR                       0X24
#define BNO055_QUATERNION_DATA_Y_MSB_ADDR                       0X25
#define BNO055_QUATERNION_DATA_Z_LSB_ADDR                       0X26
#define BNO055_QUATERNION_DATA_Z_MSB_ADDR                       0X27

/* Linear acceleration data registers */
#define BNO055_LINEAR_ACCEL_DATA_X_LSB_ADDR                     0X28
#define BNO055_LINEAR_ACCEL_DATA_X_MSB_ADDR                     0X29
#define BNO055_LINEAR_ACCEL_DATA_Y_LSB_ADDR                     0X2A
#define BNO055_LINEAR_ACCEL_DATA_Y_MSB_ADDR                     0X2B
#define BNO055_LINEAR_ACCEL_DATA_Z_LSB_ADDR                     0X2C
#define BNO055_LINEAR_ACCEL_DATA_Z_MSB_ADDR                     0X2D

/* Gravity data registers */
#define BNO055_GRAVITY_DATA_X_LSB_ADDR                          0X2E
#define BNO055_GRAVITY_DATA_X_MSB_ADDR                          0X2F
#define BNO055_GRAVITY_DATA_Y_LSB_ADDR                          0X30
#define BNO055_GRAVITY_DATA_Y_MSB_ADDR                          0X31
#define BNO055_GRAVITY_DATA_Z_LSB_ADDR                          0X32
#define BNO055_GRAVITY_DATA_Z_MSB_ADDR                          0X33

/* Temperature data register */
#define BNO055_TEMP_ADDR                                        0X34

/* Status registers */
#define BNO055_CALIB_STAT_ADDR                                  0X35
#define BNO055_SELFTEST_RESULT_ADDR                             0X36
#define BNO055_INTR_STAT_ADDR                                   0X37

#define BNO055_SYS_CLK_STAT_ADDR                                0X38
#define BNO055_SYS_STAT_ADDR                                    0X39
#define BNO055_SYS_ERR_ADDR                                     0X3A

/* Unit selection register */
#define BNO055_UNIT_SEL_ADDR                                    0X3B

#define BNO055_DATA_SELECT_ADDR                                 0X3C

/* Mode registers */
#define BNO055_OPR_MODE_ADDR                                    0X3D
#define BNO055_PWR_MODE_ADDR                                    0X3E

#define BNO055_SYS_TRIGGER_ADDR                                 0X3F
#define BNO055_SYS_TRIGGER_CLK_SEL                       (0x01 << 7)
#define BNO055_SYS_TRIGGER_RST_INT                       (0x01 << 6)
#define BNO055_SYS_TRIGGER_RST_SYS                       (0x01 << 5)
#define BNO055_SYS_TRIGGER_SELF_TEST                          (0x01)

#define BNO055_TEMP_SOURCE_ADDR                                 0X40

/* Axis remap registers */
#define BNO055_AXIS_MAP_CONFIG_ADDR                             0X41
#define BNO055_AXIS_MAP_SIGN_ADDR                               0X42

/* SIC registers */
#define BNO055_SIC_MATRIX_0_LSB_ADDR                            0X43
#define BNO055_SIC_MATRIX_0_MSB_ADDR                            0X44
#define BNO055_SIC_MATRIX_1_LSB_ADDR                            0X45
#define BNO055_SIC_MATRIX_1_MSB_ADDR                            0X46
#define BNO055_SIC_MATRIX_2_LSB_ADDR                            0X47
#define BNO055_SIC_MATRIX_2_MSB_ADDR                            0X48
#define BNO055_SIC_MATRIX_3_LSB_ADDR                            0X49
#define BNO055_SIC_MATRIX_3_MSB_ADDR                            0X4A
#define BNO055_SIC_MATRIX_4_LSB_ADDR                            0X4B
#define BNO055_SIC_MATRIX_4_MSB_ADDR                            0X4C
#define BNO055_SIC_MATRIX_5_LSB_ADDR                            0X4D
#define BNO055_SIC_MATRIX_5_MSB_ADDR                            0X4E
#define BNO055_SIC_MATRIX_6_LSB_ADDR                            0X4F
#define BNO055_SIC_MATRIX_6_MSB_ADDR                            0X50
#define BNO055_SIC_MATRIX_7_LSB_ADDR                            0X51
#define BNO055_SIC_MATRIX_7_MSB_ADDR                            0X52
#define BNO055_SIC_MATRIX_8_LSB_ADDR                            0X53
#define BNO055_SIC_MATRIX_8_MSB_ADDR                            0X54

/* Accelerometer Offset registers */
#define BNO055_ACCEL_OFFSET_X_LSB_ADDR                          0X55
#define BNO055_ACCEL_OFFSET_X_MSB_ADDR                          0X56
#define BNO055_ACCEL_OFFSET_Y_LSB_ADDR                          0X57
#define BNO055_ACCEL_OFFSET_Y_MSB_ADDR                          0X58
#define BNO055_ACCEL_OFFSET_Z_LSB_ADDR                          0X59
#define BNO055_ACCEL_OFFSET_Z_MSB_ADDR                          0X5A

/* Magnetometer Offset registers */
#define BNO055_MAG_OFFSET_X_LSB_ADDR                            0X5B
#define BNO055_MAG_OFFSET_X_MSB_ADDR                            0X5C
#define BNO055_MAG_OFFSET_Y_LSB_ADDR                            0X5D
#define BNO055_MAG_OFFSET_Y_MSB_ADDR                            0X5E
#define BNO055_MAG_OFFSET_Z_LSB_ADDR                            0X5F
#define BNO055_MAG_OFFSET_Z_MSB_ADDR                            0X60

/* Gyroscope Offset register s*/
#define BNO055_GYRO_OFFSET_X_LSB_ADDR                           0X61
#define BNO055_GYRO_OFFSET_X_MSB_ADDR                           0X62
#define BNO055_GYRO_OFFSET_Y_LSB_ADDR                           0X63
#define BNO055_GYRO_OFFSET_Y_MSB_ADDR                           0X64
#define BNO055_GYRO_OFFSET_Z_LSB_ADDR                           0X65
#define BNO055_GYRO_OFFSET_Z_MSB_ADDR                           0X66

/* Radius registers */
#define BNO055_ACCEL_RADIUS_LSB_ADDR                            0X67
#define BNO055_ACCEL_RADIUS_MSB_ADDR                            0X68
#define BNO055_MAG_RADIUS_LSB_ADDR                              0X69
#define BNO055_MAG_RADIUS_MSB_ADDR                              0X6A

/* Remap config - Default: 0x24 */
#define BNO055_REMAP_CONFIG_P0                                  0x21
#define BNO055_REMAP_CONFIG_P1                                  0x24
#define BNO055_REMAP_CONFIG_P2                                  0x24
#define BNO055_REMAP_CONFIG_P3                                  0x21
#define BNO055_REMAP_CONFIG_P4                                  0x24
#define BNO055_REMAP_CONFIG_P5                                  0x21
#define BNO055_REMAP_CONFIG_P6                                  0x21
#define BNO055_REMAP_CONFIG_P7                                  0x24

/* Remap Sign - Default: 0x00 */
#define BNO055_REMAP_SIGN_P0                                    0x04
#define BNO055_REMAP_SIGN_P1                                    0x00
#define BNO055_REMAP_SIGN_P2                                    0x06
#define BNO055_REMAP_SIGN_P3                                    0x02
#define BNO055_REMAP_SIGN_P4                                    0x03
#define BNO055_REMAP_SIGN_P5                                    0x01
#define BNO055_REMAP_SIGN_P6                                    0x07
#define BNO055_REMAP_SIGN_P7                                    0x05

/* PAGE1 REGISTERS DEFINITION START*/
/* Configuration registers*/
#define BNO055_ACCEL_CONFIG_ADDR                                0X08
#define BNO055_MAG_CONFIG_ADDR                                  0X09
#define BNO055_GYRO_CONFIG_ADDR                                 0X0A
#define BNO055_GYRO_MODE_CONFIG_ADDR                            0X0B
#define BNO055_ACCEL_SLEEP_CONFIG_ADDR                          0X0C
#define BNO055_GYRO_SLEEP_CONFIG_ADDR                           0X0D
#define BNO055_MAG_SLEEP_CONFIG_ADDR                            0x0E

/* Interrupt Registers */
#define BNO055_INT_MASK_ADDR                                    0X0F

#define BNO055_INT_EN_ADDR                                      0X10
#define BNO055_INT_EN_ACC_NM                                (1 << 7)
#define BNO055_INT_EN_ACC_AM                                (1 << 6)
#define BNO055_INT_EN_ACC_HG                                (1 << 5)
#define BNO055_INT_EN_GYR_HR                                (1 << 3)
#define BNO055_INT_EN_GYR_AM                                (1 << 2)

#define BNO055_ACCEL_ANY_MOTION_THRES_ADDR                      0X11
#define BNO055_ACCEL_INTR_SETTINGS_ADDR                         0X12
#define BNO055_ACCEL_HIGH_G_DURN_ADDR                           0X13
#define BNO055_ACCEL_HIGH_G_THRES_ADDR                          0X14

#define BNO055_ACCEL_NO_MOTION_THRES_ADDR                       0X15
#define BNO055_ACCEL_SMNM                                   (1 << 0)

#define BNO055_ACCEL_NO_MOTION_SET_ADDR                         0X16
#define BNO055_GYRO_INTR_SETTINGS_ADDR                          0X17
#define BNO055_GYRO_HIGHRATE_X_SET_ADDR                         0X18
#define BNO055_GYRO_DURN_X_ADDR                                 0X19
#define BNO055_GYRO_HIGHRATE_Y_SET_ADDR                         0X1A
#define BNO055_GYRO_DURN_Y_ADDR                                 0X1B
#define BNO055_GYRO_HIGHRATE_Z_SET_ADDR                         0X1C
#define BNO055_GYRO_DURN_Z_ADDR                                 0X1D
#define BNO055_GYRO_ANY_MOTION_THRES_ADDR                       0X1E
#define BNO055_GYRO_ANY_MOTION_SET_ADDR                         0X1F

#define BNO055_NUM_OFFSET_REGISTERS                               22

#define BNO055_INT_ACC_AM_POS                                      0
#define BNO055_INT_ACC_HG_POS                                      8
#define BNO055_INT_GYR_HR_POS                                     16
#define BNO055_INT_GYR_AM_POS                                     24

#define BNO055_INT_ACC_AM                                          1
#define BNO055_INT_ACC_AM_X_AXIS   (1 << 2) << BNO055_INT_ACC_AM_POS
#define BNO055_INT_ACC_AM_Y_AXIS   (1 << 3) << BNO055_INT_ACC_AM_POS
#define BNO055_INT_ACC_AM_Z_AXIS   (1 << 4) << BNO055_INT_ACC_AM_POS

#define BNO055_INT_ACC_HG                                          2
#define BNO055_INT_ACC_HG_X_AXIS   (1 << 5) << BNO055_INT_ACC_HG_POS
#define BNO055_INT_ACC_HG_Y_AXIS   (1 << 6) << BNO055_INT_ACC_HG_POS
#define BNO055_INT_ACC_HG_Z_AXIS   (1 << 7) << BNO055_INT_ACC_HG_POS

#define BNO055_INT_GYR_HR                                          4
#define BNO055_INT_GYR_HR_X_AXIS   (1 << 3) << BNO055_INT_GYR_HR_POS
#define BNO055_INT_GYR_HR_Y_AXIS   (1 << 4) << BNO055_INT_GYR_HR_POS
#define BNO055_INT_GYR_HR_Z_AXIS   (1 << 5) << BNO055_INT_GYR_HR_POS

#define BNO055_INT_GYR_AM                                          8
#define BNO055_INT_GYR_AM_X_AXIS   (1 << 0) << BNO055_INT_GYR_AM_POS
#define BNO055_INT_GYR_AM_Y_AXIS   (1 << 1) << BNO055_INT_GYR_AM_POS
#define BNO055_INT_GYR_AM_Z_AXIS   (1 << 2) << BNO055_INT_GYR_AM_POS

#define BNO055_INT_ACC_NM                                         16

#define BNO055_INT_ACC_SM                                         32

#define BNO055_ID                                               0xA0

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
bno055_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);

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
bno055_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);

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
bno055_writelen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                uint8_t len);

