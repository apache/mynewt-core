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

#ifndef __LIS2DS12_PRIV_H__
#define __LIS2DS12_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LIS2DS12_ID                             0x43

#define LIS2DS12_REG_WHO_AM_I                   0x0F

#define LIS2DS12_REG_CTRL_REG1                  0x20
#define LIS2DS12_CTRL_REG1_ODR                  (0xf << 4)
#define LIS2DS12_CTRL_REG1_FS                   (0x3 << 2)
#define LIS2DS12_CTRL_REG1_HF_ODR               (0x1 << 1)
#define LIS2DS12_CTRL_REG1_BDU                  (0x1 << 0)

/* Be careful to preserve CTRL_REG2 as LIS2DS12_CTRL_REG2_IF_ADD_INC
 * is the one (and thankfully only) default enabled register bit on this device
 */
#define LIS2DS12_REG_CTRL_REG2                  0x21
#define LIS2DS12_CTRL_REG2_BOOT                 (1 << 7)
#define LIS2DS12_CTRL_REG2_SOFT_RESET           (1 << 6)
#define LIS2DS12_CTRL_REG2_FDS_SLOPE            (1 << 3)
#define LIS2DS12_CTRL_REG2_IF_ADD_INC           (1 << 2)
#define LIS2DS12_CTRL_REG2_I2C_DISABLE          (1 << 1)
#define LIS2DS12_CTRL_REG2_SIM                  (1 << 0)

#define LIS2DS12_REG_CTRL_REG3                  0x22
#define LIS2DS12_CTRL_REG3_ST_MODE              (0x3 << 6)
#define LIS2DS12_CTRL_REG3_TAP_X_EN             (1 << 5)
#define LIS2DS12_CTRL_REG3_TAP_Y_EN             (1 << 4)
#define LIS2DS12_CTRL_REG3_TAP_Z_EN             (1 << 3)
#define LIS2DS12_CTRL_REG3_LIR                  (1 << 2)
#define LIS2DS12_CTRL_REG3_H_LACTIVE            (1 << 1)
#define LIS2DS12_CTRL_REG3_PP_OD                (1 << 0)

#define LIS2DS12_REG_CTRL_REG4                  0x23
#define LIS2DS12_CTRL_REG4_INT1_MASTER_DRDY     (1 << 7)
#define LIS2DS12_CTRL_REG4_INT1_S_TAP           (1 << 6)
#define LIS2DS12_CTRL_REG4_INT1_WU              (1 << 5)
#define LIS2DS12_CTRL_REG4_INT1_FF              (1 << 4)
#define LIS2DS12_CTRL_REG4_INT1_TAP             (1 << 3)
#define LIS2DS12_CTRL_REG4_INT1_6D              (1 << 2)
#define LIS2DS12_CTRL_REG4_INT1_FTH             (1 << 1)
#define LIS2DS12_CTRL_REG4_INT1_DRDY            (1 << 0)

#define LIS2DS12_REG_CTRL_REG5                  0x24
#define LIS2DS12_CTRL_REG5_DRDY_PULSED	     	(1 << 7)
#define LIS2DS12_CTRL_REG5_INT2_BOOT	     	(1 << 6)
#define LIS2DS12_CTRL_REG5_INT2_ON_INT1	     	(1 << 5)
#define LIS2DS12_CTRL_REG5_INT2_TILT	     	(1 << 4)
#define LIS2DS12_CTRL_REG5_INT2_SIG_MOT	     	(1 << 3)
#define LIS2DS12_CTRL_REG5_INT2_STEP_DET	    (1 << 2)
#define LIS2DS12_CTRL_REG5_INT2_FTH	     		(1 << 1)
#define LIS2DS12_CTRL_REG5_INT2_DRDY	     	(1 << 0)

#define LIS2DS12_REG_FIFO_CTRL                  0x25
#define LIS2DS12_CTRL_FIFO_FMODE                (0x07 << 5)
#define LIS2DS12_CTRL_FIFO_INT2_STEP_COUNT_OV   (1 << 4)
#define LIS2DS12_CTRL_FIFO_MODULE_TO_FIFO       (1 << 3)
#define LIS2DS12_CTRL_FIFO_IF_CS_PU_DIS         (1 << 0)

#define LIS2DS12_REG_TEMP_OUT                   0x26

#define LIS2DS12_REG_STATUS                     0x27

#define LIS2DS12_REG_OUT_X_L                    0x28
#define LIS2DS12_REG_OUT_X_H                    0x29
#define LIS2DS12_REG_OUT_Y_L                    0x2A
#define LIS2DS12_REG_OUT_Y_H                    0x2B
#define LIS2DS12_REG_OUT_Z_L                    0x2C
#define LIS2DS12_REG_OUT_Z_H                    0x2D

#define LIS2DS12_REG_FIFO_THS                   0x2E

#define LIS2DS12_REG_FIFO_SRC                   0x2F
#define LIS2DS12_FIFO_SRC_FTH                   (1 << 7)
#define LIS2DS12_FIFO_SRC_OVR                   (1 << 6)
#define LIS2DS12_FIFO_SRC_DIFF8                 (1 << 5)

#define LIS2DS12_REG_FIFO_SAMPLES               0x30
    
#define LIS2DS12_REG_TAP_6D_THS                 0x31
#define LIS2DS12_TAP_6D_THS_4D_EN               (1 << 7)
#define LIS2DS12_TAP_6D_THS_6D_THS              (0x03 << 5)
#define LIS2DS12_TAP_6D_THS_TAP_THS             (0x1F << 0)

#define LIS2DS12_REG_INT_DUR                    0x32
#define LIS2DS12_INT_DUR_LATENCY                (0xf << 4)
#define LIS2DS12_INT_DUR_QUIET                  (0x3 << 2)
#define LIS2DS12_INT_DUR_SHOCK                  (0x3 << 0)

#define LIS2DS12_REG_WAKE_UP_THS                0x33
#define LIS2DS12_WAKE_THS_SINGLE_DOUBLE_TAP     (1 << 7)
#define LIS2DS12_WAKE_THS_SLEEP_ON              (1 << 6)
#define LIS2DS12_WAKE_THS_THS                   (0x3f << 0)

#define LIS2DS12_REG_WAKE_UP_DUR                0x34
#define LIS2DS12_WAKE_DUR_FF_DUR                (1 << 7)
#define LIS2DS12_WAKE_DUR_DUR                   (0x03 << 5)
#define LIS2DS12_WAKE_DUR_SLEEP_DUR             (0xf << 0)

#define LIS2DS12_REG_FREEFALL                   0x35
#define LIS2DS12_FREEFALL_DUR                   (0x1F << 3)
#define LIS2DS12_FREEFALL_THS                   (0x7 << 0)

#define LIS2DS12_REG_STATUS_DUP                 0x36
#define LIS2DS12_STATUS_DUP_OVR                 (1 << 7)
#define LIS2DS12_STATUS_DUP_WU_IA               (1 << 6)
#define LIS2DS12_STATUS_DUP_SLEEP_STATE         (1 << 5)
#define LIS2DS12_STATUS_DUP_DOUBLE_TAP          (1 << 4)
#define LIS2DS12_STATUS_DUP_SINGLE_TAP          (1 << 3)
#define LIS2DS12_STATUS_DUP_6D_IA               (1 << 2)
#define LIS2DS12_STATUS_DUP_FF_IA               (1 << 1)
#define LIS2DS12_STATUS_DUP_DRDY                (1 << 0)

#define LIS2DS12_REG_WAKE_UP_SRC                0x37

#define LIS2DS12_REG_TAP_SRC                    0x38

#define LIS2DS12_REG_6D_SRC                     0x39
#define LIS2DS12_6D_SRC_6D_IA                   (1 << 6)
#define LIS2DS12_6D_SRC_ZH                      (1 << 5)
#define LIS2DS12_6D_SRC_ZL                      (1 << 4)
#define LIS2DS12_6D_SRC_YH                      (1 << 3)
#define LIS2DS12_6D_SRC_YL                      (1 << 2)
#define LIS2DS12_6D_SRC_XH                      (1 << 1)
#define LIS2DS12_6D_SRC_XL                      (1 << 0)

#define LIS2DS12_REG_STEP_COUNTER_MINTHS        0x3A
#define LIS2DS12_STEP_COUNTER_MINTHS_RST_nSTEP  (1 << 7)
#define LIS2DS12_STEP_COUNTER_MINTHS_PEDO4g     (1 << 6)
#define LIS2DS12_STEP_COUNTER_MINTHS_SC_MTHS    (0x3f << 0)

#define LIS2DS12_REG_STEP_COUNTER_L             0x3B
#define LIS2DS12_REG_STEP_COUNTER_H             0x3C

#define LIS2DS12_SPI_READ_CMD_BIT               0x80


int lis2ds12_i2c_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len);

int lis2ds12_spi_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len);

int lis2ds12_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);
int lis2ds12_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);
int lis2ds12_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer, uint8_t len);

void lis2ds12_calc_acc_ms2(int16_t raw_acc, float *facc);
void lis2ds12_calc_acc_mg(float acc_ms2, int16_t *acc_mg);

int lis2ds12_get_data(struct sensor_itf *itf, uint8_t fs, int16_t *x, int16_t *y, int16_t *z);

int lis2ds12_get_fs(struct sensor_itf *itf, uint8_t *fs);

#ifdef __cplusplus
}
#endif

#endif /* __LIS2DS12_PRIV_H_ */
