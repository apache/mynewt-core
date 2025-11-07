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

#ifndef __INA228_H__
#define __INA228_H__

#include <stdint.h>
#include <os/mynewt.h>
#include <stats/stats.h>
#include <sensor/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INA228_CONFIG_REG_ADDR            0x00
#define INA228_ADC_CONFIG_REG_ADDR        0x01
#define INA228_SHUNT_CAL_REG_ADDR         0x02
#define INA228_SHUNT_TEMPCO_REG_ADDR      0x03
#define INA228_VSHUNT_REG_ADDR            0x04
#define INA228_VBUS_REG_ADDR              0x05
#define INA228_DIETEMP_REG_ADDR           0x06
#define INA228_CURRENT_REG_ADDR           0x07
#define INA228_POWER_REG_ADDR             0x08
#define INA228_ENERGY_REG_ADDR            0x09
#define INA228_CHARGE_REG_ADDR            0x0A
#define INA228_DIAG_ALRT_REG_ADDR         0x0B
#define INA228_SOVL_REG_ADDR              0x0C
#define INA228_SUVL_REG_ADDR              0x0D
#define INA228_BOVL_REG_ADDR              0x0E
#define INA228_BUVL_REG_ADDR              0x0F
#define INA228_TEMP_LOMIT_REG_ADDR        0x10
#define INA228_PWR_LOMIT_REG_ADDR         0x11
#define INA228_MANUFACTURER_ID_REG_ADDR   0x3E
#define INA228_DEVICE_ID_REG_ADDR         0x3F

#define INA228_CONFIG_ADCRANGE_Pos        4u
#define INA228_CONFIG_ADCRANGE_Msk        (1u << INA228_CONFIG_ADCRANGE_Pos)
#define INA228_CONFIG_TEMPCOMP_Pos        5u
#define INA228_CONFIG_TEMPCOMP_Msk        (1u << INA228_CONFIG_TEMPCOMP_Pos)
#define INA228_CONFIG_CONVDL_Pos          6u
#define INA228_CONFIG_CONVDL_Msk          (0xFF << INA228_CONFIG_CONVDL_Pos)
#define INA228_CONFIG_RSTACC_Pos          14u
#define INA228_CONFIG_RSTACC_Msk          (1u << INA228_CONFIG_RSTACC_Pos)
#define INA228_CONFIG_RST_Pos             15u
#define INA228_CONFIG_RST_Msk             (1u << INA228_CONFIG_RST_Pos)

#define INA228_ADC_CONFIG_MODE_Pos        12u
#define INA228_ADC_CONFIG_MODE_Msk        (0xF << INA228_ADC_CONFIG_MODE_Pos)
#define INA228_ADC_CONFIG_VBUSCT_Pos      9u
#define INA228_ADC_CONFIG_VBUSCT_Msk      (0x7 << INA228_ADC_CONFIG_VBUSCT_Pos)
#define INA228_ADC_CONFIG_VSHCT_Pos       6u
#define INA228_ADC_CONFIG_VSHCT_Msk       (0x7 << INA228_ADC_CONFIG_VSHCT_Pos)
#define INA228_ADC_CONFIG_VTCT_Pos        3u
#define INA228_ADC_CONFIG_VTCT_Msk        (0x7 << INA228_ADC_CONFIG_VTCT_Pos)
#define INA228_ADC_CONFIG_AVG_Pos         0u
#define INA228_ADC_CONFIG_AVG_Msk         (0x7 << INA228_ADC_CONFIG_AVG_Pos)

#define INA228_ADC_CONFIG_MODE_SHUTDOWN   0
#define INA228_ADC_CONFIG_MODE_VBUS       (1 << INA228_ADC_CONFIG_MODE_Pos)
#define INA228_ADC_CONFIG_MODE_VSHUNT     (2 << INA228_ADC_CONFIG_MODE_Pos)
#define INA228_ADC_CONFIG_MODE_TEMP       (4 << INA228_ADC_CONFIG_MODE_Pos)
#define INA228_ADC_CONFIG_MODE_CONTINUOUS (8 << INA228_ADC_CONFIG_MODE_Pos)

#define INA228_SHUNT_CAL_SHUNT_CAL_Pos    0u
#define INA228_SHUNT_CAL_SHUNT_CAL_Msk    (0x7FFF << INA228_SHUNT_CAL_SHUNT_CAL_Pos)

#define INA228_SHUNT_TEMPCO_TEMPCO_Pos    0u
#define INA228_SHUNT_TEMPCO_TEMPCO_Msk    (0x3FFF << INA228_SHUNT_TEMPCO_TEMPCO_Pos)

#define INA228_VSHUNT_VSHUNT_Pos          4u
#define INA228_VSHUNT_VSHUNT_Msk          (0x0FFFFF << INA228_VSHUNT_VSHUNT_Pos)

#define INA228_VBUS_VBUS_Pos              4u
#define INA228_VBUS_VBUS_Msk              (0x0FFFFF << INA228_VBUS_VBUS_Pos)

#define INA228_DIETEMP_DIETEMP_Pos        0u
#define INA228_DIETEMP_DIETEMP_Msk        (0xFFFF << INA228_DIETEMP_DIETEMP_Pos)

#define INA228_CURRENT_CURRENT_Pos        4u
#define INA228_CURRENT_CURRENT_Msk        (0x0FFFFF << INA228_CURRENT_CURRENT_Pos)

#define INA228_POWER_POWER_Pos            0u
#define INA228_POWER_POWER_Msk            (0x0FFFFFF << INA228_POWER_POWER_Pos)

#define INA228_ENERGY_ENERGY_Pos          0u
#define INA228_ENERGY_ENERGY_Msk          (0x00FFFFFFFFFFULL << INA228_ENERGY_ENERGY_Pos)

#define INA228_CHARGE_CHARGE_Pos          0u
#define INA228_CHARGE_CHARGE_Msk          (0x00FFFFFFFFFFULL << INA228_CHARGE_CHARGE_Pos)

#define INA228_MANUFACTURER_ID            0x5449

#define INA228_DIAG_ALRT_MEMSTAT          0x0001
#define INA228_DIAG_ALRT_CNVRF            0x0002
#define INA228_DIAG_ALRT_POL              0x0004
#define INA228_DIAG_ALRT_BUSUL            0x0008
#define INA228_DIAG_ALRT_BUSOL            0x0010
#define INA228_DIAG_ALRT_SHNTUL           0x0020
#define INA228_DIAG_ALRT_SHNTOL           0x0040
#define INA228_DIAG_ALRT_TMPOL            0x0080
#define INA228_DIAG_ALRT_MATHOF           0x0200
#define INA228_DIAG_ALRT_CHARGEOF         0x0400
#define INA228_DIAG_ALRT_ENERGYOF         0x0800
#define INA228_DIAG_ALRT_APOL             0x1000
#define INA228_DIAG_ALRT_SLOWALERT        0x2000
#define INA228_DIAG_ALRT_CNRV             0x4000
#define INA228_DIAG_ALRT_ALATCH           0x8000

/* 312.5 nV */
#define INA228_SHUNT_VOLTAGE_0_LSB        312500
/* 78.125 nV */
#define INA228_SHUNT_VOLTAGE_1_LSB        78125
/* 195.3125 uV */
#define INA228_BUS_VOLTAGE_LSB            195312
/* 7.8125 mC */
#define INA228_TEMPERATURE_LSB            7812

/* INA228 operating modes */
enum ina228_avg_mode {
    INA228_AVG_1,
    INA228_AVG_4,
    INA228_AVG_16,
    INA228_AVG_64,
    INA228_AVG_128,
    INA228_AVG_256,
    INA228_AVG_512,
    INA228_AVG_1024,
};

/* Bus/shunt voltage/temperature conversion time */
enum ina228_ct {
    INA228_CT_50_US,
    INA228_CT_84_US,
    INA228_CT_150_US,
    INA228_CT_280_US,
    INA228_CT_540_US,
    INA228_CT_1052_US,
    INA228_CT_2074_US,
    INA228_CT_4120_US,
};

/* INA228 operating modes */
enum ina228_oper_mode {
    INA228_MODE_SHUTDOWN = 0,
    INA228_MODE_BUS_VOLTAGE = 1,
    INA228_MODE_SHUNT_VOLTAGE = 2,
    INA228_MODE_TEMPERATURE = 4,
    INA228_MODE_CONTINUOUS = 8,
};

struct ina228_hw_cfg {
    struct sensor_itf itf;
    /** Shunt resistance in mOhm. */
    uint32_t shunt_resistance;
    /** Interrupt pin number, -1 if not used. */
    int16_t interrupt_pin;
    /** Interrupt pin requires pull-up.
     * Set to false if external pull up resistor is present.
     */
    bool interrupt_pullup;
};

struct ina228_cfg {
    /** VBus conversion time */
    enum ina228_ct vbusct;
    /** Shunt conversion time */
    enum ina228_ct vshct;
    /** Temperature conversion time */
    enum ina228_ct vtct;
    /** Averaging mode */
    enum ina228_avg_mode avg_mode;
};

/* Define the stats section and records */
STATS_SECT_START(ina228_stat_section)
    STATS_SECT_ENTRY(read_count)
    STATS_SECT_ENTRY(write_count)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

struct ina228_dev {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_i2c_node i2c_node;
#else
    struct os_dev dev;
#endif
    struct sensor sensor;
    /* Hardware wiring config, (pin, shunt, i2c) */
    struct ina228_hw_cfg hw_cfg;

    uint16_t config_reg;
    uint16_t adc_config_reg;
    uint16_t shunt_cal_reg;
    uint16_t shunt_tempco_reg;
    uint16_t diag_alrt_reg;
    uint16_t sovl_reg;
    uint16_t suvl_reg;
    uint16_t bovl_reg;
    uint16_t buvl_reg;
    uint16_t temp_limit_reg;
    uint16_t pwr_limit_reg;
    uint16_t mask_enable_reg;
    STATS_SECT_DECL(ina228_stat_section) stats;

    uint32_t conversion_time;
    struct os_sem conversion_done;
};

/**
 * Write INA228 sensor register over sensor interface
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 * @return 0 on success, non-zero error on failure.
 */
int ina228_write_reg(struct ina228_dev *dev, uint8_t reg, uint16_t reg_val);

/**
 * Read 16 bit register from INA228
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 * @return 0 on success, non-zero error on failure.
 */
int ina228_read_reg(struct ina228_dev *dev, uint8_t reg, uint16_t *reg_val);

/**
 * Read 24 bit register from INA228
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 * @return 0 on success, non-zero error on failure.
 */
int ina228_read_reg24(struct ina228_dev *dev, uint8_t reg, uint32_t *reg_val);

/**
 * Read 32 bit register from INA228
 *
 * @param The sensor interface
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 * @return 0 on success, non-zero error on failure.
 */
int ina228_read_reg32(struct ina228_dev *dev, uint8_t reg, uint32_t *reg_val);

/**
 * Read multiple length data from INA228 sensor over sensor interface
 *
 * @param The Sensor interface
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero error on failure.
 */
int ina228_read(struct ina228_dev *dev, uint8_t *payload, uint32_t len);

/**
 * Initialize the ina228. this function is normally called by sysinit.
 *
 * @param ina228  Pointer to the ina228_dev device descriptor.
 * @param arg     Pointer to struct ina228_hw_cfg.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_init(struct os_dev *ina228, void *arg);

/**
 * Reset sensor.
 *
 * @param ina228    Pointer to the ina228 device.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_reset(struct ina228_dev *ina228);

/**
 * Sets operating mode configuration.
 *
 * @param  ina228      Pointer to the ina228 device.
 * @param  ina228_cfg  Pointer to the ina228 configuration.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_config(struct ina228_dev *ina228, const struct ina228_cfg *cfg);

/**
 * Reads shunt voltage.
 *
 * @param ina228    Pointer to ina228 device.
 * @param voltage   Pointer to shunt voltage in nV.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_read_shunt_voltage(struct ina228_dev *ina228, int32_t *voltage);

/**
 * Reads bus voltage.
 *
 * @param ina228    Pointer to ina228 device.
 * @param voltage   Pointer to output voltage in uV.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_read_bus_voltage(struct ina228_dev *ina228, uint32_t *voltage);

/**
 * Reads shunt voltage and returns current.
 *
 * @param ina228    Pointer to ina228 device.
 * @param current   Pointer to output current in uA.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_read_current(struct ina228_dev *ina228, int32_t *current);

/**
 * Reads manufactured and die id.
 *
 * @param ina228    Pointer to ina228 device.
 * @param mfg       Pointer to manufacturer id.
 * @param die       Pointer to die id.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_read_id(struct ina228_dev *ina228, uint16_t *mfg, uint16_t *die);

/**
 * Start reads current, shunt and bus voltage.
 *
 * Function does not wait for actual conversion.
 * Code should call ina228_wait_and_read() to get results.
 * This can be useful when conversion time is big enough and user
 * code can do other tasks while waiting for conversion to finish.
 *
 * @param ina228    Pointer to ina228 device.
 * @param current   Pointer to output current in uA (can be NULL).
 * @param vbus      Pointer to bus voltage in uV (can be NULL).
 * @param vshunt    Pointer to shunt voltage in nV (can be NULL).
 * @param temp      Pointer to temperature mC (can be NULL).
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_one_shot_read_start(struct ina228_dev *ina228, int32_t *current,
                               uint32_t *vbus, int32_t *vshunt, int32_t *temp);

/**
 * Reads current, shunt and bus voltage.
 * Function reads value in blocking mode.
 * Total time depends on conversion time and averaging mode.
 *
 * @param ina228    Pointer to ina228 device.
 * @param current   Pointer to output current in uA (can be NULL).
 * @param vbus      Pointer to bus voltage in uV (can be NULL).
 * @param vshunt    Pointer to shunt voltage in nV (can be NULL).
 * @param temp      Pointer to temperature mC (can be NULL).
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_one_shot_read(struct ina228_dev *ina228, int32_t *current,
                         uint32_t *vbus, int32_t *vshunt, int32_t *temp);

/**
 * Waits for interrupt to fire and read values.
 * Intended to be used with continuous read mode.
 *
 * @param ina228    Pointer to ina228 device.
 * @param current   Pointer to output current in uA (can be NULL).
 * @param vbus      Pointer to bus voltage in uV (can be NULL).
 * @param vshunt    Pointer to shunt voltage in nV (can be NULL).
 * @param temp      Pointer to temperature in mC (can be NULL).
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_wait_and_read(struct ina228_dev *ina228, int32_t *current,
                         uint32_t *vbus, int32_t *vshunt, int32_t *temp);

/**
 * Start continuous read mode.
 *
 * @param ina228    Pointer to ina228 device.
 * @param mode      shunt and/or vbus mode.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_start_continuous_mode(struct ina228_dev *ina228, enum ina228_oper_mode mode);

/**
 * Stop continuous read mode.
 *
 * @param ina228    Pointer to ina228 device.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina228_stop_continuous_mode(struct ina228_dev *ina228);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
int ina228_create_sensor_dev(struct ina228_dev *ina228, const char *name,
                             const struct bus_i2c_node_cfg *i2c_cfg,
                             const struct ina228_hw_cfg *hw_cfg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __INA228_H__ */
