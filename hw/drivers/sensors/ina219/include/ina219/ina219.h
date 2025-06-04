/*
 * Copyright 2020 Jesus Ipanienko
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __INA219_H__
#define __INA219_H__

#include <stdint.h>
#include <os/mynewt.h>
#include <stats/stats.h>
#include <sensor/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INA219_CONFIGURATION_REG_ADDR       0
#define INA219_SHUNT_VOLTAGE_REG_ADDR       1
#define INA219_BUS_VOLTAGE_REG_ADDR         2
#define INA219_POWER_REG_ADDR               3
#define INA219_CURRENT_REG_ADDR             4
#define INA219_CALIBRATION_REG_ADDR         5

#define INA219_CONF_REG_RST_Msk             0x8000
#define INA219_CONF_REG_RST_Pos             15
#define INA219_CONF_REG_BRNG_Msk            0x2000
#define INA219_CONF_REG_BRNG_Pos            13
#define INA219_CONF_REG_PG_Msk              0x1800
#define INA219_CONF_REG_PG_Pos              11
#define INA219_CONF_REG_BADC_Msk            0x0780
#define INA219_CONF_REG_BADC_Pos            7
#define INA219_CONF_REG_SADC_Msk            0x0078
#define INA219_CONF_REG_SADC_Pos            3
#define INA219_CONF_REG_MODE_Msk            0x0007
#define INA219_CONF_REG_MODE_Pos            0

/* 10 uV */
#define INA219_SHUNT_VOLTAGE_LSB            10
/* 4 mV at 16V full scale */
#define INA219_BUS_VOLTAGE_16V_LSB          4
/* 8 mV at 32V full scale */
#define INA219_BUS_VOLTAGE_32V_LSB          8

/* INA219 ADC mode/sample */
enum ina219_adc_mode {
    INA219_ADC_9_BITS       = 0,
    INA219_ADC_10_BITS      = 1,
    INA219_ADC_11_BITS      = 2,
    INA219_ADC_12_BITS      = 3,
    INA219_ADC_2_SAMPLES    = 9,
    INA219_ADC_4_SAMPLES    = 10,
    INA219_ADC_8_SAMPLES    = 11,
    INA219_ADC_16_SAMPLES   = 12,
    INA219_ADC_32_SAMPLES   = 13,
    INA219_ADC_64_SAMPLES   = 14,
    INA219_ADC_128_SAMPLES  = 15,
};

/* INA219 operating modes */
enum ina219_oper_mode {
    INA219_OPER_POWER_DOWN                  = 0,
    INA219_OPER_SHUNT_VOLTAGE_TRIGGERED     = 1,
    INA219_OPER_BUS_VOLTAGE_TRIGGERED       = 2,
    INA219_OPER_SHUNT_AND_BUS_TRIGGERED     = 3,
    INA219_OPER_ADC_OFF                     = 4,
    INA219_OPER_CONTINUOUS_MODE             = 4,
    INA219_OPER_SHUNT_VOLTAGE_CONTINUOUS    = 5,
    INA219_OPER_BUS_VOLTAGE_CONTINUOUS      = 6,
    INA219_OPER_SHUNT_AND_BUS_CONTINUOUS    = 7,
};

enum ina219_vbus_full_scale {
    INA219_VBUS_FULL_SCALE_16V              = 0,
    INA219_VBUS_FULL_SCALE_32V              = 1,
};

struct ina219_hw_cfg {
    struct sensor_itf itf;
    /** Shunt resistance in mOhm. */
    uint32_t shunt_resistance;
};

struct ina219_cfg {
    /** VBus adc mode */
    enum ina219_adc_mode vbus_mode;
    /** Shunt adc mode */
    enum ina219_adc_mode vshunt_mode;
    /** Operation mode mode */
    enum ina219_oper_mode oper_mode;
    /** Full scale for VBUS */
    enum ina219_vbus_full_scale vbus_fs;

    uint32_t sensors_mask;
};

/* Define the stats section and records */
STATS_SECT_START(ina219_stat_section)
    STATS_SECT_ENTRY(read_count)
    STATS_SECT_ENTRY(write_count)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

struct ina219_dev {
    struct os_dev dev;
    struct sensor sensor;
    /* Hardware wiring config, (shunt, i2c) */
    struct ina219_hw_cfg hw_cfg;

    uint16_t config_reg;
    STATS_SECT_DECL(ina219_stat_section) stats;

    uint32_t conversion_time;
};

/**
 * Initialize the ina219. this function is normally called by sysinit.
 *
 * @param dev  Pointer to the ina219_dev device descriptor
 * @param arg  Pointer to struct ina219_hw_cfg.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_init(struct os_dev *dev, void *arg);

/**
 * Reset sensor.
 *
 * @param ina219    Pointer to the ina219 device.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_reset(struct ina219_dev *ina219);

/**
 * Sets sensor device configuration.
 *
 * @param  ina219      Pointer to the ina219 struct
 * @param  ina219_cfg  Pointer to the ina219 configuration
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_config(struct ina219_dev *ina219, const struct ina219_cfg *cfg);

/**
 * Reads shunt voltage.
 *
 * @param ina219    Pointer to ina219 device.
 * @param voltage   Pointer to shunt voltage in uV.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_read_shunt_voltage(struct ina219_dev *ina219, int32_t *voltage);

/**
 * Reads bus voltage.
 *
 * @param ina219    Pointer to ina219 device.
 * @param voltage   Pointer to output voltage in mV.
 * @param conversion_ready Pointer to output value set to true if this is new value.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_read_bus_voltage(struct ina219_dev *ina219, uint16_t *voltage, bool *conversion_ready);

/**
 * Reads shunt voltage and returns current.
 *
 * @param ina219    Pointer to ina219 device.
 * @param voltage   Pointer to output current in uA.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_read_current(struct ina219_dev *ina219, int32_t *current);

/**
 * Read current values of current and voltage.
 *
 * @param ina219    Pointer to ina219 device.
 * @param current   Pointer to output current in uA (can be NULL).
 * @param vbus      Pointer to bus voltage in mV (can be NULL).
 * @param vshunt    Pointer to shunt voltage in uV (can be NULL).
 *
 * @return 0 on success, non-zero on failure. SYS_EAGAIN conversion not ready, read again.
 */
int ina219_read_values(struct ina219_dev *ina219, int32_t *current, uint16_t *vbus, int32_t *vshunt);

/**
 * Reads current, shunt and bus voltage.
 *
 * @param ina219    Pointer to ina219 device.
 * @param current   Pointer to output current in uA (can be NULL).
 * @param vbus      Pointer to bus voltage in mV (can be NULL).
 * @param vshunt    Pointer to shunt voltage in uV (can be NULL).
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_one_shot_read(struct ina219_dev *ina219, int32_t *current, uint16_t *vbus, int32_t *vshunt);

/**
 * Start continuous read mode.
 *
 * @param ina219    Pointer to ina219 device.
 * @param mode      shunt and/or vbus mode.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_start_continuous_mode(struct ina219_dev *ina219, enum ina219_oper_mode mode);

/**
 * Stop continuous read mode.
 *
 * @param ina219    Pointer to ina219 device.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina219_stop_continuous_mode(struct ina219_dev *ina219);

#ifdef __cplusplus
}
#endif

#endif /* __INA219_H__ */

