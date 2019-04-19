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

#ifndef __LIS2DE12_PRIV_H__
#define __LIS2DE12_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LIS2DE12_ID                          0x33

#define LIS2DE12_REG_STATUS_AUX              0x07
#define LIS2DE12_STATUS_AUX_TOR          (1 << 6)
#define LIS2DE12_STATUS_AUX_TDA          (1 << 2)

#define LIS2DE12_REG_OUT_TEMP_L              0x0C
#define LIS2DE12_REG_OUT_TEMP_H              0x0D

#define LIS2DE12_REG_WHO_AM_I                0x0F

#define LIS2DE12_REG_CTRL_REG0               0x1E
#define LIS2DE12_CTRL_REG0_SPD           (1 << 7)
#define LIS2DE12_CTRL_REG0_CORR_OP       (1 << 4)

#define LIS2DE12_REG_TEMP_CFG                0x1F
#define LIS2DE12_TEMP_CFG_EN           (0x3 << 6)

#define LIS2DE12_REG_CTRL_REG1               0x20
#define LIS2DE12_CTRL_REG1_ODR         (0xf << 4)
#define LIS2DE12_CTRL_REG1_LPEN          (1 << 3)
#define LIS2DE12_CTRL_REG1_ZPEN          (1 << 2)
#define LIS2DE12_CTRL_REG1_YPEN          (1 << 1)
#define LIS2DE12_CTRL_REG1_XPEN          (1 << 0)

#define LIS2DE12_REG_CTRL_REG2               0x21
#define LIS2DE12_CTRL_REG2_HPM         (0x3 << 6)
#define LIS2DE12_CTRL_REG2_HPCF        (0x3 << 4)
#define LIS2DE12_CTRL_REG2_FDS           (1 << 3)
#define LIS2DE12_CTRL_REG2_HPCLICK       (1 << 2)
#define LIS2DE12_CTRL_REG2_HPIA2         (1 << 1)
#define LIS2DE12_CTRL_REG2_HPIA1              (1)

#define LIS2DE12_REG_CTRL_REG3               0x22

#define LIS2DE12_REG_CTRL_REG4               0x23
#define LIS2DE12_CTRL_REG4_BDU           (1 << 7)
#define LIS2DE12_CTRL_REG4_FS          (0x3 << 4)
#define LIS2DE12_CTRL_REG4_ST          (0x3 << 1)
#define LIS2DE12_CTRL_REG4_SIM                (1)

#define LIS2DE12_REG_CTRL_REG5               0x24
#define LIS2DE12_CTRL_REG5_BOOT          (1 << 7)
#define LIS2DE12_CTRL_REG5_FIFO_EN       (1 << 6)
#define LIS2DE12_CTRL_REG5_LIR_INT1      (1 << 3)
#define LIS2DE12_CTRL_REG5_D4D_INT1      (1 << 2)
#define LIS2DE12_CTRL_REG5_LIR_INT2      (1 << 1)
#define LIS2DE12_CTRL_REG5_D4D_INT2           (1)

#define LIS2DE12_REG_CTRL_REG6               0x25

#define LIS2DE12_REG_REFERENCE               0x26

#define LIS2DE12_REG_STATUS_REG              0x27
#define LIS2DE12_STATUS_ZYXOR            (1 << 7)
#define LIS2DE12_STATUS_ZOR              (1 << 6)
#define LIS2DE12_STATUS_YOR              (1 << 5)
#define LIS2DE12_STATUS_XOR              (1 << 4)
#define LIS2DE12_STATUS_ZYXDA            (1 << 3)
#define LIS2DE12_STATUS_ZDA              (1 << 2)
#define LIS2DE12_STATUS_YDA              (1 << 1)
#define LIS2DE12_STATUS_XDA                   (1)

#define LIS2DE12_REG_OUT_X_L                 0x28
#define LIS2DE12_REG_OUT_X_H                 0x29
#define LIS2DE12_REG_OUT_Y_L                 0x2A
#define LIS2DE12_REG_OUT_Y_H                 0x2B
#define LIS2DE12_REG_OUT_Z_L                 0x2C
#define LIS2DE12_REG_OUT_Z_H                 0x2D

#define LIS2DE12_REG_FIFO_CTRL_REG           0x2E
#define LIS2DE12_FIFO_CTRL_REG_FM      (0x3 << 6)
#define LIS2DE12_FIFO_CTRL_REG_TR        (1 << 5)
#define LIS2DE12_FIFO_CTRL_REG_FTH         (0x1F)

#define LIS2DE12_REG_FIFO_SRC_REG            0x2F
#define LIS2DE12_FIFO_SRC_WTM            (1 << 7)
#define LIS2DE12_FIFO_SRC_OVRN_FIFO      (1 << 6)
#define LIS2DE12_FIFO_SRC_EMPTY          (1 << 5)
#define LIS2DE12_FIFO_SRC_FSS              (0x1f)

#define LIS2DE12_REG_INT1_CFG                0x30
#define LIS2DE12_INT1_CFG_AOI            (1 << 7)
#define LIS2DE12_INT1_CFG_6D             (1 << 6)
#define LIS2DE12_INT1_CFG_ZHIE           (1 << 5)
#define LIS2DE12_INT1_CFG_ZLIE           (1 << 4)
#define LIS2DE12_INT1_CFG_YHIE           (1 << 3)
#define LIS2DE12_INT1_CFG_YLIE           (1 << 2)
#define LIS2DE12_INT1_CFG_XHIE           (1 << 1)
#define LIS2DE12_INT1_CFG_XLIE                (1)

#define LIS2DE12_REG_INT1_SRC                0x31

#define LIS2DE12_REG_INT1_THS                0x32
#define LIS2DE12_INT1_THS                  (0x7f)

#define LIS2DE12_REG_INT1_DURATION           0x33

#define LIS2DE12_REG_INT2_CFG                0x34
#define LIS2DE12_INT2_CFG_AOI            (1 << 7)
#define LIS2DE12_INT2_CFG_6D             (1 << 6)
#define LIS2DE12_INT2_CFG_ZHIE           (1 << 5)
#define LIS2DE12_INT2_CFG_ZLIE           (1 << 4)
#define LIS2DE12_INT2_CFG_YHIE           (1 << 3)
#define LIS2DE12_INT2_CFG_YLIE           (1 << 2)
#define LIS2DE12_INT2_CFG_XHIE           (1 << 1)
#define LIS2DE12_INT2_CFG_XLIE                (1)

#define LIS2DE12_REG_INT2_SRC                0x35

#define LIS2DE12_REG_INT2_THS                0x36
#define LIS2DE12_INT2_THS                  (0x7f)

#define LIS2DE12_REG_INT2_DURATION           0x37

#define LIS2DE12_REG_CLICK_CFG               0x38
#define LIS2DE12_CLICK_CFG_ZD            (1 << 5)
#define LIS2DE12_CLICK_CFG_ZS            (1 << 4)
#define LIS2DE12_CLICK_CFG_YD            (1 << 3)
#define LIS2DE12_CLICK_CFG_YS            (1 << 2)
#define LIS2DE12_CLICK_CFG_XD            (1 << 1)
#define LIS2DE12_CLICK_CFG_XS                 (1)

#define LIS2DE12_REG_CLICK_SRC               0x39

#define LIS2DE12_REG_CLICK_THS               0x3A
#define LIS2DE12_CLICK_LIR               (1 << 7)
#define LIS2DE12_CLICK_THS                 (0x7f)

#define LIS2DE12_REG_TIME_LIMIT              0x3B
#define LIS2DE12_TIME_LIMIT_TLI            (0x7f)

#define LIS2DE12_REG_TIME_LATENCY            0x3C

#define LIS2DE12_REG_TIME_WINDOW             0x3D

#define LIS2DE12_REG_ACT_THS                 0x3E
#define LIS2DE12_ACT_THS_ACTH              (0x7f)

#define LIS2DE12_REG_ACT_DUR                 0x3F

#define LIS2DE12_SPI_READ_CMD_BIT            0x80
#define LIS2DE12_SPI_ADDR_INC                0x40
#define LIS2DE12_I2C_ADDR_INC                0x80

#define LIS2DE12_ST_MAX                      1440
#define LIS2DE12_ST_MIN                        68

int lis2de12_writelen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload, uint8_t len);
int lis2de12_readlen(struct sensor_itf *itf, uint8_t addr, uint8_t *payload, uint8_t len);
int lis2de12_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);
int lis2de12_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);
int lis2de12_get_data(struct sensor_itf *itf, uint8_t fs, int16_t *x, int16_t *y, int16_t *z);
int lis2de12_get_fs(struct sensor_itf *itf, uint8_t *fs);

#ifdef __cplusplus
}
#endif

#endif /* __LIS2DE12_PRIV_H_ */
