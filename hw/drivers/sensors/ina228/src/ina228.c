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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include <os/mynewt.h>
#include <hal/hal_i2c.h>
#include <i2cn/i2cn.h>
#include <sensor/sensor.h>
#include <sensor/voltage.h>
#include <sensor/current.h>
#include <sensor/temperature.h>

#include <stats/stats.h>
#include <ina228/ina228.h>
#include <hal/hal_gpio.h>

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#define OS_DEV(dev_)   (&dev_->i2c_node.bnode.odev)
#define I2C_ADDR(dev_) (dev_->i2c_node.addr)
#else
#define OS_DEV(dev_)   (&dev_->dev)
#define I2C_ADDR(dev_) (dev_->hw_cfg.itf.si_addr)
#endif

/* Define stat names for querying */
STATS_NAME_START(ina228_stat_section)
    STATS_NAME(ina228_stat_section, read_count)
    STATS_NAME(ina228_stat_section, write_count)
    STATS_NAME(ina228_stat_section, read_errors)
    STATS_NAME(ina228_stat_section, write_errors)
STATS_NAME_END(ina228_stat_section)

/* Exports for the sensor API */
static int ina228_sensor_read(struct sensor *sensor, sensor_type_t typ,
                              sensor_data_func_t data_func,
                              void *data_func_arg, uint32_t timeout);

static int ina228_sensor_get_config(struct sensor *sensor, sensor_type_t typ,
                                    struct sensor_cfg *cfg);

static const struct sensor_driver g_ina228_sensor_driver = {
    .sd_read = ina228_sensor_read,
    .sd_get_config = ina228_sensor_get_config,
};

static uint32_t
bit_field(uint32_t val, uint32_t mask, uint32_t pos)
{
    return (val & mask) >> pos;
}

static uint16_t
ina228_averaging(uint16_t adc_config)
{
    static const uint16_t averaging[8] = { 1, 4, 16, 64, 128, 256, 512, 1024 };
    int ix = bit_field(adc_config, INA228_ADC_CONFIG_AVG_Msk, INA228_ADC_CONFIG_AVG_Pos);

    return averaging[ix];
}

static uint16_t
ina228_conv_time(uint16_t adc_config, uint32_t mask, uint32_t pos)
{
    /* Conversion times depending on CT filed [us] */
    static const uint16_t conversion_times[8] = { 50,  84,   150,  280,
                                                  540, 1052, 2074, 4120 };
    int ix = bit_field(adc_config, mask, pos);

    return conversion_times[ix];
}

uint16_t
ina228_vshunt_conv_time(uint16_t adc_config)
{
    return ina228_conv_time(adc_config, INA228_ADC_CONFIG_VSHCT_Msk,
                            INA228_ADC_CONFIG_VSHCT_Pos);
}

uint16_t
ina228_vbus_conv_time(uint16_t adc_config)
{
    return ina228_conv_time(adc_config, INA228_ADC_CONFIG_VBUSCT_Msk,
                            INA228_ADC_CONFIG_VBUSCT_Pos);
}

uint16_t
ina228_temp_conv_time(uint16_t adc_config)
{
    return ina228_conv_time(adc_config, INA228_ADC_CONFIG_VTCT_Msk,
                            INA228_ADC_CONFIG_VTCT_Pos);
}

/* Calculate total conversion time in us. */
static uint32_t
ina228_conversion_time(uint32_t adc_config)
{
    uint32_t time;

    if (adc_config & INA228_ADC_CONFIG_MODE_VSHUNT) {
        time = ina228_vshunt_conv_time(adc_config);
    } else {
        time = 0;
    }
    if (adc_config & INA228_ADC_CONFIG_MODE_VBUS) {
        time += ina228_vbus_conv_time(adc_config);
    }
    if (adc_config & INA228_ADC_CONFIG_MODE_TEMP) {
        time += ina228_temp_conv_time(adc_config);
    }
    /* Multiple bus and shunt conversion time by averaging count */
    time *= ina228_averaging(adc_config);
    return time;
}

#if MYNEWT_VAL(INA228_INT_SUPPORT)

static void
ina228_irq_handler(void *arg)
{
    struct ina228_dev *ina228 = (struct ina228_dev *)arg;

    int sr;
    OS_ENTER_CRITICAL(sr);
    if (os_sem_get_count(&ina228->conversion_done) == 0) {
        os_sem_release(&ina228->conversion_done);
    }
    OS_EXIT_CRITICAL(sr);
}

static int
ina228_init_interrupt(struct ina228_dev *ina228)
{
    int pin;
    bool pullup;
    int rc = OS_OK;

    pin = ina228->hw_cfg.interrupt_pin;
    pullup = ina228->hw_cfg.interrupt_pullup;

    if (pin < 0) {
        goto exit;
    }

    rc = hal_gpio_irq_init(pin, ina228_irq_handler, ina228, HAL_GPIO_TRIG_FALLING,
                           pullup ? HAL_GPIO_PULL_UP : HAL_GPIO_PULL_NONE);
    assert(rc == 0);
exit:
    return rc;
}

static void
disable_interrupt(struct ina228_dev *ina228)
{
    if (ina228->hw_cfg.interrupt_pin >= 0) {
        hal_gpio_irq_disable(ina228->hw_cfg.interrupt_pin);
    }
}

static void
enable_interrupt(struct ina228_dev *ina228)
{
    if (ina228->hw_cfg.interrupt_pin >= 0) {
        hal_gpio_irq_enable(ina228->hw_cfg.interrupt_pin);
        if ((ina228->diag_alrt_reg & INA228_DIAG_ALRT_CNRV) == 0) {
            ina228->diag_alrt_reg |= INA228_DIAG_ALRT_CNRV;
            ina228_write_reg(ina228, INA228_DIAG_ALRT_REG_ADDR, ina228->diag_alrt_reg);
        }
    }
}

#else
#define enable_interrupt(ina228)      (void)ina228
#define disable_interrupt(ina228)     (void)ina228
#define ina228_init_interrupt(ina228) 0
#endif

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
int
ina228_write_reg(struct ina228_dev *ina228, uint8_t reg, uint16_t reg_val)
{
    int rc;
    uint8_t payload[3] = { reg, reg_val >> 8, (uint8_t)reg_val };

    STATS_INC(ina228->stats, write_count);
    rc = bus_node_simple_write(OS_DEV(ina228), payload, sizeof(payload));
    if (rc) {
        STATS_INC(ina228->stats, write_errors);
        INA228_LOG_ERROR("INA228 write I2C failed\n");
    }

    return rc;
}

int
ina228_read_reg_buf(struct ina228_dev *ina228, uint8_t reg, uint8_t *buf, size_t buf_size)
{
    int rc;

    STATS_INC(ina228->stats, read_count);
    rc = bus_node_simple_write_read_transact(OS_DEV(ina228), &reg, 1, buf, buf_size);
    if (rc) {
        STATS_INC(ina228->stats, read_errors);
        INA228_LOG_ERROR("INA228 read I2C failed\n");
    }

    return rc;
}

#else
int
ina228_write_reg_buf(struct ina228_dev *ina228, uint8_t reg,
                     const uint8_t *buf, size_t buf_size)
{
    int rc;
    uint8_t payload[buf_size + 1] = { reg, reg_val >> 8, (uint8_t)reg_val };

    struct hal_i2c_master_data data_struct = {
        .address = ina228->sensor.s_itf.si_addr,
        .len = 3,
        .buffer = payload,
    };

    rc = sensor_itf_lock(&ina228->sensor.s_itf, MYNEWT_VAL(INA228_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }
    STATS_INC(ina228->stats, write_count);
    rc = i2cn_master_write(ina228->sensor.s_itf.si_num, &data_struct,
                           OS_TICKS_PER_SEC / 10, 1, MYNEWT_VAL(INA228_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina228->stats, write_errors);
        INA228_LOG_ERROR("INA228 write I2C failed\n");
    }

    sensor_itf_unlock(&ina228->sensor.s_itf);

    return rc;
}

int
ina228_read_reg_buf(struct ina228_dev *ina228, uint8_t reg, uint8_t *buf, size_t buf_size)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = ina228->sensor.s_itf.si_addr,
        .len = 1,
        .buffer = &reg
    };

    rc = sensor_itf_lock(&ina228->sensor.s_itf, MYNEWT_VAL(INA228_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    STATS_INC(ina228->stats, read_count);
    rc = i2cn_master_write(ina228->sensor.s_itf.si_num, &data_struct,
                           OS_TICKS_PER_SEC / 10, 1, MYNEWT_VAL(INA228_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina228->stats, read_errors);
        INA228_LOG_ERROR("INA228 write I2C failed\n");
        goto exit;
    }

    data_struct.buffer = buf;
    data_struct.len = buf_size;
    rc = i2cn_master_read(ina228->sensor.s_itf.si_num, &data_struct,
                          OS_TICKS_PER_SEC / 10, 1, MYNEWT_VAL(INA228_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina228->stats, read_errors);
        INA228_LOG_ERROR("INA228 read I2C failed\n");
    }

exit:
    sensor_itf_unlock(&ina228->sensor.s_itf);

    return rc;
}

#endif

int
ina228_read_reg(struct ina228_dev *ina228, uint8_t reg, uint16_t *reg_val)
{
    int rc;
    uint8_t buf[2];

    rc = ina228_read_reg_buf(ina228, reg, buf, sizeof(buf));

    if (rc == 0) {
        *reg_val = (buf[0] << 8) | buf[1];
    }

    return rc;
}

int
ina228_read_reg24(struct ina228_dev *ina228, uint8_t reg, uint32_t *reg_val)
{
    int rc;
    uint8_t buf[3];

    rc = ina228_read_reg_buf(ina228, reg, buf, sizeof(buf));

    if (rc == 0) {
        *reg_val = (buf[0] << 16) | (buf[1] << 8) | buf[2];
    }

    return rc;
}

int
ina228_reset(struct ina228_dev *ina228)
{
    int rc;

    rc = ina228_write_reg(ina228, INA228_CONFIG_REG_ADDR, INA228_CONFIG_RST_Msk);
    if (rc == SYS_EOK) {
        ina228_read_reg(ina228, INA228_CONFIG_REG_ADDR, &ina228->config_reg);
        ina228_read_reg(ina228, INA228_ADC_CONFIG_REG_ADDR, &ina228->adc_config_reg);
        ina228_read_reg(ina228, INA228_SHUNT_CAL_REG_ADDR, &ina228->shunt_cal_reg);
        ina228_read_reg(ina228, INA228_SHUNT_TEMPCO_REG_ADDR, &ina228->shunt_tempco_reg);
        ina228_read_reg(ina228, INA228_DIAG_ALRT_REG_ADDR, &ina228->diag_alrt_reg);
    }
    return rc;
}

int
ina228_config(struct ina228_dev *ina228, const struct ina228_cfg *cfg)
{
    int rc;

    /* Start in power down mode. */
    ina228->adc_config_reg =
        (INA228_ADC_CONFIG_VSHCT_Msk &
         ((uint16_t)cfg->vshct << INA228_ADC_CONFIG_VSHCT_Pos)) |
        (INA228_ADC_CONFIG_VBUSCT_Msk &
         ((uint16_t)cfg->vbusct << INA228_ADC_CONFIG_VBUSCT_Pos)) |
        (INA228_ADC_CONFIG_VTCT_Msk &
         ((uint16_t)cfg->vtct << INA228_ADC_CONFIG_VTCT_Pos)) |
        (INA228_ADC_CONFIG_MODE_Msk &
         ((uint16_t)cfg->avg_mode << INA228_ADC_CONFIG_MODE_Pos));

    rc = ina228_write_reg(ina228, INA228_ADC_CONFIG_REG_ADDR, ina228->adc_config_reg);

    return rc;
}

int
ina228_read_bus_voltage(struct ina228_dev *ina228, uint32_t *voltage)
{
    uint32_t v;
    int rc;

    rc = ina228_read_reg24(ina228, INA228_VBUS_REG_ADDR, &v);
    if (rc == SYS_EOK) {
        /* Extract bits 23-4 */
        v >>= 4;
        if (v > 16384) {
            *voltage = v * (INA228_BUS_VOLTAGE_LSB / 1000);
        } else {
            *voltage = (v * INA228_BUS_VOLTAGE_LSB) / 1000;
        }
    }
    return rc;
}

int
ina228_read_temperature(struct ina228_dev *ina228, int32_t *temp)
{
    int16_t t;
    int rc;

    rc = ina228_read_reg(ina228, INA228_DIETEMP_REG_ADDR, (uint16_t *)&t);
    if (rc == SYS_EOK) {
        if (abs(t) > 16384) {
            *temp = t * (INA228_TEMPERATURE_LSB / 1000);
        } else {
            *temp = (t * INA228_TEMPERATURE_LSB) / 1000;
        }
    }
    return rc;
}

int
ina228_read_shunt_voltage(struct ina228_dev *ina228, int32_t *voltage)
{
    int32_t v;
    int rc;

    rc = ina228_read_reg24(ina228, INA228_VSHUNT_REG_ADDR, (uint32_t *)&v);
    if (rc == SYS_EOK) {
        /* Extract bits 23-4 with sign extension */
        v = (v << 8) >> 12;
        if (abs(v) > 4096) {
            if (ina228->config_reg & INA228_CONFIG_ADCRANGE_Msk) {
                v = v * ((int32_t)INA228_SHUNT_VOLTAGE_1_LSB / 1000);
            } else {
                v = v * ((int32_t)INA228_SHUNT_VOLTAGE_0_LSB / 1000);
            }
        } else {
            if (ina228->config_reg & INA228_CONFIG_ADCRANGE_Msk) {
                v = (v * (int32_t)INA228_SHUNT_VOLTAGE_1_LSB) / 1000;
            } else {
                v = v * ((int32_t)INA228_SHUNT_VOLTAGE_0_LSB) / 1000;
            }
        }
        *voltage = v;
    }
    return rc;
}

int
ina228_read_diag_alert(struct ina228_dev *ina228, uint16_t *diag_alert)
{
    return ina228_read_reg(ina228, INA228_DIAG_ALRT_REG_ADDR, diag_alert);
}

int
ina228_conversion_ready(struct ina228_dev *ina228)
{
    int rc;
    uint16_t diag_alert;

    /* Clear interrupt by reading mask/enable register */
    rc = ina228_read_diag_alert(ina228, &diag_alert);
    if (rc == SYS_EOK) {
        rc = diag_alert & INA228_DIAG_ALRT_CNVRF ? SYS_EOK : SYS_EBUSY;
    }
    return rc;
}

int
ina228_read_values(struct ina228_dev *ina228, int32_t *current, uint32_t *vbus,
                   int32_t *vshunt, int32_t *temp)
{
    int32_t vshunt_nv;
    int rc = SYS_EOK;

    if (current != NULL || vshunt != NULL) {
        rc = ina228_read_shunt_voltage(ina228, &vshunt_nv);
        if (rc != SYS_EOK) {
            goto exit;
        }
        if (vshunt != NULL) {
            *vshunt = vshunt_nv;
        }
        if (current != NULL) {
            *current = vshunt_nv / (int32_t)ina228->hw_cfg.shunt_resistance;
        }
    }
    if (vbus != NULL) {
        rc = ina228_read_bus_voltage(ina228, vbus);
        if (rc != SYS_EOK) {
            goto exit;
        }
    }
    if (temp != NULL) {
        rc = ina228_read_temperature(ina228, temp);
        if (rc != SYS_EOK) {
            goto exit;
        }
    }

exit:
    return rc;
}

int
ina228_wait_and_read(struct ina228_dev *ina228, int32_t *current,
                     uint32_t *vbus, int32_t *vshunt, int32_t *temp)
{
    int rc;

    if (ina228 == NULL) {
        return SYS_EINVAL;
    }
    if (MYNEWT_VAL(INA228_INT_SUPPORT) && ina228->hw_cfg.interrupt_pin >= 0) {
        os_sem_pend(&ina228->conversion_done,
                    2 * (1 + os_time_ms_to_ticks32(ina228->conversion_time / 1000)));
    } else {
        os_cputime_delay_usecs(ina228->conversion_time);
    }

    do {
        rc = ina228_conversion_ready(ina228);
    } while (rc == SYS_EBUSY);

    if (rc == SYS_EOK) {
        rc = ina228_read_values(ina228, current, vbus, vshunt, temp);
    }
    return rc;
}

int
ina228_one_shot_read_start(struct ina228_dev *ina228, int32_t *current,
                           uint32_t *vbus, int32_t *vshunt, int32_t *temp)
{
    int rc;

    ina228->adc_config_reg &= ~INA228_ADC_CONFIG_MODE_Msk;

    if (current != NULL || vshunt != NULL) {
        ina228->adc_config_reg |= INA228_ADC_CONFIG_MODE_VSHUNT;
    }
    if (vbus != NULL) {
        ina228->adc_config_reg |= INA228_ADC_CONFIG_MODE_VBUS;
    }
    if (temp != NULL) {
        ina228->adc_config_reg |= INA228_ADC_CONFIG_MODE_TEMP;
    }
    ina228->conversion_time = ina228_conversion_time(ina228->adc_config_reg);

    os_sem_pend(&ina228->conversion_done, 0);

    /* Start one shot conversion. */
    rc = ina228_write_reg(ina228, INA228_ADC_CONFIG_REG_ADDR, ina228->adc_config_reg);

    return rc;
}

int
ina228_one_shot_read(struct ina228_dev *ina228, int32_t *current,
                     uint32_t *vbus, int32_t *vshunt, int32_t *temp)
{
    int rc;

    rc = ina228_one_shot_read_start(ina228, current, vbus, vshunt, temp);

    if (rc == SYS_EOK) {
        rc = ina228_wait_and_read(ina228, current, vbus, vshunt, temp);
    }

    return rc;
}

int
ina228_power_down(struct ina228_dev *ina228)
{
    int rc = SYS_EOK;
    uint32_t adc_config_reg;

    adc_config_reg = ina228->config_reg & ~INA228_ADC_CONFIG_MODE_Msk;

    if (adc_config_reg != ina228->config_reg) {
        ina228->adc_config_reg = adc_config_reg;
        rc = ina228_write_reg(ina228, INA228_ADC_CONFIG_REG_ADDR,
                              ina228->adc_config_reg);
        (void)ina228_conversion_ready(ina228);
    }

    return rc;
}

int
ina228_start_continuous_mode(struct ina228_dev *ina228, enum ina228_oper_mode mode)
{
    int rc;

    assert((uint8_t)mode & (uint8_t)INA228_MODE_CONTINUOUS);
    ina228->adc_config_reg &= ~INA228_ADC_CONFIG_MODE_Msk;
    ina228->adc_config_reg |= (mode << INA228_ADC_CONFIG_MODE_Pos);

    /* Read status register */
    (void)ina228_conversion_ready(ina228);
    os_sem_pend(&ina228->conversion_done, 0);

    rc = ina228_write_reg(ina228, INA228_ADC_CONFIG_REG_ADDR, ina228->adc_config_reg);

    ina228->conversion_time = ina228_conversion_time(ina228->adc_config_reg);

    return rc;
}

int
ina228_stop_continuous_mode(struct ina228_dev *ina228)
{
    return ina228_power_down(ina228);
}

int
ina228_read_id(struct ina228_dev *ina228, uint16_t *mfg, uint16_t *die)
{
    int rc;

    rc = ina228_read_reg(ina228, INA228_MANUFACTURER_ID_REG_ADDR, mfg);
    if (rc == SYS_EOK) {
        rc = ina228_read_reg(ina228, INA228_DEVICE_ID_REG_ADDR, die);
    }
    return rc;
}

static int
ina228_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    /* Default values after power on. */
    const struct ina228_cfg default_cfg = {
        .avg_mode = MYNEWT_VAL_INA228_DEFAULT_AVERAGING,
        .vbusct = MYNEWT_VAL_INA228_DEFAULT_VBUS_CONVERSION_TIME,
        .vshct = MYNEWT_VAL_INA228_DEFAULT_VSHUNT_CONVERSION_TIME,
        .vtct = MYNEWT_VAL_INA228_DEFAULT_TEMPERATURE_CONVERSION_TIME,
    };
    int rc;
    uint16_t mfg;
    uint16_t die;
    struct ina228_dev *ina228 = (struct ina228_dev *)dev;

    (void)wait;

    /* Reset sensor. */
    rc = ina228_reset(ina228);

    if (rc) {
        goto exit;
    }

    /* Verify sensor ID. */
    rc = ina228_read_id(ina228, &mfg, &die);
    if (rc != SYS_EOK) {
        goto exit;
    }
    if (mfg != INA228_MANUFACTURER_ID) {
        INA228_LOG_ERROR("INA228 read ID failed, no INA228 at 0x%X, found 0x%X 0x%X\n",
                         I2C_ADDR(ina228), mfg, die);
        rc = SYS_ENODEV;
        goto exit;
    }

    if (arg == NULL) {
        rc = ina228_config(ina228, &default_cfg);
    } else {
        rc = ina228_config(ina228, (const struct ina228_cfg *)arg);
    }
    if (rc == SYS_EOK) {
        enable_interrupt(ina228);
    }
exit:
    return rc;
}

static int
ina228_close(struct os_dev *dev)
{
    struct ina228_dev *ina228 = (struct ina228_dev *)dev;

    disable_interrupt(ina228);
    return ina228_power_down(ina228);
}

int
ina228_init(struct os_dev *dev, void *arg)
{
    struct ina228_dev *ina228;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto exit;
    }

    ina228 = (struct ina228_dev *)dev;
    ina228->hw_cfg = *(struct ina228_hw_cfg *)arg;
    os_sem_init(&ina228->conversion_done, 0);

    sensor = &ina228->sensor;

    /* Initialise the stats entry. */
    rc = stats_init(STATS_HDR(ina228->stats),
                    STATS_SIZE_INIT_PARMS(ina228->stats, STATS_SIZE_32),
                    STATS_NAME_INIT_PARMS(ina228_stat_section));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);
    /* Register the entry with the stats registry */
    rc = stats_register(OS_DEV(ina228)->od_name, STATS_HDR(ina228->stats));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);

    rc = ina228_init_interrupt(ina228);
    if (rc != SYS_EOK) {
        goto exit;
    }

    rc = sensor_init(sensor, dev);
    if (rc != SYS_EOK) {
        goto exit;
    }

    (void)sensor_set_interface(sensor, &ina228->hw_cfg.itf);
    (void)sensor_set_type_mask(sensor, SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT);
    (void)sensor_set_driver(sensor, SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT,
                            (struct sensor_driver *)&g_ina228_sensor_driver);

    OS_DEV_SETHANDLERS(dev, ina228_open, ina228_close);
exit:
    return rc;
}

static int
ina228_sensor_read(struct sensor *sensor, sensor_type_t type,
                   sensor_data_func_t data_func, void *data_func_arg, uint32_t timeout)
{
    int rc;
    struct ina228_dev *ina228;
    int32_t current;
    uint32_t vbus;
    int32_t temp;
    int32_t *pcurrent;
    uint32_t *pvbus;
    int32_t *ptemp;
    union {
        struct sensor_voltage_data svd;
        struct sensor_current_data scd;
        struct sensor_temp_data std;
    } databuf;
    (void)timeout;

    /* If the read isn't looking for bus voltage or current, don't do anything. */
    if (0 == (type & (SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT))) {
        rc = SYS_EINVAL;
        INA228_LOG_ERROR("ina228_sensor_read unsupported sensor type\n");
        goto exit;
    }

    pcurrent = (type & SENSOR_TYPE_CURRENT) ? &current : NULL;
    pvbus = (type & SENSOR_TYPE_VOLTAGE) ? &vbus : NULL;
    ptemp = (type & SENSOR_TYPE_TEMPERATURE) ? &temp : NULL;

    ina228 = (struct ina228_dev *)SENSOR_GET_DEVICE(sensor);

    rc = ina228_one_shot_read(ina228, pcurrent, pvbus, NULL, ptemp);
    if (rc == 0) {
        if (pcurrent) {
            databuf.scd.scd_current = current / 1000000.0f;
            databuf.scd.scd_current_is_valid = 1;
            data_func(sensor, data_func_arg, &databuf.scd, SENSOR_TYPE_CURRENT);
        }
        if (pvbus) {
            /* vbus in uV convert to V. */
            databuf.svd.svd_voltage = vbus / 1000000.0f;
            databuf.svd.svd_voltage_is_valid = 1;
            data_func(sensor, data_func_arg, &databuf.scd, SENSOR_TYPE_VOLTAGE);
        }
        if (ptemp) {
            /* temperature in mC convert to C. */
            databuf.std.std_temp = temp / 1000000.0f;
            databuf.std.std_temp_is_valid = 1;
            data_func(sensor, data_func_arg, &databuf.std, SENSOR_TYPE_TEMPERATURE);
        }
    }
exit:
    return rc;
}

static int
ina228_sensor_get_config(struct sensor *sensor, sensor_type_t typ,
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

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)

static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    ina228_init(&bnode->odev, arg);
}

int
ina228_create_sensor_dev(struct ina228_dev *ina228, const char *name,
                         const struct bus_i2c_node_cfg *i2c_cfg,
                         const struct ina228_hw_cfg *hw_cfg)
{
    int rc;
    static struct sensor_itf itf;

    rc = sensor_create_i2c_device(&ina228->i2c_node, name, i2c_cfg,
                                  init_node_cb, (void *)hw_cfg, &itf);

    return rc;
}
#endif
