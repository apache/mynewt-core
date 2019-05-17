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

#ifndef __DPS368_H__
#define __DPS368_H__


#include "os/mynewt.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#endif
#include <stats/stats.h>

#include "sensor/sensor.h"

#define    IFX_DPS368_OFF_TO_IDLE_MS        40
#define    IFX_DPS368_TRIM_FINISH_TIME_MS   12
#define    IFX_DPS368_DEV_PROD_REVID        0x10

#define    IFX_DPS368_CONFIG_TIME_MS        1 /* 1 ms config time */

#define    IFX_DPS368_FAC_TEST_RUN_PRS_ODR  DPS368_ODR_8
#define    IFX_DPS368_FAC_TEST_RUN_TMP_ODR  DPS368_ODR_4

#define    IFX_DPS368_DEFAULT_PRS_ODR       DPS368_ODR_32
#define    IFX_DPS368_DEFAULT_TMP_ODR       DPS368_ODR_8

/* DPS368 Device Register Addresses , Values and Lengths of
 * register words (values to be read or written)*/
#define    IFX_DPS368_PROD_REV_ID_REG_ADDR             0x0D
#define    IFX_DPS368_PROD_REV_ID_LEN                  1
#define    IFX_DSPS368_PROD_REV_ID_VAL                 IFX_DPS368_DEV_PROD_REVID

#define    IFX_DPS368_SOFT_RESET_REG_ADDR              0x0C
#define    IFX_DPS368_SOFT_RESET_REG_DATA              0x09
#define    IFX_DPS368_SOFT_RESET_REG_LEN               1
#define    IFX_DPS368_SOFT_RESET_VERIFY_REG_ADDR       0x06

#define    IFX_DPS368_COEF_REG_ADDR                    0x10
#define    IFX_DPS368_COEF_LEN                         18    /* Length in bytes */

#define    IFX_DPS368_TMP_COEF_SRCE_REG_ADDR           0x28
#define    IFX_DPS368_TMP_COEF_SRCE_REG_LEN            1
#define    IFX_DPS368_TMP_COEF_SRCE_REG_POS            7

#define    IFX_DPS368_PSR_TMP_READ_REG_ADDR            0x00
#define    IFX_DPS368_PSR_TMP_READ_LEN                 6

#define    IFX_DPS368_PRS_CFG_REG_ADDR                 0x06
#define    IFX_DPS368_PRS_CFG_REG_LEN                  1

#define    IFX_DPS368_TMP_CFG_REG_ADDR                 0x07
#define    IFX_DPS368_TMP_CFG_REG_LEN                  1

#define    IFX_DPS368_MEAS_CFG_REG_ADDR                0x08
#define    IFX_DPS368_MEAS_CFG_REG_LEN                 1
#define    IFX_DPS368_MEAS_CFG_REG_SEN_RDY_VAL         0x40
#define    IFX_DPS368_MEAS_CFG_REG_COEF_RDY_VAL        0x80

#define    IFX_DPS368_CFG_REG_ADDR                     0x09
#define    IFX_DPS368_CFG_REG_LEN                      1

#define    IFX_DPS368_CFG_TMP_SHIFT_EN_SET_VAL         0x08
#define    IFX_DPS368_CFG_PRS_SHIFT_EN_SET_VAL         0x04

#define    IFX_DPS368_FIFO_READ_REG_ADDR               0x00
#define    IFX_DPS368_FIFO_REG_READ_LEN                3
#define    IFX_DPS368_FIFO_BYTES_PER_ENTRY             3

#define    IFX_DPS368_FIFO_FLUSH_REG_ADDR              0x0C
#define    IFX_DPS368_FIFO_FLUSH_REG_VAL               0b10000000U

#define    IFX_DPS368_CFG_SPI_MODE_POS                 0
#define    IFX_DPS368_CFG_SPI_MODE_3_WIRE_VAL          1
#define    IFX_DPS368_CFG_SPI_MODE_4_WIRE_VAL          0

#define    IFX_DPS368_CFG_FIFO_ENABLE_POS              1
#define    IFX_DPS368_CFG_FIFO_ENABLE_VAL              1
#define    IFX_DPS368_CFG_FIFO_DISABLE_VAL             0

#define    IFX_DPS368_CFG_INTR_PRS_ENABLE_POS          4
#define    IFX_DPS368_CFG_INTR_PRS_ENABLE_VAL          1U
#define    IFX_DPS368_CFG_INTR_PRS_DISABLE_VAL         0U

#define    IFX_DPS368_CFG_INTR_TEMP_ENABLE_POS         5
#define    IFX_DPS368_CFG_INTR_TEMP_ENABLE_VAL         1U
#define    IFX_DPS368_CFG_INTR_TEMP_DISABLE_VAL        0U

#define    IFX_DPS368_CFG_INTR_FIFO_FULL_ENABLE_POS    6
#define    IFX_DPS368_CFG_INTR_FIFO_FULL_ENABLE_VAL    1U
#define    IFX_DPS368_CFG_INTR_FIFO_FULL_DISABLE_VAL   0U

#define    IFX_DPS368_CFG_INTR_LEVEL_TYP_SEL_POS       7
#define    IFX_DPS368_CFG_INTR_LEVEL_TYP_ACTIVE_H      1U
#define    IFX_DPS368_CFG_INTR_LEVEL_TYP_ACTIVE_L      0U

#define    IFX_DPS368_INTR_SOURCE_PRESSURE             0
#define    IFX_DPS368_INTR_SOURCE_TEMPERATURE          1
#define    IFX_DPS368_INTR_SOURCE_BOTH                 2

#define    IFX_DPS368_INTR_STATUS_REG_ADDR             0x0A
#define    IFX_DPS368_INTR_STATUS_REG_LEN              1

#define    IFX_DPS368_INTR_DISABLE_ALL                (uint8_t)0b10001111



/*=======================================================================

 MACROS INTERNAL DEFINITIONS

 ========================================================================*/
#define POW_2_23_MINUS_1    0x7FFFFF   //implies 2^23-1
#define POW_2_24            0x1000000
#define POW_2_15_MINUS_1    0x7FFF
#define POW_2_16            0x10000
#define POW_2_11_MINUS_1    0x7FF
#define POW_2_12            0x1000
#define POW_2_20            0x100000
#define POW_2_19_MINUS_1    524287


#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char dps_cfg;

/* Struct to hold calibration coefficients read from device*/
typedef struct
{
  /* calibration registers */
  int16_t   C0;     // 12bit
  int16_t   C1;     // 12bit

  int32_t   C00;    // 20bit
  int32_t   C10;    // 20bit
  int16_t   C01;    // 16bit
  int16_t   C11;    // 16bit
  int16_t   C20;    // 16bit
  int16_t   C21;    // 16bit
  int16_t   C30;    // 16bit

}dps3xx_cal_coeff_regs_s;

/* enum for seeting/getting device operating mode*/
typedef enum {
    DPS3xx_MODE_IDLE = 0b00000000,
    /*One shot pressure measurement*/
    DPS3xx_MODE_COMMAND_PRESSURE = 0b00000001,
    /*One shot temperature measurement */
    DPS3xx_MODE_COMMAND_TEMPERATURE = 0b00000010,
    /*Continuous pressure measurement */
    DPS3xx_MODE_BACKGROUND_PRESSURE = 0b00000101,
    /*Continuous temperature measurement */
    DPS3xx_MODE_BACKGROUND_TEMPERATURE = 0b00000110,
    /*Continuous temperature and pressure measurement */
    DPS3xx_MODE_BACKGROUND_ALL = 0b00000111,

} dps3xx_operating_modes_e;

/* enum of scaling coefficients either Kp or Kt*/
typedef enum {
    OSR_SF_1   = 524288,
    OSR_SF_2   = 1572864,
    OSR_SF_4   = 3670016,
    OSR_SF_8   = 7864320,
    OSR_SF_16  = 253952,
    OSR_SF_32  = 516096,
    OSR_SF_64  = 1040384,
    OSR_SF_128 = 2088960,

} dps3xx_scaling_coeffs_e;

/* enum of oversampling rates for pressure and temperature*/
typedef enum {
    OSR_1   = 0b00000000,
    OSR_2   = 0b00000001,
    OSR_4   = 0b00000010,
    OSR_8   = 0b00000011,
    OSR_16  = 0b00000100,
    OSR_32  = 0b00000101,
    OSR_64  = 0b00000110,
    OSR_128 = 0b00000111,

} dps3xx_osr_e;

/* enum of measurement rates for pressure*/
typedef enum
{
    ODR_1   = 0b00000000,
    ODR_2   = 0b00010000,
    ODR_4   = 0b00100000,
    ODR_8   = 0b00110000,
    ODR_16  = 0b01000000,
    ODR_32  = 0b01010000,
    ODR_64  = 0b01100000,
    ODR_128 = 0b01110000,

} dps3xx_odr_e;

/* enum of oversampling and measurement rates*/
typedef enum
{
    TMP_EXT_ASIC = 0x00,
    TMP_EXT_MEMS = 0x80,

}dps3xx_temperature_src_e;

typedef enum {
    /*Reset for first time with all initialization steps and default
    * or explicit config */
    DPS3xx_CONF_WITH_INIT_SEQUENCE  =   (1 << 0),
    /*Reset ODR OSR and also mode as per config options */
    DPS3xx_RECONF_ALL               =   (1 << 1),
    /*Reset ODR only as per config options */
    DPS3xx_RECONF_ODR_ONLY          =   (1 << 2),
    /*Reset OSR only as per config options */
    DPS3xx_RECONF_OSR_ONLY          =   (1 << 3),
    /*Reset ODR with highest possible OSR. OSR is set internally for given ODR */
    DPS3xx_RECONF_ODR_WITH_BEST_OSR =   (1 << 4),
    /*Reset Mode only */
    DPS3xx_RECONF_MODE_ONLY         =   (1 << 5),

}dps3xx_config_options_e;

struct dps368_cfg_s {
    dps3xx_osr_e osr_t;
    dps3xx_osr_e osr_p;
    dps3xx_odr_e odr_t;
    dps3xx_odr_e odr_p;
    dps3xx_operating_modes_e mode;
    dps3xx_config_options_e config_opt;
    sensor_type_t chosen_type;
};

/* Define the stats section and records */
STATS_SECT_START(dps368_stat_section)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

struct dps368 {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    union {
        struct bus_i2c_node i2c_node;
        struct bus_spi_node spi_node;
    };
    bool node_is_spi;
#else
    struct os_dev dev;
#endif
    struct sensor sensor;
    struct dps368_cfg_s cfg;

    dps3xx_scaling_coeffs_e osr_scale_t;
    dps3xx_scaling_coeffs_e osr_scale_p;
    dps3xx_cal_coeff_regs_s calib_coeffs;
    dps3xx_operating_modes_e mode;
    dps3xx_temperature_src_e temp_src;
    dps_cfg cfg_word;
    uint8_t validated;
    /* Variable used to hold stats data */
    STATS_SECT_DECL(dps368_stat_section) stats;
};


#define DPS368_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(DPS368_LOG_MODULE), __VA_ARGS__)

/**
 * Initialize the dps368.
 *
 * @param dev  Pointer to the dps368_dev device descriptor
 *
 * @return 0 on success, and non-zero error code on failure
 */
int dps368_init(struct os_dev *dev, void *arg);

/**
 * Configure DPS368 sensor
 *
 * @param Sensor device dps368 structure
 * @param Sensor device dps368 config
 *
 * @return 0 on success, and non-zero error code on failure
 */
int dps368_config(struct dps368 *dps368, struct dps368_cfg_s *cfg);

/**
 * Software reset.
 *
 * @param Ptr to the sensor
 *
 * @return 0 on success, non-zero error on failure.
 */
int dps368_soft_reset(struct sensor *sensor);

/**
 * Get the hw (product and revision from chip/ WhoAmI) id
 *
 * @param The sensor interface
 * @param ptr to fill up the hw id
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
dps368_get_hwid(struct sensor_itf *itf, uint8_t *hwid);



#if MYNEWT_VAL(DPS368_CLI)
int dps368_shell_init(void);
#endif


#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
/**
 * Create I2C bus node for DPS368 sensor
 *
 * @param node        Bus node
 * @param name        Device name
 * @param i2c_cfg     I2C node configuration
 * @param sensor_itf  Sensors interface
 *
 * @return 0 on success, non-zero on failure
 */
int
dps368_create_i2c_sensor_dev(struct bus_i2c_node *node, const char *name,
                              const struct bus_i2c_node_cfg *i2c_cfg,
                              struct sensor_itf *sensor_itf);

/**
 * Create SPI bus node for DPS368 sensor
 *
 * @param node        Bus node
 * @param name        Device name
 * @param spi_cfg     SPI node configuration
 * @param sensor_itf  Sensors interface
 *
 * @return 0 on success, non-zero on failure
 */
int
dps368_create_spi_sensor_dev(struct bus_spi_node *node, const char *name,
                              const struct bus_spi_node_cfg *spi_cfg,
                              struct sensor_itf *sensor_itf);
#endif




#ifdef __cplusplus
}
#endif



#endif /* __DPS368_H__ */
