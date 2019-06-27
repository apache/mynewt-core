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
#ifndef __BMP388_H__
#define __BMP388_H__

#include "os/mynewt.h"
#include <stats/stats.h>
#include "sensor/sensor.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TRUE
#define TRUE                UINT8_C(1)
#endif

#ifndef FALSE
#define FALSE               UINT8_C(0)
#endif

/* BMP3XX register defination */
/**\name Register Address */
#define BMP3_CHIP_ID_ADDR  0x00

/**\name BMP3 chip identifier */
#define BMP3_CHIP_ID      0x50
/**\name BMP3 pressure settling time (micro secs)*/
#define BMP3_PRESS_SETTLE_TIME  UINT16_C(392)
/**\name BMP3 temperature settling time (micro secs) */
#define BMP3_TEMP_SETTLE_TIME       UINT16_C(313)
/**\name BMP3 adc conversion time (micro secs) */
#define BMP3_ADC_CONV_TIME      UINT16_C(2000)

/**\name API warning codes */
#define BMP3_W_SENSOR_NOT_ENABLED       UINT8_C(1)
#define BMP3_W_INVALID_FIFO_REQ_FRAME_CNT   UINT8_C(2)

/**\name FIFO related macros */
/**\name FIFO enable  */
#define BMP3_ENABLE       0x01
#define BMP3_DISABLE      0x00

/**\name Sensor component selection macros
These values are internal for API implementation. Don't relate this to
data sheet.*/
#define BMP3_PRESS        UINT8_C(1)
#define BMP3_TEMP         UINT8_C(1 << 1)
#define BMP3_ALL          UINT8_C(0x03)

/**\name Power mode macros */
#define BMP3_SLEEP_MODE     UINT8_C(0x00)
#define BMP3_FORCED_MODE        UINT8_C(0x01)
#define BMP3_NORMAL_MODE        UINT8_C(0x03)

/**\name Error status macros */
#define BMP3_FATAL_ERR  UINT8_C(0x01)
#define BMP3_CMD_ERR        UINT8_C(0x02)
#define BMP3_CONF_ERR       UINT8_C(0x04)

/**\name Status macros */
#define BMP3_CMD_RDY        UINT8_C(0x10)
#define BMP3_DRDY_PRESS UINT8_C(0x20)
#define BMP3_DRDY_TEMP  UINT8_C(0x40)

/*! Power control settings */
#define POWER_CNTL      UINT16_C(0x0006)
/*! Odr and filter settings */
#define ODR_FILTER      UINT16_C(0x00F0)
/*! Interrupt control settings */
#define INT_CTRL    UINT16_C(0x0708)
/*! Advance settings */
#define ADV_SETT    UINT16_C(0x1800)

/*! FIFO settings */
/*! Mask for fifo_mode, fifo_stop_on_full, fifo_time_en, fifo_press_en and
    fifo_temp_en settings */
#define FIFO_CONFIG_1   UINT16_C(0x003E)
/*! Mask for fifo_sub_sampling and data_select settings */
#define FIFO_CONFIG_2   UINT16_C(0x00C0)
/*! Mask for fwtm_en and ffull_en settings */
#define FIFO_INT_CTRL   UINT16_C(0x0300)

/**\name Macros related to size */
#define BMP3_CALIB_DATA_LEN     UINT8_C(21)
#define BMP3_P_AND_T_HEADER_DATA_LEN    UINT8_C(7)
#define BMP3_P_OR_T_HEADER_DATA_LEN UINT8_C(4)
#define BMP3_P_T_DATA_LEN       UINT8_C(6)
#define BMP3_GEN_SETT_LEN       UINT8_C(7)
#define BMP3_P_DATA_LEN     UINT8_C(3)
#define BMP3_T_DATA_LEN     UINT8_C(3)
#define BMP3_SENSOR_TIME_LEN        UINT8_C(3)
#define BMP3_FIFO_MAX_FRAMES        UINT8_C(73)

/**\name Register Address */
#define BMP3_ERR_REG_ADDR       UINT8_C(0x02)
#define BMP3_SENS_STATUS_REG_ADDR   UINT8_C(0x03)
#define BMP3_DATA_ADDR      UINT8_C(0x04)
#define BMP3_EVENT_ADDR     UINT8_C(0x10)
#define BMP3_INT_STATUS_REG_ADDR    UINT8_C(0x11)
#define BMP3_FIFO_LENGTH_ADDR   UINT8_C(0x12)
#define BMP3_FIFO_DATA_ADDR     UINT8_C(0x14)
#define BMP3_FIFO_WM_ADDR       UINT8_C(0x15)
#define BMP3_FIFO_CONFIG_1_ADDR UINT8_C(0x17)
#define BMP3_FIFO_CONFIG_2_ADDR UINT8_C(0x18)
#define BMP3_INT_CTRL_ADDR      UINT8_C(0x19)
#define BMP3_IF_CONF_ADDR       UINT8_C(0x1A)
#define BMP3_PWR_CTRL_ADDR      UINT8_C(0x1B)
#define BMP3_OSR_ADDR           UINT8_C(0X1C)
#define BMP3_CALIB_DATA_ADDR    UINT8_C(0x31)
#define BMP3_CMD_ADDR           UINT8_C(0x7E)

/**\name FIFO Sub-sampling macros */
#define BMP3_FIFO_NO_SUBSAMPLING        UINT8_C(0x00)
#define BMP3_FIFO_SUBSAMPLING_2X        UINT8_C(0x01)
#define BMP3_FIFO_SUBSAMPLING_4X        UINT8_C(0x02)
#define BMP3_FIFO_SUBSAMPLING_8X        UINT8_C(0x03)
#define BMP3_FIFO_SUBSAMPLING_16X       UINT8_C(0x04)
#define BMP3_FIFO_SUBSAMPLING_32X       UINT8_C(0x05)
#define BMP3_FIFO_SUBSAMPLING_64X       UINT8_C(0x06)
#define BMP3_FIFO_SUBSAMPLING_128X      UINT8_C(0x07)

/**\name Over sampling macros */
#define BMP3_NO_OVERSAMPLING        0x00
#define BMP3_OVERSAMPLING_2X        0x01
#define BMP3_OVERSAMPLING_4X        0x02
#define BMP3_OVERSAMPLING_8X        0x03
#define BMP3_OVERSAMPLING_16X       0x04
#define BMP3_OVERSAMPLING_32X       0x05

/**\name Odr setting macros */
#define BMP3_ODR_200_HZ             0x00
#define BMP3_ODR_100_HZ             0x01
#define BMP3_ODR_50_HZ              0x02
#define BMP3_ODR_25_HZ              0x03
#define BMP3_ODR_12_5_HZ            0x04
#define BMP3_ODR_6_25_HZ            0x05
#define BMP3_ODR_3_1_HZ             0x06
#define BMP3_ODR_1_5_HZ             0x07
#define BMP3_ODR_0_78_HZ            0x08
#define BMP3_ODR_0_39_HZ            0x09
#define BMP3_ODR_0_2_HZ             0x0A
#define BMP3_ODR_0_1_HZ             0x0B
#define BMP3_ODR_0_05_HZ            0x0C
#define BMP3_ODR_0_02_HZ            0x0D
#define BMP3_ODR_0_01_HZ            0x0E
#define BMP3_ODR_0_006_HZ           0x0F
#define BMP3_ODR_0_003_HZ           0x10
#define BMP3_ODR_0_001_HZ           0x11

/**\name Macros to select the which sensor settings are to be set by the user.
These values are internal for API implementation. Don't relate this to
data sheet. */
#define BMP3_PRESS_EN_SEL               UINT16_C(1 << 1)
#define BMP3_TEMP_EN_SEL                UINT16_C(1 << 2)
#define BMP3_DRDY_EN_SEL                UINT16_C(1 << 3)
#define BMP3_PRESS_OS_SEL               UINT16_C(1 << 4)
#define BMP3_TEMP_OS_SEL                UINT16_C(1 << 5)
#define BMP3_IIR_FILTER_SEL             UINT16_C(1 << 6)
#define BMP3_ODR_SEL                    UINT16_C(1 << 7)
#define BMP3_OUTPUT_MODE_SEL            UINT16_C(1 << 8)
#define BMP3_LEVEL_SEL                  UINT16_C(1 << 9)
#define BMP3_LATCH_SEL                  UINT16_C(1 << 10)
#define BMP3_I2C_WDT_EN_SEL             UINT16_C(1 << 11)
#define BMP3_I2C_WDT_SEL_SEL            UINT16_C(1 << 12)
#define BMP3_ALL_SETTINGS               UINT16_C(0x7FF)

/**\name Macros to select the which FIFO settings are to be set by the user
These values are internal for API implementation. Don't relate this to
data sheet.*/
#define BMP3_FIFO_MODE_SEL                  UINT16_C(1 << 1)
#define BMP3_FIFO_STOP_ON_FULL_EN_SEL       UINT16_C(1 << 2)
#define BMP3_FIFO_TIME_EN_SEL               UINT16_C(1 << 3)
#define BMP3_FIFO_PRESS_EN_SEL              UINT16_C(1 << 4)
#define BMP3_FIFO_TEMP_EN_SEL               UINT16_C(1 << 5)
#define BMP3_FIFO_DOWN_SAMPLING_SEL         UINT16_C(1 << 6)
#define BMP3_FIFO_FILTER_EN_SEL             UINT16_C(1 << 7)
#define BMP3_FIFO_FWTM_EN_SEL               UINT16_C(1 << 8)
#define BMP3_FIFO_FULL_EN_SEL               UINT16_C(1 << 9)
#define BMP3_FIFO_ALL_SETTINGS              UINT16_C(0x3FF)


#define BMP3_ERR_FATAL_MSK      UINT8_C(0x01)

#define BMP3_ERR_CMD_MSK        UINT8_C(0x02)
#define BMP3_ERR_CMD_POS        UINT8_C(0x01)

#define BMP3_ERR_CONF_MSK       UINT8_C(0x04)
#define BMP3_ERR_CONF_POS       UINT8_C(0x02)

#define BMP3_STATUS_CMD_RDY_MSK UINT8_C(0x10)
#define BMP3_STATUS_CMD_RDY_POS UINT8_C(0x04)

#define BMP3_STATUS_DRDY_PRESS_MSK  UINT8_C(0x20)
#define BMP3_STATUS_DRDY_PRESS_POS  UINT8_C(0x05)

#define BMP3_STATUS_DRDY_TEMP_MSK   UINT8_C(0x40)
#define BMP3_STATUS_DRDY_TEMP_POS   UINT8_C(0x06)

#define BMP3_INT_STATUS_FWTM_MSK    UINT8_C(0x01)

#define BMP3_INT_STATUS_FFULL_MSK   UINT8_C(0x02)
#define BMP3_INT_STATUS_FFULL_POS   UINT8_C(0x01)

#define BMP3_INT_STATUS_DRDY_MSK    UINT8_C(0x08)
#define BMP3_INT_STATUS_DRDY_POS    UINT8_C(0x03)

#define BMP3_OP_MODE_MSK        UINT8_C(0x30)
#define BMP3_OP_MODE_POS        UINT8_C(0x04)

#define BMP3_PRESS_EN_MSK       UINT8_C(0x01)

#define BMP3_TEMP_EN_MSK        UINT8_C(0x02)
#define BMP3_TEMP_EN_POS        UINT8_C(0x01)

#define BMP3_IIR_FILTER_MSK     UINT8_C(0x0E)
#define BMP3_IIR_FILTER_POS     UINT8_C(0x01)

#define BMP3_ODR_MSK            UINT8_C(0x1F)

#define BMP3_PRESS_OS_MSK       UINT8_C(0x07)

#define BMP3_TEMP_OS_MSK        UINT8_C(0x38)
#define BMP3_TEMP_OS_POS        UINT8_C(0x03)

#define BMP3_INT_OUTPUT_MODE_MSK    UINT8_C(0x01)

#define BMP3_INT_LEVEL_MSK      UINT8_C(0x02)
#define BMP3_INT_LEVEL_POS      UINT8_C(0x01)

#define BMP3_INT_LATCH_MSK      UINT8_C(0x04)
#define BMP3_INT_LATCH_POS      UINT8_C(0x02)

#define BMP3_INT_DRDY_EN_MSK        UINT8_C(0x40)
#define BMP3_INT_DRDY_EN_POS        UINT8_C(0x06)

#define BMP3_I2C_WDT_EN_MSK     UINT8_C(0x02)
#define BMP3_I2C_WDT_EN_POS     UINT8_C(0x01)

#define BMP3_I2C_WDT_SEL_MSK        UINT8_C(0x04)
#define BMP3_I2C_WDT_SEL_POS        UINT8_C(0x02)

#define BMP3_FIFO_MODE_MSK      UINT8_C(0x01)

#define BMP3_FIFO_STOP_ON_FULL_MSK  UINT8_C(0x02)
#define BMP3_FIFO_STOP_ON_FULL_POS  UINT8_C(0x01)

#define BMP3_FIFO_TIME_EN_MSK       UINT8_C(0x04)
#define BMP3_FIFO_TIME_EN_POS       UINT8_C(0x02)

#define BMP3_FIFO_PRESS_EN_MSK  UINT8_C(0x08)
#define BMP3_FIFO_PRESS_EN_POS  UINT8_C(0x03)

#define BMP3_FIFO_TEMP_EN_MSK       UINT8_C(0x10)
#define BMP3_FIFO_TEMP_EN_POS       UINT8_C(0x04)

#define BMP3_FIFO_FILTER_EN_MSK UINT8_C(0x18)
#define BMP3_FIFO_FILTER_EN_POS UINT8_C(0x03)

#define BMP3_FIFO_DOWN_SAMPLING_MSK UINT8_C(0x07)

#define BMP3_FIFO_FWTM_EN_MSK       UINT8_C(0x08)
#define BMP3_FIFO_FWTM_EN_POS       UINT8_C(0x03)

#define BMP3_FIFO_FULL_EN_MSK       UINT8_C(0x10)
#define BMP3_FIFO_FULL_EN_POS       UINT8_C(0x04)

/**\name    UTILITY MACROS  */
#define BMP3_SET_LOW_BYTE           UINT16_C(0x00FF)
#define BMP3_SET_HIGH_BYTE          UINT16_C(0xFF00)

/**\name Macro to combine two 8 bit data's to form a 16 bit data */
#define BMP3_CONCAT_BYTES(msb, lsb)     (((uint16_t)msb << 8) | (uint16_t)lsb)

#define BMP3_SET_BITS(reg_data, bitname, data) \
                ((reg_data & ~(bitname##_MSK)) | \
                ((data << bitname##_POS) & bitname##_MSK))
/* Macro variant to handle the bitname position if it is zero */
#define BMP3_SET_BITS_POS_0(reg_data, bitname, data) \
                ((reg_data & ~(bitname##_MSK)) | \
                (data & bitname##_MSK))

#define BMP3_GET_BITS(reg_data, bitname)  ((reg_data & (bitname##_MSK)) >> \
                            (bitname##_POS))
/* Macro variant to handle the bitname position if it is zero */
#define BMP3_GET_BITS_POS_0(reg_data, bitname)  (reg_data & (bitname##_MSK))

#define BMP3_GET_LSB(var)   (uint8_t)(var & BMP3_SET_LOW_BYTE)
#define BMP3_GET_MSB(var)   (uint8_t)((var & BMP3_SET_HIGH_BYTE) >> 8)

/**\name API success code */
#define BMP3_OK                             INT8_C(0)
/**\name API error codes */
#define BMP3_E_NULL_PTR                     INT8_C(-1)
#define BMP3_E_DEV_NOT_FOUND                INT8_C(-2)
#define BMP3_E_INVALID_ODR_OSR_SETTINGS     INT8_C(-3)
#define BMP3_E_CMD_EXEC_FAILED              INT8_C(-4)
#define BMP3_E_CONFIGURATION_ERR            INT8_C(-5)
#define BMP3_E_INVALID_LEN                  INT8_C(-6)
#define BMP3_E_COMM_FAIL                    INT8_C(-7)
#define BMP3_E_FIFO_WATERMARK_NOT_REACHED   INT8_C(-8)
#define BMP3_E_WRITE                        INT8_C(-9)
#define BMP3_E_READ                         INT8_C(-10)

#define BMP388_INT_DRDY_STATE                 0x08
#define BMP388_INT_FIFOWTM_STATE              0x01
#define BMP388_INT_FIFOFULL_STATE             0x02

#define BMP388_INT_CFG_FIFOWTM                BMP3_ENABLE
#define BMP388_INT_CFG_FIFOFULL               BMP3_ENABLE
#define BMP388_INT_CFG_DRDY                   BMP3_ENABLE

/*! FIFO Header */
/*! FIFO temperature pressure header frame */
#define FIFO_TEMP_PRESS_FRAME   UINT8_C(0x94)
/*! FIFO temperature header frame */
#define FIFO_TEMP_FRAME     UINT8_C(0x90)
/*! FIFO pressure header frame */
#define FIFO_PRESS_FRAME    UINT8_C(0x84)
/*! FIFO time header frame */
#define FIFO_TIME_FRAME     UINT8_C(0xA0)
/*! FIFO error header frame */
#define FIFO_ERROR_FRAME    UINT8_C(0x44)
/*! FIFO configuration change header frame */
#define FIFO_CONFIG_CHANGE  UINT8_C(0x48)

enum bmp388_fifo_mode {
    BMP388_FIFO_M_BYPASS               = 0,
    BMP388_FIFO_M_FIFO                 = 1,
    BMP388_FIFO_M_CONTINUOUS_TO_FIFO   = 3,
    BMP388_FIFO_M_BYPASS_TO_CONTINUOUS = 4,
    BMP388_FIFO_M_CONTINUOUS           = 6
};

enum bmp388_int_type {
    BMP388_DRDY_INT                   = 1,
    BMP388_FIFO_WTMK_INT              = 2,
    BMP388_FIFO_FULL_INT              = 3,
};

enum bmp388_read_mode {
    BMP388_READ_M_POLL = 0,
    BMP388_READ_M_STREAM = 1,
};

/* Read mode configuration */
struct bmp388_read_mode_cfg {
    enum bmp388_read_mode mode;
    uint8_t int_num:1;
    uint8_t int_type;
};

struct bmp388_cfg {
    uint8_t rate;

    /* Read mode config */
    struct bmp388_read_mode_cfg read_mode;

    uint8_t filter_press_osr;
    uint8_t filter_temp_osr;

    /* interrupt config */
    uint8_t int_enable_type  : 2;
    uint8_t int_pp_od   : 1;
    uint8_t int_latched : 1;
    uint8_t int_active_low  : 1;

    /* Power mode */
    uint8_t power_mode     : 4;

    /* fifo  config */
    enum bmp388_fifo_mode fifo_mode;
    uint8_t fifo_threshold;

    /* Sensor type mask to track enabled sensors */
    sensor_type_t mask;
};

/* Used to track interrupt state to wake any present waiters */
struct bmp388_int {
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
struct bmp388_pdd {
    /* Notification event context */
    struct sensor_notify_ev_ctx notify_ctx;
    /* Inetrrupt state */
    struct bmp388_int *interrupt;
    /* Interrupt enabled flag */
    uint16_t int_enable;
};

/* Define the stats section and records */
STATS_SECT_START(bmp388_stat_section)
    STATS_SECT_ENTRY(write_errors)
    STATS_SECT_ENTRY(read_errors)
STATS_SECT_END

struct bmp388 {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_i2c_node i2c_node;
#else
    struct os_dev dev;
#endif
    struct sensor sensor;
    struct bmp388_cfg cfg;
    struct bmp388_int intr;
    struct bmp388_pdd pdd;
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bool node_is_spi;
#endif
    /* Variable used to hold stats data */
    STATS_SECT_DECL(bmp388_stat_section) stats;


};
/********************************************************/

/*!
* @brief Interface selection Enums
*/
enum bmp3_intf {
    /*! SPI interface */
    BMP3_SPI_INTF,
    /*! I2C interface */
    BMP3_I2C_INTF
};

/*!
* @brief bmp3 sensor structure which comprises of temperature and pressure
* data.
*/
struct bmp3_data {
    /*! Compensated temperature */
    int64_t temperature;
    /*! Compensated pressure */
    uint64_t pressure;
};

/*!
* @brief bmp3 sensor structure which comprises of uncompensated temperature
* and pressure data.
*/
struct bmp3_uncomp_data {
    /*! un-compensated pressure */
    uint32_t pressure;
    /*! un-compensated temperature */
    uint32_t temperature;
};

/********************************************************/
/*!
* @brief Register Trim Variables
*/
struct bmp3_reg_calib_data {

    uint16_t par_t1;
    uint16_t par_t2;
    int8_t par_t3;
    int16_t par_p1;
    int16_t par_p2;
    int8_t par_p3;
    int8_t par_p4;
    uint16_t par_p5;
    uint16_t par_p6;
    int8_t par_p7;
    int8_t par_p8;
    int16_t par_p9;
    int8_t par_p10;
    int8_t par_p11;
    int64_t t_lin;
};

/*!
* brief Calibration data
*/
struct bmp3_calib_data {
    /*! Register data */
    struct bmp3_reg_calib_data reg_calib_data;
};

/*!
* brief bmp3 advance settings
*/
struct bmp3_adv_settings {
    /*! i2c watch dog enable */
    uint8_t i2c_wdt_en;
    /*! i2c watch dog select */
    uint8_t i2c_wdt_sel;
};

/*!
* brief bmp3 odr and filter settings
*/
struct bmp3_odr_filter_settings {
    /*! Pressure oversampling */
    uint8_t press_os;
    /*! Temperature oversampling */
    uint8_t temp_os;
    /*! IIR filter */
    uint8_t iir_filter;
    /*! Output data rate */
    uint8_t odr;
};

/*!
* brief bmp3 interrupt pin settings
*/
struct bmp3_int_ctrl_settings {
    /*! Output mode */
    uint8_t output_mode;
    /*! Active high/low */
    uint8_t level;
    /*! Latched or Non-latched */
    uint8_t latch;
    /*! Data ready interrupt */
    uint8_t drdy_en;
};

/*!
* brief bmp3 device settings
*/
struct bmp3_settings {
    /*! Power mode which user wants to set */
    uint8_t op_mode;
    /*! Enable/Disable pressure sensor */
    uint8_t press_en;
    /*! Enable/Disable temperature sensor */
    uint8_t temp_en;
    /*! ODR and filter configuration */
    struct bmp3_odr_filter_settings odr_filter;
    /*! Interrupt configuration */
    struct bmp3_int_ctrl_settings int_settings;
    /*! Advance settings */
    struct bmp3_adv_settings adv_settings;
};

/*!
* brief bmp3 fifo frame
*/
struct bmp3_fifo_data {
    /*! Data buffer of user defined length is to be mapped here
        512 + 4 + 7*3 */
    uint8_t buffer[540];
    /*! Number of bytes of data read from the fifo */
    uint16_t byte_count;
    /*! Number of frames to be read as specified by the user */
    uint8_t req_frames;
    /*! Will be equal to length when no more frames are there to parse */
    uint16_t start_idx;
    /*! Will contain the no of parsed data frames from fifo */
    uint8_t parsed_frames;
    /*! Configuration error */
    uint8_t config_err;
    /*! Sensor time */
    uint32_t sensor_time;
    /*! FIFO input configuration change */
    uint8_t config_change;
    /*! All available frames are parsed */
    uint8_t frame_not_available;
};

/*!
* brief bmp3 fifo configuration
*/
struct bmp3_fifo_settings {
    /*! enable/disable */
    uint8_t mode;
    /*! stop on full enable/disable */
    uint8_t stop_on_full_en;
    /*! time enable/disable */
    uint8_t time_en;
    /*! pressure enable/disable */
    uint8_t press_en;
    /*! temperature enable/disable */
    uint8_t temp_en;
    /*! down sampling rate */
    uint8_t down_sampling;
    /*! filter enable/disable */
    uint8_t filter_en;
    /*! FIFO watermark enable/disable */
    uint8_t fwtm_en;
    /*! FIFO full enable/disable */
    uint8_t ffull_en;
};

/*!
* brief bmp3 bmp3 FIFO
*/
struct bmp3_fifo {
    /*! FIFO frame structure */
    struct bmp3_fifo_data data;
    /*! FIFO config structure */
    struct bmp3_fifo_settings settings;
    bool no_need_sensortime;
    bool sensortime_updated;
};

/*!
* brief bmp3 sensor status flags
*/
struct bmp3_sens_status {
    /*! Command ready status */
    uint8_t cmd_rdy;
    /*! Data ready for pressure */
    uint8_t drdy_press;
    /*! Data ready for temperature */
    uint8_t drdy_temp;
};

/*!
* brief bmp3 interrupt status flags
*/
struct bmp3_int_status {
    /*! fifo watermark interrupt */
    uint8_t fifo_wm;
    /*! fifo full interrupt */
    uint8_t fifo_full;
    /*! data ready interrupt */
    uint8_t drdy;
};

/*!
* brief bmp3 error status flags
*/
struct bmp3_err_status {
    /*! fatal error */
    uint8_t fatal;
    /*! command error */
    uint8_t cmd;
    /*! configuration error */
    uint8_t conf;
};

/*!
* brief bmp3 status flags
*/
struct bmp3_status {
    /*! Interrupt status */
    struct bmp3_int_status intr;
    /*! Sensor status */
    struct bmp3_sens_status sensor;
    /*! Error status */
    struct bmp3_err_status err;
    /*! power on reset status */
    uint8_t pwr_on_rst;
};

/*!
* brief bmp3 device structure
*/
struct bmp3_dev {
    /*! Chip Id */
    uint8_t chip_id;
    /*! Device Id */
    uint8_t dev_id;
    /*! SPI/I2C interface */
    enum bmp3_intf intf;
    /*! Decide SPI or I2C read mechanism */
    uint8_t dummy_byte;
    /*! Trim data */
    struct bmp3_calib_data calib_data;
    /*! Sensor Settings */
    struct bmp3_settings settings;
    /*! Sensor and interrupt status flags */
    struct bmp3_status status;
    /*! FIFO data and settings structure */
    struct bmp3_fifo *fifo;
    /* fifo water marklevel */
    uint8_t fifo_watermark_level;
};

/**
* Set bmp388 to normal mode
*
* @param itf The sensor interface
*
* @return 0 on success, non-zero on failure
*/
int8_t bmp388_set_normal_mode(struct sensor_itf *itf, struct bmp3_dev *dev);

/**
* Set bmp388 to force mode with OSR
*
* @param itf The sensor interface
*
* @return 0 on success, non-zero on failure
*/
int8_t bmp388_set_forced_mode_with_osr(struct sensor_itf *itf, struct bmp3_dev *dev);

/*!
*  @brief This API is the entry point.
*  It performs the selection of I2C/SPI read mechanism according to the
*  selected interface and reads the chip-id and calibration data of the sensor.
*/
int8_t bmp3_init(struct sensor_itf *itf, struct bmp3_dev *dev);

int8_t bmp388_get_sensor_data(struct sensor_itf *itf, struct bmp3_dev *dev, struct bmp3_data *sensor_data);

/*!
* @brief This API sets the power control(pressure enable and
* temperature enable), over sampling, odr and filter
* settings in the sensor.
*
* @param[in] dev : Structure instance of bmp3_dev.
* @param[in] desired_settings : Variable used to select the settings which
* are to be set in the sensor.
*
* @note : Below are the macros to be used by the user for selecting the
* desired settings. User can do OR operation of these macros for configuring
* multiple settings.
*
* Macros          |   Functionality
* -----------------------|----------------------------------------------
* BMP3_PRESS_EN_SEL    |   Enable/Disable pressure.
* BMP3_TEMP_EN_SEL     |   Enable/Disable temperature.
* BMP3_PRESS_OS_SEL    |   Set pressure oversampling.
* BMP3_TEMP_OS_SEL     |   Set temperature oversampling.
* BMP3_IIR_FILTER_SEL  |   Set IIR filter.
* BMP3_ODR_SEL         |   Set ODR.
* BMP3_OUTPUT_MODE_SEL |   Set either open drain or push pull
* BMP3_LEVEL_SEL       |   Set interrupt pad to be active high or low
* BMP3_LATCH_SEL       |   Set interrupt pad to be latched or nonlatched.
* BMP3_DRDY_EN_SEL     |   Map/Unmap the drdy interrupt to interrupt pad.
* BMP3_I2C_WDT_EN_SEL  |   Enable/Disable I2C internal watch dog.
* BMP3_I2C_WDT_SEL_SEL |   Set I2C watch dog timeout delay.
*
* @return Result of API execution status
* @retval zero -> Success / +ve value -> Warning / -ve value -> Error.
*/
int8_t bmp3_set_sensor_settings(struct sensor_itf *itf, uint32_t desired_settings, struct bmp3_dev *dev);

/**
* Get chip ID
*
* @param itf The sensor interface
* @param chip_id Ptr to chip id to be filled up
*/
int bmp388_get_chip_id(struct sensor_itf *itf, uint8_t *chip_id);

/**
* Dump the registers
*
* @param itf The sensor interface
*/
int bmp388_dump(struct sensor_itf *itf);


/**
* Sets the rate
*
* @param itf The sensor interface
* @param rate The sampling rate of the sensor
*
* @return 0 on success, non-zero on failure
*/
int bmp388_set_rate(struct sensor_itf *itf, uint8_t rate);

/**
* Gets the current rate
*
* @param itf The sensor interface
* @param rate Ptr to rate read from the sensor
*
* @return 0 on success, non-zero on failure
*/
int bmp388_get_rate(struct sensor_itf *itf, uint8_t *rate);

/**
* Sets the power mode of the sensor
*
* @param itf The sensor interface
* @param mode Power mode
*
* @return 0 on success, non-zero on failure
*/
int bmp388_set_power_mode(struct sensor_itf *itf, uint8_t mode);

/**
* Gets the power mode of the sensor
*
* @param itf The sensor interface
* @param mode Power mode
*
* @return 0 on success, non-zero on failure
*/
int bmp388_get_power_mode(struct sensor_itf *itf, uint8_t *mode);

/**
* Sets the interrupt push-pull/open-drain selection
*
* @param itf The sensor interface
* @param mode Interrupt setting (0 = push-pull, 1 = open-drain)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_set_int_pp_od(struct sensor_itf *itf, uint8_t mode);

/**
* Gets the interrupt push-pull/open-drain selection
*
* @param itf The sensor interface
* @param mode Ptr to store setting (0 = push-pull, 1 = open-drain)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_get_int_pp_od(struct sensor_itf *itf, uint8_t *mode);

/**
* Sets whether latched interrupts are enabled
*
* @param itf The sensor interface
* @param en Value to set (0 = not latched, 1 = latched)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_set_latched_int(struct sensor_itf *itf, uint8_t en);

/**
* Gets whether latched interrupts are enabled
*
* @param itf The sensor interface
* @param en Ptr to store value (0 = not latched, 1 = latched)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_get_latched_int(struct sensor_itf *itf, uint8_t *en);

/**
* Sets whether interrupts are active high or low
*
* @param itf The sensor interface
* @param low Value to set (0 = active high, 1 = active low)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_set_int_active_low(struct sensor_itf *itf, uint8_t low);

/**
* Gets whether interrupts are active high or low
*
* @param itf The sensor interface
* @param low Ptr to store value (0 = active high, 1 = active low)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_get_int_active_low(struct sensor_itf *itf, uint8_t *low);

/**
* Set filter config
*
* @param itf The sensor interface
* @param bw The filter bandwidth
* @param type The filter type (1 = high pass, 0 = low pass)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_set_filter_cfg(struct sensor_itf *itf, uint8_t press_osr, uint8_t temp_osr);

/**
* Get filter config
*
* @param itf The sensor interface
* @param bw Ptr to the filter bandwidth
* @param type Ptr to filter type (1 = high pass, 0 = low pass)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_get_filter_cfg(struct sensor_itf *itf, uint8_t *bw, uint8_t *type);


/**
* Clear interrupt pin configuration for interrupt 1
*
* @param itf The sensor interface
* @param cfg int1 config
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_clear_int1_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
* Clear interrupt pin configuration for interrupt 2
*
* @param itf The sensor interface
* @param cfg int2 config
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_clear_int2_pin_cfg(struct sensor_itf *itf, uint8_t cfg);

/**
* Set whether interrupts are enabled
*
* @param itf The sensor interface
* @param enabled Value to set (0 = disabled, 1 = enabled)
*
* @return 0 on success, non-zero on failure
*/
int bmp388_set_int_enable(struct sensor_itf *itf, uint8_t enabled, uint8_t int_type);

/**
* Clear interrupts
*
* @param itf The sensor interface
* @param src Ptr to return interrupt source in
*
* @return 0 on success, non-zero on failure
*/
int bmp388_clear_int(struct sensor_itf *itf);

/**
* Setup FIFO
*
* @param itf The sensor interface
* @param mode FIFO mode to setup
* @param fifo_ths Threshold to set for FIFO
*
* @return 0 on success, non-zero on failure
*/
int bmp388_set_fifo_cfg(struct sensor_itf *itf, enum bmp388_fifo_mode mode, uint8_t fifo_ths);

/**
* Run Self test on sensor
*
* @param itf The sensor interface
* @param result Ptr to return test result in (0 on pass, non-zero on failure)
*
* @return 0 on sucess, non-zero on failure
*/
int bmp388_run_self_test(struct sensor_itf *itf, int *result);

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
int bmp388_stream_read(struct sensor *sensor,
                        sensor_type_t sensor_type,
                        sensor_data_func_t read_func,
                        void *read_arg,
                        uint32_t time_ms);

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
int bmp388_poll_read(struct sensor *sensor,
                    sensor_type_t sensor_type,
                    sensor_data_func_t data_func,
                    void *data_arg,
                    uint32_t timeout);

/**
* Expects to be called back through os_dev_create().
*
* @param dev Ptr to the device object associated with this accelerometer
* @param arg Argument passed to OS device init
*
* @return 0 on success, non-zero on failure.
*/
int bmp388_init(struct os_dev *dev, void *arg);

/**
* Configure the sensor
*
* @param lis2dw12 Ptr to sensor driver
* @param cfg Ptr to sensor driver config
*/
int bmp388_config(struct bmp388 *bmp388, struct bmp388_cfg *cfg);

#if MYNEWT_VAL(BMP388_CLI)
int bmp388_shell_init(void);
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
* Create I2C bus node for BMP388 sensor
*
* @param node        Bus node
* @param name        Device name
* @param i2c_cfg     I2C node configuration
* @param sensor_itf  Sensors interface
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                                        const struct bus_i2c_node_cfg *i2c_cfg,
                                        struct sensor_itf *sensor_itf);

/**
* Create SPI bus node for bmp388 sensor
*
* @param node        Bus node
* @param name        Device name
* @param spi_cfg     SPI node configuration
* @param sensor_itf  Sensors interface
*
* @return 0 on success, non-zero on failure
*/
int
bmp388_create_spi_sensor_dev(struct bus_spi_node *node, const char *name,
                                        const struct bus_spi_node_cfg *spi_cfg,
                                        struct sensor_itf *sensor_itf);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __BMP388_H__ */
