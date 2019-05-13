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

#ifndef __KXTJ3_PRIV_H__
#define __KXTJ3_PRIV_H__

#ifdef __cplusplus
#extern "C" {
#endif

/** KXTJ3 registers **/
/* output register x */
#define KXTJ3_XOUT_L 0x06
#define KXTJ3_XOUT_H 0x07
/* output register y */
#define KXTJ3_YOUT_L 0x08
#define KXTJ3_YOUT_H 0x09
/* output register z */
#define KXTJ3_ZOUT_L 0x0A
#define KXTJ3_ZOUT_H 0x0B
/* This register can be used to verify proper integrated circuit functionality */
#define KXTJ3_DCST_RESP 0x0C
/* This register can be used for supplier recognition, as it can be factory written to a known byte value. */
#define KXTJ3_WHO_AM_I 0x0F
/* This register reports which function caused an interrupt. */
#define KXTJ3_INT_SOURCE1 0x16
/* This register reports the axis and direction of detected motion */
#define KXTJ3_INT_SOURCE2 0x17
/* This register reports the status of the interrupt */
#define KXTJ3_STATUS_REG 0x18
#define KXTJ3_INT_REL 0x1A
/* Read/write control register that controls the main feature set */
#define KXTJ3_CTRL_REG1 0x1B
/* Read/write control register that provides more feature set control */
#define KXTJ3_CTRL_REG2 0x1D
/* This register controls the settings for the physical interrupt pin */
#define KXTJ3_INT_CTRL_REG1 0x1E
/* This register controls which axis and direction of detected motion can cause an interrupt */
#define KXTJ3_INT_CTRL_REG2 0x1F
/* Read/write control register that configures the acceleration outputs */
#define KXTJ3_DATA_CTRL_REG 0x21
#define KXTJ3_WAKEUP_COUNTER 0x29
#define KXTJ3_NA_COUNTER 0x2A
/* When 0xCA is written to this register, the MEMS self-test function is enabled */
#define KXTJ3_SELF_TEST 0x3A
#define KXTJ3_WAKEUP_THRESHOLD_H 0x6A
#define KXTJ3_WAKEUP_THRESHOLD_L 0x6B

/** KXTJ3 registers bits **/
/* before set */
#define KXTJ3_DCST_RESP_DCSTR_BEFORE (0x55 << 0)
/* after set */
#define KXTJ3_DCST_RESP_DCSTR_AFTER (0xAA << 0)
/* WHO_AM_I -value for KXTJ3 */
#define KXTJ3_WHO_AM_I_WIA_ID (0x35 << 0)
/* indicates that new acceleration data */
#define KXTJ3_INT_SOURCE1_DRDY (0x01 << 4)
/* Wake up */
#define KXTJ3_INT_SOURCE1_WUFS (0x01 << 1)
/* x- */
#define KXTJ3_INT_SOURCE2_XNWU (0x01 << 5)
/* x */
#define KXTJ3_INT_SOURCE2_XPWU (0x01 << 4)
/* y- */
#define KXTJ3_INT_SOURCE2_YNWU (0x01 << 3)
/* y */
#define KXTJ3_INT_SOURCE2_YPWU (0x01 << 2)
/* z- */
#define KXTJ3_INT_SOURCE2_ZNWU (0x01 << 1)
/* z */
#define KXTJ3_INT_SOURCE2_ZPWU (0x01 << 0)
/* reports the combined (OR) interrupt information of DRDY and WUFS in the interrupt source register */
#define KXTJ3_STATUS_REG_INT (0x01 << 4)
/* controls the operating mode of the KXTJ3 */
#define KXTJ3_CTRL_REG1_PC (0x01 << 7)
/* determines the performance mode of the KXTJ3 */
#define KXTJ3_CTRL_REG1_RES (0x01 << 6)
/* enables the reporting of the availability of new acceleration data as an interrupt */
#define KXTJ3_CTRL_REG1_DRDYE (0x01 << 5)
/* 2g range */
#define KXTJ3_CTRL_REG1_GSEL_2G (0x00 << 2)
/* 16g range */
#define KXTJ3_CTRL_REG1_GSEL_16G (0x01 << 2)
/* 4g range */
#define KXTJ3_CTRL_REG1_GSEL_4G (0x02 << 2)
/* 16g range */
#define KXTJ3_CTRL_REG1_GSEL_16G2 (0x03 << 2)
/* 8g range */
#define KXTJ3_CTRL_REG1_GSEL_8G (0x04 << 2)
/* 16g range */
#define KXTJ3_CTRL_REG1_GSEL_16G3 (0x05 << 2)
/* 8g range with 14b resolution */
#define KXTJ3_CTRL_REG1_GSEL_8G_14 (0x06 << 2)
/* 16g range with 14b resolution */
#define KXTJ3_CTRL_REG1_GSEL_16G_14 (0x07 << 2)
/* enables 14-bit mode if GSEL = '11' */
#define KXTJ3_CTRL_REG1_EN16G (0x01 << 2)
/* enables the Wake Up (motion detect) function. */
#define KXTJ3_CTRL_REG1_WUFE (0x01 << 1)
/* initiates software reset */
#define KXTJ3_CTRL_REG2_SRST (0x01 << 7)
/* initiates the digital communication self-test function. */
#define KXTJ3_CTRL_REG2_DCST (0x01 << 4)
/* 0.78Hz */
#define KXTJ3_CTRL_REG2_OWUF_0P781 (0x00 << 0)
/* 1.563Hz */
#define KXTJ3_CTRL_REG2_OWUF_1P563 (0x01 << 0)
/* 3.125Hz */
#define KXTJ3_CTRL_REG2_OWUF_3P125 (0x02 << 0)
/* 6.25Hz */
#define KXTJ3_CTRL_REG2_OWUF_6P25 (0x03 << 0)
/* 12.5Hz */
#define KXTJ3_CTRL_REG2_OWUF_12P5 (0x04 << 0)
/* 25Hz */
#define KXTJ3_CTRL_REG2_OWUF_25 (0x05 << 0)
/* 50Hz */
#define KXTJ3_CTRL_REG2_OWUF_50 (0x06 << 0)
/* 100Hz */
#define KXTJ3_CTRL_REG2_OWUF_100 (0x07 << 0)
/* enables/disables the physical interrupt pin */
#define KXTJ3_INT_CTRL_REG1_IEN (0x01 << 5)
/* sets the polarity of the physical interrupt pin */
#define KXTJ3_INT_CTRL_REG1_IEA (0x01 << 4)
/* sets the response of the physical interrupt pin */
#define KXTJ3_INT_CTRL_REG1_IEL (0x01 << 3)
/* selftest polarity */
#define KXTJ3_INT_CTRL_REG1_STPOL (0x01 << 1)
/* Unlatched mode motion  interrupt; 0=disabled,1=enabled */
#define KXTJ3_INT_CTRL_REG2_ULMODE (0x01 << 7)
/* x- */
#define KXTJ3_INT_CTRL_REG2_XNWU (0x01 << 5)
/* x */
#define KXTJ3_INT_CTRL_REG2_XPWU (0x01 << 4)
/* y- */
#define KXTJ3_INT_CTRL_REG2_YNWU (0x01 << 3)
/* y */
#define KXTJ3_INT_CTRL_REG2_YPWU (0x01 << 2)
/* z- */
#define KXTJ3_INT_CTRL_REG2_ZNWU (0x01 << 1)
/* z */
#define KXTJ3_INT_CTRL_REG2_ZPWU (0x01 << 0)
/* 12.5Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_12P5 (0x00 << 0)
/* 25Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_25 (0x01 << 0)
/* 50Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_50 (0x02 << 0)
/* 100Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_100 (0x03 << 0)
/* 200Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_200 (0x04 << 0)
/* 400Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_400 (0x05 << 0)
/* 800Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_800 (0x06 << 0)
/* 1600Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_1600 (0x07 << 0)
/* 0.78Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_0P781 (0x08 << 0)
/* 1.563Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_1P563 (0x09 << 0)
/* 3.125Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_3P125 (0x0A << 0)
/* 6.25Hz */
#define KXTJ3_DATA_CTRL_REG_OSA_6P25 (0x0B << 0)
/* charge on */
#define KXTJ3_SELF_TEST_MEMS_TEST_ENABLE (0xCA << 0)
/* charge off */
#define KXTJ3_SELF_TEST_MEMS_TEST_DISABLE (0x00 << 0)

/** KXTJ3 registers bit masks **/
#define KXTJ3_DCST_RESP_DCSTR_MASK 0xFF

#define KXTJ3_WHO_AM_I_WIA_MASK 0xFF
/* selects the acceleration range of the accelerometer outputs */
#define KXTJ3_CTRL_REG1_GSEL_MASK 0x1C
/* sets the Output Data Rate for the Wake Up function */
#define KXTJ3_CTRL_REG2_OWUF_MASK 0x07
/* sets the output data rate (ODR) */
#define KXTJ3_DATA_CTRL_REG_OSA_MASK 0x0F

#define KXTJ3_SELF_TEST_MEMS_TEST_MASK 0xFF

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
kxtj3_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);

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
kxtj3_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);

int
kxtj3_wait_for_wakeup(struct sensor *sensor);

#ifdef __cplusplus
}
#endif

#endif /* __KXTJ3_PRIV_H__ */

