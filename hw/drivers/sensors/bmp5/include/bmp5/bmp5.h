/**
 * Copyright (c) 2017-2025 Bosch Sensortec GmbH. All rights reserved.
 *
 * BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file       bmp5.h
 * @date       2025-08-13
 * @version    v1.0.0
 *
 */

/**
 * @defgroup bmp5 BMP5
 * @brief Sensor driver for BMP5 sensor
 */

#ifndef __BMP5_H__
#define __BMP5_H__

/******************************************************************************
 * Header files                                                               *
 *****************************************************************************/
#include "os/mynewt.h"
#include <stats/stats.h>
#include "sensor/sensor.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#endif

/* CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/* BMP580 Identifier */
#define BMP581_BMP580_CHIP_ID 0x50

/* BMP585 chip identifier */
#define BMP585_CHIP_ID 0x51

/* FIFO related macros */
/* FIFO enable  */
#define BMP5_ENABLE  0x01
#define BMP5_DISABLE 0x00

/* Sensor component selection macros
These values are internal for API implementation. Don't relate this to
data sheet.*/
#define BMP5_PRESS (uint8_t)(1)
#define BMP5_TEMP  (uint8_t)(1 << 1)
#define BMP5_ALL   (uint8_t)(0x03)

/* Power mode macros */
#define BMP5_STANDBY_MODE      (uint8_t)(0x00)
#define BMP5_NORMAL_MODE       (uint8_t)(0x01)
#define BMP5_FORCED_MODE       (uint8_t)(0x02)
#define BMP5_CONTINUOUS_MODE   (uint8_t)(0x03)
#define BMP5_DEEP_STANDBY_MODE (uint8_t)(0x04)

/* Status macros */
#define BMP5_CORE_RDY    (uint8_t)(0x01)
#define BMP5_NVM_RDY     (uint8_t)(0x02)
#define BMP5_NVM_ERR     (uint8_t)(0x04)
#define BMP5_NVM_CMD_ERR (uint8_t)(0x08)

#define BMP5_ODR_FILTER                                                       \
    (uint32_t)(BMP5_PRESS_OS_SEL | BMP5_TEMP_OS_SEL | BMP5_DSP_IIR_T_SEL |    \
               BMP5_DSP_IIR_P_SEL | BMP5_ODR_SEL)
/* FIFO Selection config settings */
#define BMP5_FIFO_SEL_CONFIG                                                  \
    (uint32_t)(BMP5_FIFO_PRESS_EN_SEL | BMP5_FIFO_TEMP_EN_SEL | BMP5_FIFO_DECIMENT_SEL)
/* Interrupt control settings */
#define BMP5_INT_CONFIG                                                       \
    (uint32_t)(BMP5_INT_OD | BMP5_INT_POL | BMP5_INT_MODE |                   \
               BMP5_INT_DRDY_EN | BMP5_INT_PAD_DRV)
/* Advance settings */
#define BMP5_DRIVE_CONFIG                                                     \
    (uint32_t)(BMP5_DRV_CNF_I2C_CSB_PULL_UP_EN | BMP5_DRV_CNF_SPI3_MODE_EN |  \
               BMP5_DRV_CNF_PAD_IF_DRV)
/* Compensation settings */
#define BMP5_COMPENSATE (uint32_t)(BMP5_PRESS_COMP_EN | BMP5_TEMP_COMP_EN)

/* FIFO settings */
/* Mask for fifo_mode, fifo_stop_on_full, fifo_press_en and
 * fifo_temp_en settings
 */
/* Mask for fths_en and ffull_en settings */
#define FIFO_INT_CONFIG (uint16_t)(0x030)

/* Macros related to size */
#define BMP5_P_T_DATA_LEN     (uint8_t)(6)
#define BMP5_P_DATA_LEN       (uint8_t)(3)
#define BMP5_T_DATA_LEN       (uint8_t)(3)
#define BMP5_P_AND_T_DATA_LEN (uint8_t)(6)
#define BMP5_P_OR_T_DATA_LEN  (uint8_t)(3)
#define BMP5_FIFO_MAX_FRAMES  (uint8_t)(73)

/* Register Address */
#define BMP5_CHIP_ID_ADDR            (uint8_t)(0x01)
#define BMP5_ASIC_REV_ID_ADDR        (uint8_t)(0x02)
#define BMP5_CHIP_STATUS_ADDR        (uint8_t)(0x11)
#define BMP5_DRIVE_CONF_ADDR         (uint8_t)(0x13)
#define BMP5_INT_CONFIG_ADDR         (uint8_t)(0x14)
#define BMP5_INT_SRC_ADDR            (uint8_t)(0x15)
#define BMP5_FIFO_CONFIG_ADDR        (uint8_t)(0x16)
#define BMP5_FIFO_COUNT_ADDR         (uint8_t)(0x17)
#define BMP5_FIFO_SEL_ADDR           (uint8_t)(0x18)
#define BMP5_DATA_ADDR               (uint8_t)(0x1D)
#define BMP5_INT_STATUS_REG_ADDR     (uint8_t)(0x27)
#define BMP5_STATUS_REG_ADDR         (uint8_t)(0x28)
#define BMP5_FIFO_DATA_ADDR          (uint8_t)(0x29)
#define BMP5_NVM_ADDR                (uint8_t)(0x2B)
#define BMP5_NVM_DATA_ADDR           (uint8_t)(0x2C)
#define BMP5_DSP_CONFIG_ADDR         (uint8_t)(0x30)
#define BMP5_DSP_IIR_ADDR            (uint8_t)(0x31)
#define BMP5_OOR_THR_PRESSURE_ADDR   (uint8_t)(0x32)
#define BMP5_OOR_RANGE_PRESSURE_ADDR (uint8_t)(0x34)
#define BMP5_OOR_CONFIG_ADDR         (uint8_t)(0x35)
#define BMP5_OSR_CONFIG_ADDR         (uint8_t)(0x36)
#define BMP5_ODR_CONFIG_ADDR         (uint8_t)(0x37)
#define BMP5_OSR_EFF_ADDR            (uint8_t)(0x38)
#define BMP5_CMD_ADDR                (uint8_t)(0x7E)

/* FIFO downsampling macros */
#define BMP5_FIFO_NO_DOWNSAMPLING   (uint8_t)(0x00)
#define BMP5_FIFO_DOWNSAMPLING_2X   (uint8_t)(0x01)
#define BMP5_FIFO_DOWNSAMPLING_4X   (uint8_t)(0x02)
#define BMP5_FIFO_DOWNSAMPLING_8X   (uint8_t)(0x03)
#define BMP5_FIFO_DOWNSAMPLING_16X  (uint8_t)(0x04)
#define BMP5_FIFO_DOWNSAMPLING_32X  (uint8_t)(0x05)
#define BMP5_FIFO_DOWNSAMPLING_64X  (uint8_t)(0x06)
#define BMP5_FIFO_DOWNSAMPLING_128X (uint8_t)(0x07)

/* Over sampling macros */
#define BMP5_NO_OVERSAMPLING   0x00
#define BMP5_OVERSAMPLING_2X   0x01
#define BMP5_OVERSAMPLING_4X   0x02
#define BMP5_OVERSAMPLING_8X   0x03
#define BMP5_OVERSAMPLING_16X  0x04
#define BMP5_OVERSAMPLING_32X  0x05
#define BMP5_OVERSAMPLING_64X  0x06
#define BMP5_OVERSAMPLING_128X 0x07

/* Odr setting macros */
#define BMP5_ODR_240_HZ     0x00
#define BMP5_ODR_218_537HZ  0x01
#define BMP5_ODR_199_111HZ  0x02
#define BMP5_ODR_179_200HZ  0x03
#define BMP5_ODR_160_000_HZ 0x04
#define BMP5_ODR_149_333_HZ 0x05
#define BMP5_ODR_140_000_HZ 0x06
#define BMP5_ODR_129_855_HZ 0x07
#define BMP5_ODR_120_000_HZ 0x08
#define BMP5_ODR_110_164_HZ 0x09
#define BMP5_ODR_100_299_HZ 0x0A
#define BMP5_ODR_89_600_HZ  0x0B
#define BMP5_ODR_80_000_HZ  0x0C
#define BMP5_ODR_70_000_HZ  0x0D
#define BMP5_ODR_60_000_HZ  0x0E
#define BMP5_ODR_50_056_HZ  0x0F
#define BMP5_ODR_45_025_HZ  0x10
#define BMP5_ODR_40_000_HZ  0x11
#define BMP5_ODR_35_000_HZ  0x12
#define BMP5_ODR_30_000_HZ  0x13
#define BMP5_ODR_25_005_HZ  0x14
#define BMP5_ODR_20_000_HZ  0x15
#define BMP5_ODR_15_000_HZ  0x16
#define BMP5_ODR_10_000_HZ  0x17
#define BMP5_ODR_5_000_HZ   0x18
#define BMP5_ODR_4_000_HZ   0x19
#define BMP5_ODR_3_000_HZ   0x1A
#define BMP5_ODR_2_000_HZ   0x1B
#define BMP5_ODR_1_000_HZ   0x1C
#define BMP5_ODR_0_500_HZ   0x1D
#define BMP5_ODR_0_250_HZ   0x1E
#define BMP5_ODR_0_125_HZ   0x1F

/* Macros to select the which sensor settings are to be set by the user.
 * These values are internal for API implementation. Don't relate this to
 * data sheet
 */
#define BMP5_PRESS_EN_SEL                (uint32_t)(1UL << 1)
#define BMP5_TEMP_EN_SEL                 (uint32_t)(1UL << 2)
#define BMP5_PRESS_OS_SEL                (uint32_t)(1UL << 3)
#define BMP5_TEMP_OS_SEL                 (uint32_t)(1UL << 4)
#define BMP5_DSP_IIR_T_SEL               (uint32_t)(1UL << 5)
#define BMP5_DSP_IIR_P_SEL               (uint32_t)(1UL << 6)
#define BMP5_ODR_SEL                     (uint32_t)(1UL << 7)
#define BMP5_INT_DRDY_EN                 (uint32_t)(1UL << 8)
#define BMP5_INT_OD                      (uint32_t)(1UL << 9)
#define BMP5_INT_POL                     (uint32_t)(1UL << 10)
#define BMP5_INT_MODE                    (uint32_t)(1UL << 11)
#define BMP5_INT_PAD_DRV                 (uint32_t)(1UL << 12)
#define BMP5_DRV_CNF_I2C_CSB_PULL_UP_EN  (uint32_t)(1UL << 13)
#define BMP5_DRV_CNF_SPI3_MODE_EN        (uint32_t)(1UL << 14)
#define BMP5_DRV_CNF_PAD_IF_DRV          (uint32_t)(1UL << 15)
#define BMP5_PRESS_COMP_EN               (uint32_t)(1UL << 16)
#define BMP5_TEMP_COMP_EN                (uint32_t)(1UL << 17)
#define BMP5_DSP_CNF_IIR_FLUSH_FORCED_EN (uint32_t)(1UL << 18)
#define BMP5_DSP_CNF_IIR_SHADOW_SEL_T    (uint32_t)(1UL << 19)
#define BMP5_DSP_CNF_IIR_FIFO_SEL_T      (uint32_t)(1UL << 20)
#define BMP5_DSP_CNF_IIR_SHADOW_SEL_P    (uint32_t)(1UL << 21)
#define BMP5_DSP_CNF_IIR_FIFO_SEL_P      (uint32_t)(1UL << 22)
#define BMP5_DSP_CNF_OOR_SEL_IIR_P       (uint32_t)(1UL << 23)
#define BMP5_POWER_MODE_SEL              (uint32_t)(1UL << 24)
#define BMP5_ALL_SETTINGS                (uint32_t)(0xFFFFF)

/* Macros to select the which FIFO settings are to be set by the user
 * These values are internal for API implementation. Don't relate this to
 * data sheet
 */
#define BMP5_FIFO_MODE_SEL            (uint16_t)(1UL << 1)
#define BMP5_FIFO_STOP_ON_FULL_EN_SEL (uint16_t)(1UL << 2)
#define BMP5_FIFO_PRESS_EN_SEL        (uint16_t)(1UL << 4)
#define BMP5_FIFO_TEMP_EN_SEL         (uint16_t)(1UL << 5)
#define BMP5_FIFO_DECIMENT_SEL        (uint16_t)(1UL << 6)
#define BMP5_FIFO_FILTER_EN_SEL       (uint16_t)(1UL << 7)
#define BMP5_FIFO_FTHS_EN_SEL         (uint16_t)(1UL << 8)
#define BMP5_FIFO_FFULL_EN_SEL        (uint16_t)(1UL << 9)
#define BMP5_FIFO_ALL_SETTINGS        (uint16_t)(0x3FF)

#define BMP5_STATUS_CORE_RDY_MSK (uint8_t)(0x01)

#define BMP5_STATUS_NVM_RDY_MSK (uint8_t)(0x02)
#define BMP5_STATUS_NVM_RDY_POS (uint8_t)(0x01)

#define BMP5_STATUS_NVM_ERR_MSK (uint8_t)(0x04)
#define BMP5_STATUS_NVM_ERR_POS (uint8_t)(0x02)

#define BMP5_INT_STATUS_DRDY_MSK (uint8_t)(0x01)

#define BMP5_INT_STATUS_FFULL_MSK (uint8_t)(0x02)
#define BMP5_INT_STATUS_FFULL_POS (uint8_t)(0x01)

#define BMP5_INT_STATUS_FTHS_MSK (uint8_t)(0x04)
#define BMP5_INT_STATUS_FTHS_POS (uint8_t)(0x02)

#define BMP5_INT_STATUS_OOR_P_MSK (uint8_t)(0x08)
#define BMP5_INT_STATUS_OOR_P_POS (uint8_t)(0x03)

#define BMP5_INT_STATUS_POR_MSK (uint8_t)(0x10)
#define BMP5_INT_STATUS_POR_POS (uint8_t)(0x04)

#define BMP5_ODR_CNF_POWER_MODE_MSK (uint8_t)(0x03)

#define BMP5_ODR_CNF_DEEP_DIS_MSK (uint8_t)(0x80)
#define BMP5_ODR_CNF_DEEP_DIS_POS (uint8_t)(0x07)

#define BMP5_OSR_CNF_PRESS_EN_MSK (uint8_t)(0x40)
#define BMP5_OSR_CNF_PRESS_EN_POS (uint8_t)(0x06)

#define BMP5_DSP_IIR_FILTER_T_MSK (uint8_t)(0x07)
#define BMP5_DSP_IIR_FILTER_P_MSK (uint8_t)(0x38)
#define BMP5_DSP_IIR_FILTER_P_POS (uint8_t)(0x03)

#define BMP5_ODR_MSK (uint8_t)(0x7C)
#define BMP5_ODR_POS (uint8_t)(0x02)

#define BMP5_DSP_CNF_IIR_FLUSH_FORCED_EN_MSK (uint8_t)(0x04)
#define BMP5_DSP_CNF_IIR_FLUSH_FORCED_EN_POS (uint8_t)(0x02)

#define BMP5_DSP_CNF_IIR_SHADOW_SEL_T_MSK (uint8_t)(0x08)
#define BMP5_DSP_CNF_IIR_SHADOW_SEL_T_POS (uint8_t)(0x03)

#define BMP5_DSP_CNF_IIR_FIFO_SEL_T_MSK (uint8_t)(0x10)
#define BMP5_DSP_CNF_IIR_FIFO_SEL_T_POS (uint8_t)(0x04)

#define BMP5_DSP_CNF_IIR_SHADOW_SEL_P_MSK (uint8_t)(0x20)
#define BMP5_DSP_CNF_IIR_SHADOW_SEL_P_POS (uint8_t)(0x05)

#define BMP5_DSP_CNF_IIR_FIFO_SEL_P_MSK (uint8_t)(0x40)
#define BMP5_DSP_CNF_IIR_FIFO_SEL_P_POS (uint8_t)(0x06)

#define BMP5_DSP_CNF_OOR_SEL_IIR_P_MSK (uint8_t)(0x80)
#define BMP5_DSP_CNF_OOR_SEL_IIR_P_POS (uint8_t)(0x07)

#define BMP5_DSP_CNF_T_COMP_EN_MSK (uint8_t)(0x01)

#define BMP5_DSP_CNF_P_COMP_EN_MSK (uint8_t)(0x02)
#define BMP5_DSP_CNF_P_COMP_EN_POS (uint8_t)(0x01)

#define BMP5_INT_OD_MSK (uint8_t)(0x01)

#define BMP5_INT_POL_MSK (uint8_t)(0x02)
#define BMP5_INT_POL_POS (uint8_t)(0x01)

#define BMP5_INT_MODE_MSK (uint8_t)(0x04)
#define BMP5_INT_MODE_POS (uint8_t)(0x02)

#define BMP5_INT_DRDY_MSK (uint8_t)(0x04)
#define BMP5_INT_DRDY_POS (uint8_t)(0x02)

#define BMP5_INT_PAD_DRV_MSK (uint8_t)(0xF0)
#define BMP5_INT_PAD_DRV_POS (uint8_t)(0x04)

#define BMP5_INT_DRDY_EN_MSK (uint8_t)(0x40)
#define BMP5_INT_DRDY_EN_POS (uint8_t)(0x06)

#define BMP5_DRV_CNF_PAD_IF_DRV_EN_MSK (uint8_t)(0xF0)
#define BMP5_DRV_CNF_PAD_IF_DRV_EN_POS (uint8_t)(0x07)

#define BMP5_DRV_CNF_SPI3_EN_MSK (uint8_t)(0x02)
#define BMP5_DRV_CNF_SPI3_EN_POS (uint8_t)(0x01)

#define BMP5_DRV_CNF_I2C_CSB_PUP_EN_MSK (uint8_t)(0x01)
#define BMP5_DRV_CNF_I2C_CSB_PUP_EN_POS (uint8_t)(0x00)

#define BMP5_FIFO_THS_MSK (uint8_t)(0x1F)

#define BMP5_FIFO_MODE_MSK (uint8_t)(0x20)
#define BMP5_FIFO_MODE_POS (uint8_t)(0x05)

#define BMP5_TEMP_OS_MSK (uint8_t)(0x07)

#define BMP5_PRESS_OS_MSK (uint8_t)(0x38)
#define BMP5_PRESS_OS_POS (uint8_t)(0x03)

#define BMP5_FIFO_FRAME_SEL_TEMP_EN_MSK (uint8_t)(0x01)

#define BMP5_FIFO_FRAME_SEL_PRESS_EN_MSK (uint8_t)(0x04)
#define BMP5_FIFO_FRAME_SEL_PRESS_EN_POS (uint8_t)(0x02)

/* Downsampling settings */
#define BMP5_FIFO_DECIMENT_SEL_MSK (uint8_t)(0x1c)
#define BMP5_FIFO_DECIMENT_SEL_POS (uint8_t)(0x04)

#define BMP5_OSR_EFF_TEMP_MSK (uint8_t)(0x07)

#define BMP5_OSR_EFF_PRESS_MSK (uint8_t)(0x38)
#define BMP5_OSR_EFF_PRESS_POS (uint8_t)(0x03)

#define BMP5_OSR_EFF_ODR_IS_VALID_MSK (uint8_t)(0x80)
#define BMP5_OSR_EFF_ODR_IS_VALID_POS (uint8_t)(0x07)

/*    UTILITY MACROS  */
#define BMP5_SET_LOW_BYTE  (uint16_t)(0x00FF)
#define BMP5_SET_HIGH_BYTE (uint16_t)(0xFF00)

/* Macro to combine two 8 bit data's to form a 16 bit data */
#define BMP5_CONCAT_BYTES(msb, lsb) (((uint16_t)msb << 8) | (uint16_t)lsb)

#define BMP5_SET_BITS(reg_data, bitname, data)                                \
    ((reg_data & ~(bitname##_MSK)) | ((data << bitname##_POS) & bitname##_MSK))
/* Macro variant to handle the bitname position if it is zero */
#define BMP5_SET_BITS_POS_0(reg_data, bitname, data)                          \
    ((reg_data & ~(bitname##_MSK)) | (data & bitname##_MSK))

#define BMP5_GET_BITS(reg_data, bitname)                                      \
    ((reg_data & (bitname##_MSK)) >> (bitname##_POS))
/* Macro variant to handle the bitname position if it is zero */
#define BMP5_GET_BITS_POS_0(reg_data, bitname) (reg_data & (bitname##_MSK))

#define BMP5_GET_LSB(var) (uint8_t)(var & BMP5_SET_LOW_BYTE)
#define BMP5_GET_MSB(var) (uint8_t)((var & BMP5_SET_HIGH_BYTE) >> 8)

#define BMP5_INT_DRDY_STATE     0x00
#define BMP5_INT_FIFOFULL_STATE 0x02
#define BMP5_INT_FIFOTHS_STATE  0x04
#define BMP5_INT_OOR_P_STATE    0x08

#define BMP5_INT_CFG_FIFOTHS  BMP5_ENABLE
#define BMP5_INT_CFG_FIFOFULL BMP5_ENABLE
#define BMP5_INT_CFG_DRDY     BMP5_ENABLE

#define BMP5_DEEP_ENABLED  0x00
#define BMP5_DEEP_DISABLED 0x01

enum bmp5_fifo_mode {
    BMP5_FIFO_M_BYPASS = 0,
    BMP5_FIFO_M_FIFO = 1,
    BMP5_FIFO_M_CONTINUOUS_TO_FIFO = 3,
    BMP5_FIFO_M_BYPASS_TO_CONTINUOUS = 4,
    BMP5_FIFO_M_CONTINUOUS = 6
};

enum bmp5_int_type {
    BMP5_DRDY_INT = 1,
    BMP5_FIFO_THS_INT = 2,
    BMP5_FIFO_FULL_INT = 3,
};

enum bmp5_read_mode {
    BMP5_READ_M_POLL = 0,
    BMP5_READ_M_STREAM = 1,
    BMP5_READ_M_HYBRID = 2,
};

/* Read mode configuration */
struct bmp5_read_mode_cfg {
    enum bmp5_read_mode mode;
    uint8_t int_num : 1;
    uint8_t int_type;
};

struct bmp5_cfg {
    uint8_t rate;

    /* Read mode config */
    struct bmp5_read_mode_cfg read_mode;

    uint8_t filter_press_osr;
    uint8_t filter_temp_osr;

    /* interrupt config */
    uint8_t int_enable_type : 2;
    uint8_t int_pp_od : 1;
    uint8_t int_mode : 1;
    uint8_t int_active_pol : 1;

    /* Power mode */
    uint8_t power_mode : 4;

    /* fifo  config */
    enum bmp5_fifo_mode fifo_mode;
    uint8_t fifo_threshold;

    /* Sensor type mask to track enabled sensors */
    sensor_type_t mask;
};

/* Used to track interrupt state to wake any present waiters */
struct bmp5_int {
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
struct bmp5_pdd {
    /* Notification event context */
    struct sensor_notify_ev_ctx notify_ctx;
    /* Inetrrupt state */
    struct bmp5_int *interrupt;
    /* Interrupt enabled flag */
    uint16_t int_enable;
};

/* Define the stats section and records */
STATS_SECT_START(bmp5_stat_section)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(read_errors)
STATS_SECT_END

/********************************************************/

/**
 * Interface selection Enums
 */
enum bmp5_intf {
    /* SPI interface */
    BMP5_SPI_INTF,
    /* I2C interface */
    BMP5_I2C_INTF
};

/**
 * bmp5 sensor data which comprises temperature and pressure
 * data.
 */
struct bmp5_data {
    /* Compensated temperature in 0.01 degC*/
    int16_t temperature;
    /* Compensated pressure in 0.01 Pa */
    uint32_t pressure;
};

/********************************************************/

/**
 * bmp5 advance settings
 */
struct bmp5_adv_settings {
    /* i2c CSB pull up enable */
    uint8_t i2c_csb_pull_up_en;
    /* SPI3 mode enable */
    uint8_t spi3_mode_en;
    /* Pad drive strength for serial IO pins SDZ/SDO */
    uint8_t pad_if_drv;
    /* IIR flushed forced enable */
    uint8_t iir_flush_forced_en : 1;
    /* Shadow sel IIR Temperature */
    uint8_t iir_shadow_sel_t : 1;
    /* FIFO selected IIR Temperature */
    uint8_t fifo_sel_iir_t : 1;
    /* Shadow sel IIR Pressure */
    uint8_t iir_shadow_sel_p : 1;
    /* FIFO selected IIR Pressure */
    uint8_t fifo_sel_iir_p : 1;
    /* OOR Sel IIR Pressure */
    uint8_t oor_sel_iir_p : 1;
};

/**
 * bmp5 odr and filter settings
 */
struct bmp5_odr_filter_settings {
    /* Pressure oversampling */
    uint8_t press_os;
    /* Temperature oversampling */
    uint8_t temp_os;
    /* IIR filter temperature */
    uint8_t iir_filter_t;
    /* IIR filter pressure */
    uint8_t iir_filter_p;
    /* Output data rate */
    uint8_t odr;
};

/**
 * bmp5 interrupt pin settings
 */
struct bmp5_int_config_settings {
    /* Output mode */
    uint8_t od;
    /* Active high/low */
    uint8_t pol;
    /* Latched or Non-latched */
    uint8_t mode;
    /* Pad drive strength for interrupt pad */
    uint8_t pad_drv;
    /* Data ready interrupt */
    uint8_t drdy_en;
};

/**
 * bmp5 device settings
 */
struct bmp5_settings {
    /* Compensation */
    uint8_t press_comp_en : 1;
    uint8_t temp_comp_en : 1;
    /* Enable/Disable pressure sensor */
    uint8_t press_en : 1;
    /* Enable/Disable temperature sensor */
    uint8_t temp_en : 1;
    /* Power mode which user wants to set */
    uint8_t pwr_mode : 3;
    /* Reserved */
    uint8_t reserved : 1;
    /* ODR and filter configuration */
    struct bmp5_odr_filter_settings odr_filter;
    /* Interrupt configuration */
    struct bmp5_int_config_settings int_settings;
    /* Advance settings */
    struct bmp5_adv_settings adv_settings;
};

/**
 * bmp5 fifo frame
 */
struct bmp5_fifo_data {
    /* Data buffer of user defined length is to be mapped here
        512 + 4 + 7 * 3 */
    uint8_t buffer[540];
    /* Number of bytes of data read from the fifo */
    uint16_t byte_count;
    /* Number of frames to be read as specified by the user */
    uint8_t req_frames;
    /* Will be equal to length when no more frames are there to parse */
    uint16_t start_idx;
    /* Will contain the no of parsed data frames from fifo */
    uint8_t parsed_frames;
    /* Configuration error */
    uint8_t config_err;
    /* FIFO input configuration change */
    uint8_t config_change;
    /* All available frames are parsed */
    uint8_t frame_not_available;
};

/**
 * bmp5 fifo configuration
 */
struct bmp5_fifo_settings {
    /* enable/disable */
    uint8_t mode;
    /* stop on full enable/disable */
    uint8_t stop_on_full_en;
    /* pressure enable/disable */
    uint8_t press_en;
    /* temperature enable/disable */
    uint8_t temp_en;
    /* deciment selection/down sampling rate */
    uint8_t dec_sel;
    /* filter enable/disable */
    uint8_t filter_en;
    /* FIFO threshold enable/disable */
    uint8_t fths_en;
    /* FIFO full enable/disable */
    uint8_t ffull_en;
};

/**
 * bmp5 bmp5 FIFO
 */
struct bmp5_fifo {
    /* FIFO frame structure */
    struct bmp5_fifo_data data;
    /* FIFO config structure */
    struct bmp5_fifo_settings settings;
    bool no_need_sensortime;
    bool sensortime_updated;
};

/**
 * bmp5 sensor status flags
 */
struct bmp5_sens_status {
    /* NVM ready status */
    uint8_t nvm_rdy;
    /* NVM error status */
    uint8_t nvm_err;
};

/**
 * bmp5 interrupt status flags
 */
struct bmp5_int_status {
    /* fifo threshold interrupt */
    uint8_t fifo_ths;
    /* fifo full interrupt */
    uint8_t fifo_full;
    /* data ready interrupt */
    uint8_t drdy;
    /* out of range pressure interrupt */
    uint8_t oor_p;
};

/**
 * bmp5 error status flags
 */
struct bmp5_err_status {
    /* fatal error */
    uint8_t fatal;
    /* command error */
    uint8_t cmd;
    /* configuration error */
    uint8_t conf;
};

/**
 * bmp5 status flags
 */
struct bmp5_status {
    /* Interrupt status */
    struct bmp5_int_status intr;
    /* Sensor status */
    struct bmp5_sens_status sensor;
    /* Error status */
    struct bmp5_err_status err;
    /* power on reset status */
    uint8_t pwr_on_rst;
};

/**
 * bmp5 device structure
 */
struct bmp5_dev {
    /* Chip Id */
    uint8_t chip_id;
    /* Device Id */
    uint8_t dev_id;
    /* SPI/I2C interface */
    enum bmp5_intf intf;
    /* Decide SPI or I2C read mechanism */
    uint8_t dummy_byte;
    /* Sensor Settings */
    struct bmp5_settings settings;
    /* Sensor and interrupt status flags */
    struct bmp5_status status;
    /* FIFO data and settings structure */
    struct bmp5_fifo *fifo;
    /* fifo water marklevel */
    uint8_t fifo_threshold_level;
};

struct bmp5 {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_i2c_node i2c_node;
#else
    struct os_dev dev;
#endif
    struct sensor sensor;
    struct bmp5_cfg cfg;
    struct bmp5_int intr;
    struct bmp5_pdd pdd;
    struct bmp5_dev bmp5_dev;
    /* Variable used to hold stats data */
    STATS_SECT_DECL(bmp5_stat_section) stats;
    bool bmp5_cfg_complete;
};

/**
 * Set bmp5 to normal mode
 *
 * @param bmp5 The device
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_normal_mode(struct bmp5 *bmp5);

/**
 * Set bmp5 to force mode with OSR
 *
 * @param bmp5 The device
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_forced_mode_with_osr(struct bmp5 *bmp5);

/**
 *  @brief This API is the entry point.
 *  It performs the selection of I2C/SPI read mechanism according to the
 *  selected interface and reads the chip-id and calibration data of the sensor.
 */
int bmp5_itf_init(struct bmp5 *dev);

int bmp5_get_sensor_data(struct bmp5 *bmp5, struct bmp5_data *sensor_data);

/**
 * @brief This API sets the power control(pressure enable and
 * temperature enable), over sampling, odr and filter
 * settings in the sensor.
 *
 * @param dev : Structure instance of bmp5_dev.
 * @param desired_settings : Variable used to select the settings which
 * are to be set in the sensor.
 *
 * @note : Below are the macros to be used by the user for selecting the
 * desired settings. User can do OR operation of these macros for configuring
 * multiple settings.
 *
 * Macros                   |   Functionality
 * -------------------------|----------------------------------------------
 * BMP5_PRESS_EN_SEL        |   Enable/Disable pressure.
 * BMP5_TEMP_EN_SEL         |   Enable/Disable temperature.
 * BMP5_PRESS_COMP_EN       |   Enable/Disable pressure compensation.
 * BMP5_TEMP_COMP_EN        |   Enable/Disable temperature compensation.
 * BMP5_PRESS_OS_SEL        |   Set pressure oversampling.
 * BMP5_TEMP_OS_SEL         |   Set temperature oversampling.
 * BMP5_IIR_FILTER_SEL      |   Set IIR filter.
 * BMP5_ODR_SEL             |   Set ODR.
 * BMP5_INT_OD              |   Set interrupt pin either to be open drain or
 *                          |   push pull
 * BMP5_INT_POL             |   Set interrupt pad to be active high or low
 * BMP5_INT_MODE            |   Set interrupt pad to be latched or nonlatched.
 * BMP5_INT_PAD_DRV         |   Set interrupt pad drive strength for INT (should
 *                          |   be set in INT open drain config only.)
 * BMP5_INT_DRDY_EN         |   Map/Unmap the drdy interrupt to interrupt pad.
 * BMP5_I2C_CSB_PULL_UP_EN  |   Enable/Disable I2C CSB pull up.
 * BMP5_SPI3_MODE_EN        |   Enable/Disable SPI3 mode.
 * BMP5_PAD_IF_DRV          |   Set the pad drive strength for serial IO pins SDZ/SDO.
 * BMP5_POWER_MODE_SEL      |   Set the power mode of the sensor.
 *
 * @return 0 on success, +ve value on warning, -ve value on error.
 */
int bmp5_set_sensor_settings(uint32_t desired_settings, struct bmp5_dev *dev);

/**
 * Get chip ID
 *
 * @param bmp5 The device
 * @param chip_id Ptr to chip id to be filled up
 */
int bmp5_get_chip_id(struct bmp5 *bmp5, uint8_t *chip_id);

/**
 * Dump the registers
 *
 * @param bmp5 The device
 */
int bmp5_dump(struct bmp5 *bmp5);

/**
 * Sets the rate
 *
 * @param bmp5 The device
 * @param rate The sampling rate of the sensor
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_rate(struct bmp5 *bmp5, uint8_t rate);

/**
 * Sets the power mode of the sensor
 *
 * @param bmp5 The device
 * @param mode Power mode
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_power_mode(struct bmp5 *bmp5, uint8_t mode);

/**
 * Sets the interrupt push-pull/open-drain selection
 *
 * @param bmp5 The device
 * @param mode Interrupt setting (0 = push-pull, 1 = open-drain)
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_int_pp_od(struct bmp5 *bmp5, uint8_t mode);

/**
 * Sets whether latched interrupts are enabled
 *
 * @param bmp5 The device
 *  @param latched Value to set (0 = not latched, 1 = latched)
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_int_mode(struct bmp5 *bmp5, uint8_t latched);

/**
 * Sets the interrupt push-pull/open-drain selection
 *
 * @param bmp5 The device
 * @param drv Drive strength
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_int_pad_drv(struct bmp5 *bmp5, uint8_t drv);

/**
 * Sets whether interrupts are active high or low
 *
 * @param bmp5 The device
 * @param pol Value to set (1 = active high, 0 = active low)
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_int_active_pol(struct bmp5 *bmp5, uint8_t pol);

/**
 * Set filter config
 *
 * @param bmp5 The device
 * @param press_osr Oversampling selection for pressure
 *                 (BMP5_NO_OVERSAMPLING..BMP5_OVERSAMPLING_32)
 * @param temp_osr Oversampling selection for temperature
 *                 (BMP5_NO_OVERSAMPLING..BMP5_OVERSAMPLING_32)
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_filter_cfg(struct bmp5 *bmp5, uint8_t press_osr, uint8_t temp_osr);

/**
 * Set whether interrupts are enabled
 *
 * @param bmp5 The device
 * @param enabled Value to set (0 = disabled, 1 = enabled)
 * @param int_type interrupt to enable/disable
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_int_enable(struct bmp5 *bmp5, uint8_t enabled, enum bmp5_int_type int_type);

/**
 * Clear interrupts
 *
 * @param bmp5 The device
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_clear_int(struct bmp5 *bmp5);

/**
 * Setup FIFO
 *
 * @param bmp5 The device
 * @param mode FIFO mode to setup
 * @param fifo_ths Threshold to set for FIFO
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_set_fifo_cfg(struct bmp5 *bmp5, enum bmp5_fifo_mode mode, uint8_t fifo_ths);

/**
 * Run Self test on sensor
 *
 * @param bmp5 The device
 * @param result Ptr to return test result in (0 on pass, non-zero on failure)
 *
 * @return 0 on sucess, non-zero on failure
 */
int bmp5_run_self_test(struct bmp5 *bmp5, int *result);

/**
 * @brief This internal API gets the effective OSR configuration and ODR valid
 * status from the sensor.
 *
 * @param dev Structure instance of bmp5_dev.
 * @param t_eff Pointer to store the effective temperature OSR.
 * @param p_eff Pointer to store the effective pressure OSR.
 * @return 0 on success, non-zero on failure.
 */
int bmp5_get_osr_eff(struct bmp5_dev *dev, uint32_t *t_eff, uint32_t *p_eff);

/**
 * Provide a continuous stream of pressure readings.
 *
 * @param sensor The sensor ptr
 * @param type The sensor type
 * @param read_func The function pointer to invoke for each accelerometer reading.
 * @param read_arg The opaque pointer that will be passed in to the function.
 * @param time_ms If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int bmp5_stream_read(struct sensor *sensor, sensor_type_t sensor_type,
                     sensor_data_func_t read_func, void *read_arg, uint32_t time_ms);

/**
 * Do pressure sensor polling reads
 *
 * @param sensor The sensor ptr
 * @param sensor_type The sensor type
 * @param data_func The function pointer to invoke for each accelerometer reading.
 * @param data_arg The opaque pointer that will be passed in to the function.
 * @param timeout If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int bmp5_poll_read(struct sensor *sensor, sensor_type_t sensor_type,
                   sensor_data_func_t data_func, void *data_arg, uint32_t timeout);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param dev Ptr to the device object associated with this accelerometer
 * @param arg Argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int bmp5_init(struct os_dev *dev, void *arg);

/**
 * Configure the sensor
 *
 * @param bmp5 The device
 * @param cfg Ptr to sensor config
 */
int bmp5_config(struct bmp5 *bmp5, struct bmp5_cfg *cfg);

#if MYNEWT_VAL(BMP5_CLI)
int bmp5_shell_init(void);
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Create I2C bus node for BMP5 sensor
 *
 * @param node        Bus node
 * @param name        Device name
 * @param i2c_cfg     I2C node configuration
 * @param sensor_itf  Sensors interface
 *
 * @return 0 on success, non-zero on failure
 */
int bmp5_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                               const struct bus_i2c_node_cfg *i2c_cfg,
                               struct sensor_itf *sensor_itf);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __BMP5_H__ */
