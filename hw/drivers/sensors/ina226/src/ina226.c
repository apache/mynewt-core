#include <string.h>
#include <errno.h>
#include <assert.h>

#include <os/mynewt.h>
#include <hal/hal_i2c.h>
#include <i2cn/i2cn.h>
#include <sensor/sensor.h>
#include <sensor/voltage.h>
#include <sensor/current.h>
#include <stats/stats.h>

#include <ina226/ina226.h>
#include <hal/hal_gpio.h>

/* Define stat names for querying */
STATS_NAME_START(ina226_stat_section)
    STATS_NAME(ina226_stat_section, read_count)
    STATS_NAME(ina226_stat_section, write_count)
    STATS_NAME(ina226_stat_section, read_errors)
    STATS_NAME(ina226_stat_section, write_errors)
STATS_NAME_END(ina226_stat_section)

/* Exports for the sensor API */
static int ina226_sensor_read(struct sensor *sensor, sensor_type_t typ,
                              sensor_data_func_t data_func, void *data_func_arg, uint32_t timeout);

static int ina226_sensor_get_config(struct sensor *sensor, sensor_type_t typ,
                                    struct sensor_cfg *cfg);

static const struct sensor_driver g_ina226_sensor_driver = {
    .sd_read = ina226_sensor_read,
    .sd_get_config = ina226_sensor_get_config,
};

/* Conversion times depending on CT filed [us] */
static const uint16_t conversion_times[8] = { 154, 224, 365, 646, 1210, 2328, 4572, 9068 };
static const uint16_t averaging[8] = { 1, 4, 16, 64, 128, 256, 512, 1024 };

/* Calculate total conversion time in us. */
static uint32_t
ina226_conversion_time(uint32_t config_reg)
{
    uint32_t time;

    if (config_reg & INA226_OPER_SHUNT_VOLTAGE_TRIGGERED) {
        time = conversion_times[(config_reg & INA226_CONF_VSHCT_Msk) >> INA226_CONF_VSHCT_Pos];
    } else {
        time = 0;
    }
    if (config_reg & INA226_OPER_BUS_VOLTAGE_TRIGGERED) {
        time += conversion_times[(config_reg & INA226_CONF_VBUSCT_Msk) >> INA226_CONF_VBUSCT_Pos];
    }
    /* Multiple bus and shunt conversion time by averaging count */
    time *= averaging[(config_reg & INA226_CONF_AVG_Msk) >> INA226_CONF_AVG_Pos];
    return time;
}

#if MYNEWT_VAL(INA226_INT_SUPPORT)

static void
ina226_irq_handler(void *arg)
{
    struct ina226_dev *ina226 = (struct ina226_dev *)arg;

    int sr;
    OS_ENTER_CRITICAL(sr);
    if (os_sem_get_count(&ina226->conversion_done) == 0) {
        os_sem_release(&ina226->conversion_done);
    }
    OS_EXIT_CRITICAL(sr);
}

static int
ina226_init_interrupt(struct ina226_dev *ina226)
{
    int pin;
    bool pullup;
    int rc = OS_OK;

    pin = ina226->hw_cfg.interrupt_pin;
    pullup = ina226->hw_cfg.interrupt_pullup;

    if (pin < 0) {
        goto exit;
    }

    rc = hal_gpio_irq_init(pin,
                           ina226_irq_handler,
                           ina226,
                           HAL_GPIO_TRIG_FALLING,
                           pullup ? HAL_GPIO_PULL_UP : HAL_GPIO_PULL_NONE);
    assert(rc == 0);
exit:
    return rc;
}

static void
disable_interrupt(struct ina226_dev *ina226)
{
    if (ina226->hw_cfg.interrupt_pin >= 0) {
        hal_gpio_irq_disable(ina226->hw_cfg.interrupt_pin);
    }
}

static void
enable_interrupt(struct ina226_dev *ina226)
{
    if (ina226->hw_cfg.interrupt_pin >= 0) {
        hal_gpio_irq_enable(ina226->hw_cfg.interrupt_pin);
    }
}

#else
#define enable_interrupt(i) (void)i
#define disable_interrupt(i) (void)i
#define ina226_init_interrupt(i) 0
#endif

int
ina226_write_reg(struct ina226_dev *ina226, uint8_t reg, uint16_t reg_val)
{
    int rc;
    uint8_t payload[3] = {reg, reg_val >> 8, (uint8_t)reg_val };

    struct hal_i2c_master_data data_struct = {
        .address = ina226->sensor.s_itf.si_addr,
        .len = 3,
        .buffer = payload,
    };

    rc = sensor_itf_lock(&ina226->sensor.s_itf, MYNEWT_VAL(INA226_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }
    STATS_INC(ina226->stats, write_count);
    rc = i2cn_master_write(ina226->sensor.s_itf.si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1,
                           MYNEWT_VAL(INA226_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina226->stats, write_errors);
        INA226_LOG_ERROR("INA226 write I2C failed\n");
    }

    sensor_itf_unlock(&ina226->sensor.s_itf);

    return rc;
}

int
ina226_read_reg(struct ina226_dev *ina226, uint8_t reg, uint16_t *reg_val)
{
    int rc;
    uint8_t payload[2] = { reg };

    struct hal_i2c_master_data data_struct = {
        .address = ina226->sensor.s_itf.si_addr,
        .len = 1,
        .buffer = payload
    };

    rc = sensor_itf_lock(&ina226->sensor.s_itf, MYNEWT_VAL(INA226_ITF_LOCK_TMO));
    if (rc) {
        return rc;
    }

    STATS_INC(ina226->stats, read_count);
    rc = i2cn_master_write(ina226->sensor.s_itf.si_num, &data_struct, OS_TICKS_PER_SEC / 10,
                           1, MYNEWT_VAL(INA226_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina226->stats, read_errors);
        INA226_LOG_ERROR("INA226 write I2C failed\n");
        goto exit;
    }

    data_struct.len = 2;
    rc = i2cn_master_read(ina226->sensor.s_itf.si_num, &data_struct, OS_TICKS_PER_SEC / 10,
                          1, MYNEWT_VAL(INA226_I2C_RETRIES));
    if (rc) {
        STATS_INC(ina226->stats, read_errors);
        INA226_LOG_ERROR("INA226 read I2C failed\n");
    }

    *reg_val = (payload[0] << 8) | payload[1];

exit:
    sensor_itf_unlock(&ina226->sensor.s_itf);

    return rc;
}

int
ina226_reset(struct ina226_dev *ina226)
{
    int rc;

    rc = ina226_write_reg(ina226, INA226_CONFIGURATION_REG_ADDR, INA226_CONF_RST_Msk);
    if (rc == SYS_EOK) {
        ina226_read_reg(ina226, INA226_CONFIGURATION_REG_ADDR, &ina226->config_reg);
    }
    return rc;
}

int
ina226_config(struct ina226_dev *ina226, const struct ina226_cfg *cfg)
{
    int rc;

    /* Start in power down mode. */
    ina226->config_reg =
        (INA226_CONF_VSHCT_Msk & ((uint16_t) cfg->vshct << INA226_CONF_VSHCT_Pos)) |
        (INA226_CONF_VBUSCT_Msk & ((uint16_t) cfg->vbusct << INA226_CONF_VBUSCT_Pos)) |
        (INA226_CONF_AVG_Msk & ((uint16_t) cfg->avg_mode << INA226_CONF_AVG_Pos));
    rc = ina226_write_reg(ina226, INA226_CONFIGURATION_REG_ADDR, ina226->config_reg);

    ina226->mask_enable_reg = INA226_MASK_ENABLE_CNVR;

    if (rc == SYS_EOK) {
        rc = ina226_write_reg(ina226, INA226_MASK_ENABLE_REG_ADDR, ina226->mask_enable_reg);
    }
    return rc;
}

int
ina226_read_bus_voltage(struct ina226_dev *ina226, uint32_t *voltage)
{
    uint16_t v;
    int rc;

    rc = ina226_read_reg(ina226, INA226_BUS_VOLTAGE_REG_ADDR, &v);
    if (rc == SYS_EOK) {
        *voltage = v * INA226_BUS_VOLTAGE_LSB;
    }
    return rc;
}

int
ina226_read_shunt_voltage(struct ina226_dev *ina226, int32_t *voltage)
{
    int16_t v;
    int rc;

    rc = ina226_read_reg(ina226, INA226_SHUNT_VOLTAGE_REG_ADDR, (uint16_t *)&v);
    if (rc == SYS_EOK) {
        *voltage = v * (int32_t)INA226_SHUNT_VOLTAGE_LSB;
    }
    return rc;
}

int
ina226_read_mask_enable(struct ina226_dev *ina226, uint16_t *mask_enable)
{
    return ina226_read_reg(ina226, INA226_MASK_ENABLE_REG_ADDR, mask_enable);
}

int
ina226_conversion_ready(struct ina226_dev *ina226)
{
    int rc;
    uint16_t mask_enable;

    /* Clear interrupt by reading mask/enable register */
    rc = ina226_read_mask_enable(ina226, &mask_enable);
    if (rc == SYS_EOK) {
        rc = mask_enable & INA226_MASK_ENABLE_CVRF ? SYS_EOK : SYS_EBUSY;
    }
    return rc;
}

int
ina226_read_values(struct ina226_dev *ina226, int32_t *current, uint32_t *vbus, int32_t *vshunt)
{
    int32_t vshunt_nv;
    int rc = SYS_EOK;

    if (current != NULL || vshunt != NULL) {
        rc = ina226_read_shunt_voltage(ina226, &vshunt_nv);
        if (rc != SYS_EOK) {
            goto exit;
        }
        if (vshunt != NULL) {
            *vshunt = vshunt_nv;
        }
        if (current != NULL) {
            *current = vshunt_nv / (int32_t)ina226->hw_cfg.shunt_resistance;
        }
    }
    if (vbus != NULL) {
        rc = ina226_read_bus_voltage(ina226, vbus);
        if (rc != SYS_EOK) {
            goto exit;
        }
    }

exit:
    return rc;
}

int
ina226_wait_and_read(struct ina226_dev *ina226, int32_t *current, uint32_t *vbus, int32_t *vshunt)
{
    int rc;

#if MYNEWT_VAL(INA226_INT_SUPPORT)
    if (ina226->hw_cfg.interrupt_pin >= 0) {
        os_sem_pend(&ina226->conversion_done, 2 * (1 + os_time_ms_to_ticks32(ina226->conversion_time / 1000)));
    } else {
        os_cputime_delay_usecs(ina226->conversion_time);
    }
#else
    os_cputime_delay_usecs(ina226->conversion_time);
#endif

    /* Clear interrupt. */
    rc = ina226_conversion_ready(ina226);
    if (rc == SYS_EOK) {
        rc = ina226_read_values(ina226, current, vbus, vshunt);
    }
    return rc;
}

int
ina226_one_shot_read(struct ina226_dev *ina226, int32_t *current, uint32_t *vbus, int32_t *vshunt)
{
    int rc;

    ina226->config_reg &= ~INA226_CONF_OPER_MODE_Msk;
    if (current != NULL || vshunt != NULL) {
        ina226->config_reg |= (uint32_t)INA226_OPER_SHUNT_VOLTAGE_TRIGGERED;
    }
    if (vbus != NULL) {
        ina226->config_reg |= (uint32_t)INA226_OPER_BUS_VOLTAGE_TRIGGERED;
    }
    ina226->conversion_time = ina226_conversion_time(ina226->config_reg);

    os_sem_pend(&ina226->conversion_done, 0);

    /* Start one shot conversion. */
    rc = ina226_write_reg(ina226, INA226_CONFIGURATION_REG_ADDR, ina226->config_reg);
    if (rc != SYS_EOK) {
        goto exit;
    }

    ina226_wait_and_read(ina226, current, vbus, vshunt);
exit:
    return rc;
}

int
ina226_power_down(struct ina226_dev *ina226)
{
    int rc = SYS_EOK;
    uint32_t config_reg;

    config_reg = ina226->config_reg & ~INA226_CONF_OPER_MODE_Msk;

    if (config_reg != ina226->config_reg) {
        ina226->config_reg = config_reg;
        rc = ina226_write_reg(ina226, INA226_CONFIGURATION_REG_ADDR, ina226->config_reg);
        (void)ina226_conversion_ready(ina226);
    }

    return rc;
}

int
ina226_start_continuous_mode(struct ina226_dev *ina226, enum ina226_oper_mode mode)
{
    int rc;

    assert((uint8_t)mode & (uint8_t)INA226_OPER_CONTINUOUS_MODE);
    ina226->config_reg &= ~INA226_CONF_OPER_MODE_Msk;
    ina226->config_reg |= mode;

    (void)ina226_conversion_ready(ina226);
    os_sem_pend(&ina226->conversion_done, 0);

    rc = ina226_write_reg(ina226, INA226_CONFIGURATION_REG_ADDR, ina226->config_reg);

    ina226->conversion_time = ina226_conversion_time(ina226->config_reg);

    return rc;
}

int
ina226_stop_continuous_mode(struct ina226_dev *ina226)
{
    return ina226_power_down(ina226);
}

int
ina226_read_id(struct ina226_dev *ina226, uint16_t *mfg, uint16_t *die)
{
    int rc;

    rc = ina226_read_reg(ina226, INA226_MFG_ID_REG_ADDR, mfg);
    if (rc == SYS_EOK) {
        rc = ina226_read_reg(ina226, INA226_DIE_ID_REG_ADDR, die);
    }
    return rc;
}

static int
ina226_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    /* Default values after power on. */
    const struct ina226_cfg default_cfg = {
        .avg_mode = MYNEWT_VAL(INA226_DEFAULT_AVERAGING),
        .vbusct = MYNEWT_VAL(INA226_DEFAULT_VBUS_CONVERSION_TIME),
        .vshct = MYNEWT_VAL(INA226_DEFAULT_VSHUNT_CONVERSION_TIME),
    };
    int rc;
    uint16_t mfg;
    uint16_t die;
    struct ina226_dev *ina226 = (struct ina226_dev *)dev;

    (void)wait;

    /* Reset sensor. */
    rc = ina226_reset(ina226);

    if (rc) {
        goto exit;
    }

    /* Verify sensor ID. */
    rc = ina226_read_id(ina226, &mfg, &die);
    if (rc != SYS_EOK) {
        goto exit;
    }
    if (mfg != INA226_MANUFACTURER_ID) {
        INA226_LOG_ERROR("INA226 read ID failed, no INA226 at 0x%X, found 0x%X 0x%X\n",
                         ina226->hw_cfg.itf.si_addr, mfg, die);
        rc = SYS_ENODEV;
        goto exit;
    }

    if (arg == NULL) {
        rc = ina226_config(ina226, &default_cfg);
    } else {
        rc = ina226_config(ina226, (const struct ina226_cfg *)arg);
    }
    enable_interrupt(ina226);
exit:
    return rc;
}

static int
ina226_close(struct os_dev *dev)
{
    struct ina226_dev *ina226 = (struct ina226_dev *)dev;

    disable_interrupt(ina226);
    return ina226_power_down(ina226);
}

int
ina226_init(struct os_dev *dev, void *arg)
{
    struct ina226_dev *ina226;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto exit;
    }

    ina226 = (struct ina226_dev *)dev;
    ina226->hw_cfg = *(struct ina226_hw_cfg *)arg;
    os_sem_init(&ina226->conversion_done, 0);


    sensor = &ina226->sensor;

    /* Initialise the stats entry. */
    rc = stats_init(
        STATS_HDR(ina226->stats),
        STATS_SIZE_INIT_PARMS(ina226->stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(ina226_stat_section));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);
    /* Register the entry with the stats registry */
    rc = stats_register(ina226->dev.od_name, STATS_HDR(ina226->stats));
    SYSINIT_PANIC_ASSERT(rc == SYS_EOK);

    rc = ina226_init_interrupt(ina226);
    if (rc != SYS_EOK) {
        goto exit;
    }

    rc = sensor_init(sensor, dev);
    if (rc != SYS_EOK) {
        goto exit;
    }

    (void)sensor_set_interface(sensor, &ina226->hw_cfg.itf);
    (void)sensor_set_type_mask(sensor, SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT);
    (void)sensor_set_driver(sensor, SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT,
                            (struct sensor_driver *)&g_ina226_sensor_driver);

    OS_DEV_SETHANDLERS(dev, ina226_open, ina226_close);
exit:
    return rc;
}

static int
ina226_sensor_read(struct sensor *sensor, sensor_type_t type,
                   sensor_data_func_t data_func, void *data_func_arg, uint32_t timeout)
{
    int rc;
    struct ina226_dev *ina226;
    int32_t current;
    uint32_t vbus;
    int32_t *pcurrent;
    uint32_t *pvbus;
    union {
        struct sensor_voltage_data svd;
        struct sensor_current_data scd;
    } databuf;
    (void)timeout;

    /* If the read isn't looking for bus voltage or current, don't do anything. */
    if (0 == (type & (SENSOR_TYPE_VOLTAGE | SENSOR_TYPE_CURRENT))) {
        rc = SYS_EINVAL;
        INA226_LOG_ERROR("ina226_sensor_read unsupported sensor type\n");
        goto exit;
    }

    pcurrent = (type & SENSOR_TYPE_CURRENT) ? &current : NULL;
    pvbus = (type & SENSOR_TYPE_VOLTAGE) ? &vbus : NULL;

    ina226 = (struct ina226_dev *)SENSOR_GET_DEVICE(sensor);

    rc = ina226_one_shot_read(ina226, pcurrent, pvbus, NULL);
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
    }
exit:
    return rc;
}

static int
ina226_sensor_get_config(struct sensor *sensor, sensor_type_t typ,
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
