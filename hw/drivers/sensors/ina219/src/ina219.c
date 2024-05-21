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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include <os/mynewt.h>
#include <hal/hal_i2c.h>
#include <i2cn/i2cn.h>
#include <sensor/sensor.h>
#include <sensor/voltage.h>
#include <sensor/current.h>
#include <modlog/modlog.h>
#include <stats/stats.h>

#include <ina219/ina219.h>

/* Define stat names for querying */
STATS_NAME_START(ina219_stat_section)
    STATS_NAME(ina219_stat_section, read_count)
    STATS_NAME(ina219_stat_section, write_count)
    STATS_NAME(ina219_stat_section, read_errors)
    STATS_NAME(ina219_stat_section, write_errors)
STATS_NAME_END(ina219_stat_section)

/* Exports for the sensor API */
static int ina219_sensor_read(struct sensor *sensor, sensor_type_t typ,
                              sensor_data_func_t data_func, void *data_func_arg, uint32_t timeout);

static int ina219_sensor_get_config(struct sensor *sensor, sensor_type_t typ,
                                    struct sensor_cfg *cfg);

static const struct sensor_driver g_ina219_sensor_driver = {
    .sd_read = ina219_sensor_read,
    .sd_get_config = ina219_sensor_get_config,
};

/* Conversion times depending on ADC settings */
static const uint32_t conversion_times[16] = {
    93, 163, 304, 586, 93, 163, 304, 586, 586, 1060, 2130, 4260, 8510, 17020, 34050, 68100
};

/* Calculate total conversion time in us. */
static uint32_t
ina219_conversion_time(uint32_t config_reg)
{
    uint32_t time;

    if (config_reg & INA219_OPER_SHUNT_VOLTAGE_TRIGGERED) {
        time = conversion_times[(config_reg & INA219_CONF_REG_SADC_Msk) >> INA219_CONF_REG_SADC_Pos];
    } else {
        time = 0;
    }
    if (config_reg & INA219_OPER_BUS_VOLTAGE_TRIGGERED) {
        time += conversion_times[(config_reg & INA219_CONF_REG_BADC_Msk) >> INA219_CONF_REG_BADC_Pos];
    }
    return time;
}

int
ina219_write_reg(struct ina219_dev *ina219, uint8_t reg, uint16_t reg_val)
{
    int rc;
    uint8_t payload[3] = {reg, reg_val >> 8, (uint8_t)reg_val };

    struct hal_i2c_master_data data_struct = {
        .address = ina219->sensor.s_itf.si_addr,
        .len = 3,
        .buffer = payload,
    };

    rc = sensor_itf_lock(&ina219->sensor.s_itf, MYNEWT_VAL(INA219_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }
    STATS_INC(ina219->stats, write_count);
    rc = i2cn_master_write(ina219->sensor.s_itf.si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(INA219_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina219->stats, write_errors);
        INA219_LOG_ERROR("INA219 write I2C failed\n");
    }

    sensor_itf_unlock(&ina219->sensor.s_itf);

    return rc;
}

int
ina219_read_reg(struct ina219_dev *ina219, uint8_t reg, uint16_t *reg_val)
{
    int rc;
    uint8_t payload[2] = { reg };

    struct hal_i2c_master_data data_struct = {
        .address = ina219->sensor.s_itf.si_addr,
        .len = 1,
        .buffer = payload
    };

    rc = sensor_itf_lock(&ina219->sensor.s_itf, MYNEWT_VAL(INA219_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    STATS_INC(ina219->stats, read_count);
    rc = i2cn_master_write(ina219->sensor.s_itf.si_num, &data_struct, OS_TICKS_PER_SEC / 10,
                           1, MYNEWT_VAL(INA219_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina219->stats, read_errors);
        INA219_LOG_ERROR("INA219 write I2C failed\n");
        goto exit;
    }

    data_struct.len = 2;
    rc = i2cn_master_read(ina219->sensor.s_itf.si_num, &data_struct, OS_TICKS_PER_SEC / 10,
                          1, MYNEWT_VAL(INA219_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina219->stats, read_errors);
        INA219_LOG_ERROR("INA219 read I2C failed\n");
    }

    *reg_val = (payload[0] << 8) | payload[1];

exit:
    sensor_itf_unlock(&ina219->sensor.s_itf);

    return rc;
}

int
ina219_read_power_reg(struct ina219_dev *ina219, uint16_t *power_reg)
{
    return ina219_read_reg(ina219, INA219_POWER_REG_ADDR, power_reg);
}

int
ina219_read_configuration_reg(struct ina219_dev *ina219, uint16_t *config_reg)
{
    return ina219_read_reg(ina219, INA219_CONFIGURATION_REG_ADDR, config_reg);
}

int
ina219_read_bus_voltage_reg(struct ina219_dev *ina219, uint16_t *vbus_reg)
{
    return ina219_read_reg(ina219, INA219_BUS_VOLTAGE_REG_ADDR, vbus_reg);
}

int
ina219_read_shunt_voltage_reg(struct ina219_dev *ina219, uint16_t *vshunt_reg)
{
    return ina219_read_reg(ina219, INA219_SHUNT_VOLTAGE_REG_ADDR, vshunt_reg);
}

int
ina219_reset(struct ina219_dev *ina219)
{
    int rc;

    rc = ina219_write_reg(ina219, INA219_CONFIGURATION_REG_ADDR, INA219_CONF_REG_RST_Msk);
    if (rc == SYS_EOK) {
        ina219_read_configuration_reg(ina219, &ina219->config_reg);
    }
    return rc;
}

int
ina219_config(struct ina219_dev *ina219, const struct ina219_cfg *cfg)
{
    int rc;

    /* Start in power down mode. */
    ina219->config_reg =
        (INA219_CONF_REG_SADC_Msk & ((uint16_t) cfg->vshunt_mode << INA219_CONF_REG_SADC_Pos)) |
        (INA219_CONF_REG_BADC_Msk & ((uint16_t) cfg->vbus_mode << INA219_CONF_REG_BADC_Pos)) |
        (INA219_CONF_REG_PG_Msk & ((uint16_t) cfg->vbus_fs << INA219_CONF_REG_PG_Pos));
    rc = ina219_write_reg(ina219, INA219_CONFIGURATION_REG_ADDR, ina219->config_reg);

    if (rc == 0) {
        /* Value 2 is used to enable new value notification, and is not used for anything else. */
        ina219_write_reg(ina219, INA219_CALIBRATION_REG_ADDR, 2);
    }

    return rc;
}

int
ina219_read_bus_voltage(struct ina219_dev *ina219, uint16_t *voltage, bool *conversion_ready)
{
    uint16_t v;
    int rc;

    rc = ina219_read_bus_voltage_reg(ina219, &v);
    if (rc == SYS_EOK) {
        if (ina219->config_reg & INA219_CONF_REG_PG_Msk) {
            *voltage = (v >> 3) * INA219_BUS_VOLTAGE_32V_LSB;
        } else {
            *voltage = (v >> 3) * INA219_BUS_VOLTAGE_16V_LSB;
        }
        if (conversion_ready) {
            *conversion_ready = (v & 2) != 0;
        }
    }
    return rc;
}

int
ina219_read_shunt_voltage(struct ina219_dev *ina219, int32_t *voltage)
{
    int16_t v;
    int rc;

    rc = ina219_read_shunt_voltage_reg(ina219, (uint16_t *)&v);
    if (rc == SYS_EOK) {
        *voltage = v * (int32_t)INA219_SHUNT_VOLTAGE_LSB;
    }
    return rc;
}

int
ina219_read_values(struct ina219_dev *ina219, int32_t *current, uint16_t *vbus, int32_t *vshunt)
{
    int32_t vshunt_uv;
    uint16_t locla_vbus;
    uint16_t power_reg;
    bool conversion_ready;
    int rc = SYS_EOK;

    if (vbus == NULL) {
        vbus = &locla_vbus;
    }
    rc = ina219_read_bus_voltage(ina219, vbus, &conversion_ready);
    if (rc != SYS_EOK) {
        goto exit;
    }
    /* Conversion not ready yet. Don't read shunt voltage. */
    if (!conversion_ready) {
        rc = SYS_EAGAIN;
        goto exit;
    }
    if (current != NULL || vshunt != NULL) {
        rc = ina219_read_shunt_voltage(ina219, &vshunt_uv);
        if (rc != SYS_EOK) {
            goto exit;
        }
        if (vshunt != NULL) {
            *vshunt = vshunt_uv;
        }
        if (current != NULL) {
            *current = 1000 * vshunt_uv / (int32_t)ina219->hw_cfg.shunt_resistance;
        }
    }
    (void)ina219_read_power_reg(ina219, &power_reg);
    (void)power_reg;

exit:
    return rc;
}

static int
ina219_wait_and_read(struct ina219_dev *ina219, int32_t *current, uint16_t *vbus, int32_t *vshunt)
{
    int rc = SYS_EAGAIN;
    int i;

    os_cputime_delay_usecs(ina219->conversion_time);

    for (i = 0; rc == SYS_EAGAIN && i < 10 /* Sanity */; i++) {
        rc = ina219_read_values(ina219, current, vbus, vshunt);
        if (rc == SYS_EAGAIN) {
            /* Wait some more. */
            os_cputime_delay_usecs(100);
        }
    }

    return rc;
}

int
ina219_one_shot_read(struct ina219_dev *ina219, int32_t *current, uint16_t *vbus, int32_t *vshunt)
{
    int rc;

    ina219->config_reg &= ~INA219_CONF_REG_MODE_Msk;
    if (current != NULL || vshunt != NULL) {
        ina219->config_reg |= (uint32_t)INA219_OPER_SHUNT_VOLTAGE_TRIGGERED;
    }
    if (vbus != NULL) {
        ina219->config_reg |= (uint32_t)INA219_OPER_BUS_VOLTAGE_TRIGGERED;
    }
    ina219->conversion_time = ina219_conversion_time(ina219->config_reg);

    /* Start one shot conversion. */
    rc = ina219_write_reg(ina219, INA219_CONFIGURATION_REG_ADDR, ina219->config_reg);
    if (rc != SYS_EOK) {
        goto exit;
    }

    rc = ina219_wait_and_read(ina219, current, vbus, vshunt);
exit:
    return rc;
}

int
ina219_power_down(struct ina219_dev *ina219)
{
    int rc = SYS_EOK;
    uint32_t config_reg;

    config_reg = ina219->config_reg & ~INA219_CONF_REG_MODE_Msk;

    if (config_reg != ina219->config_reg) {
        ina219->config_reg = config_reg;
        rc = ina219_write_reg(ina219, INA219_CONFIGURATION_REG_ADDR, ina219->config_reg);
    }

    return rc;
}

int
ina219_start_continuous_mode(struct ina219_dev *ina219, enum ina219_oper_mode mode)
{
    int rc;

    assert((uint8_t)mode & (uint8_t)INA219_OPER_CONTINUOUS_MODE);
    ina219->config_reg &= ~INA219_CONF_REG_MODE_Msk;
    ina219->config_reg |= mode;

    rc = ina219_write_reg(ina219, INA219_CONFIGURATION_REG_ADDR, ina219->config_reg);

    ina219->conversion_time = ina219_conversion_time(ina219->config_reg);

    return rc;
}

int
ina219_stop_continuous_mode(struct ina219_dev *ina219)
{
    return ina219_power_down(ina219);
}

static int
ina219_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    /* Default values after power on. */
    const struct ina219_cfg default_cfg = {
        .vbus_fs = MYNEWT_VAL(INA219_DEFAULT_VBUS_FULL_SACLE),
        .vbus_mode = MYNEWT_VAL(INA219_DEFAULT_VBUS_ADC_MODE),
        .vshunt_mode = MYNEWT_VAL(INA219_DEFAULT_VSHUNT_ADC_MODE)
    };
    int rc;
    struct ina219_dev *ina219 = (struct ina219_dev *)dev;

    (void)wait;

    /* Reset sensor. */
    rc = ina219_reset(ina219);

    if (rc) {
        goto exit;
    }

    if (arg == NULL) {
        rc = ina219_config(ina219, &default_cfg);
    } else {
        rc = ina219_config(ina219, (const struct ina219_cfg *)arg);
    }
exit:
    return rc;
}

static int
ina219_close(struct os_dev *dev)
{
    struct ina219_dev *ina219 = (struct ina219_dev *)dev;

    return ina219_power_down(ina219);
}

int
ina219_init(struct os_dev *dev, void *arg)
{
    struct ina219_dev *ina219;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto exit;
    }

    ina219 = (struct ina219_dev *)dev;
    ina219->hw_cfg = *(struct ina219_hw_cfg *)arg;

    sensor = &ina219->sensor;

    /* Initialise the stats entry. */
    rc = stats_init(
        STATS_HDR(ina219->stats),
        STATS_SIZE_INIT_PARMS(ina219->stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(ina219_stat_section));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);
    /* Register the entry with the stats registry */
    rc = stats_register(ina219->dev.od_name, STATS_HDR(ina219->stats));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);

    rc = sensor_init(sensor, dev);
    if (rc != SYS_EOK) {
        goto exit;
    }

    (void)sensor_set_interface(sensor, &ina219->hw_cfg.itf);
    (void)sensor_set_type_mask(sensor, SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT);
    (void)sensor_set_driver(sensor, SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT,
                            (struct sensor_driver *)&g_ina219_sensor_driver);

    OS_DEV_SETHANDLERS(dev, ina219_open, ina219_close);
exit:
    return rc;
}

static int
ina219_sensor_read(struct sensor *sensor, sensor_type_t type,
                   sensor_data_func_t data_func, void *data_func_arg, uint32_t timeout)
{
    int rc;
    struct ina219_dev *ina219;
    int32_t current;
    uint16_t vbus;
    int32_t *pcurrent;
    uint16_t *pvbus;
    union {
        struct sensor_voltage_data svd;
        struct sensor_current_data scd;
    } databuf;
    (void)timeout;

    /* If the read isn't looking for bus voltage or current, don't do anything. */
    if (0 == (type & (SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT))) {
        rc = SYS_EINVAL;
        INA219_LOG_ERROR("ina219_sensor_read unsupported sensor type\n");
        goto exit;
    }

    pcurrent = (type & SENSOR_TYPE_CURRENT) ? &current : NULL;
    pvbus = (type & SENSOR_TYPE_VOLTAGE) ? &vbus : NULL;

    ina219 = (struct ina219_dev *)SENSOR_GET_DEVICE(sensor);

    rc = ina219_one_shot_read(ina219, pcurrent, pvbus, NULL);
    if (rc == 0) {
        if (pcurrent) {
            databuf.scd.scd_current = current / 1000000.0f;
            databuf.scd.scd_current_is_valid = 1;
            data_func(sensor, data_func_arg, &databuf.scd, SENSOR_TYPE_CURRENT);
        }
        if (pvbus) {
            /* vbus in mV convert to V. */
            databuf.svd.svd_voltage = vbus / 1000.0f;
            databuf.svd.svd_voltage_is_valid = 1;
            data_func(sensor, data_func_arg, &databuf.scd, SENSOR_TYPE_VOLTAGE);
        }
    }
exit:
    return rc;
}

static int
ina219_sensor_get_config(struct sensor *sensor, sensor_type_t typ,
                         struct sensor_cfg *cfg)
{
    int rc;

    (void)sensor;
    if ((cfg == NULL) || (0 != (typ & (SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT)))) {
        rc = SYS_EINVAL;
    } else {
        cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT;
        rc = SYS_EOK;
    }
    return rc;
}
