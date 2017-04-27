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

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "defs/error.h"
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "hal/hal_spi.h"
#include "sensor/sensor.h"
#include "bme280/bme280.h"
#include "bme280_priv.h"
#include "hal/hal_gpio.h"

#if MYNEWT_VAL(BME280_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(BME280_STATS)
#include "stats/stats.h"
#endif

static struct hal_spi_settings spi_bme280_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE0,
    .baudrate   = 500,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

#if MYNEWT_VAL(BME280_STATS)
/* Define the stats section and records */
STATS_SECT_START(bme280_stat_section)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(bme280_stat_section)
    STATS_NAME(bme280_stat_section, errors)
STATS_NAME_END(bme280_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(bme280_stat_section) g_bme280stats;
#endif

#if MYNEWT_VAL(BME280_LOG)
#define LOG_MODULE_BME280    (2561)
#define BME280_INFO(...)     LOG_INFO(&_log, LOG_MODULE_BME280, __VA_ARGS__)
#define BME280_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_BME280, __VA_ARGS__)
static struct log _log;
#else
#define BME280_INFO(...)
#define BME280_ERR(...)
#endif

/* Exports for the sensor interface.
 */
static void *bme280_sensor_get_interface(struct sensor *, sensor_type_t);
static int bme280_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int bme280_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_bme280_sensor_driver = {
    bme280_sensor_get_interface,
    bme280_sensor_read,
    bme280_sensor_get_config
};

static int
bme280_default_cfg(struct bme280_cfg *cfg)
{
    cfg->bc_iir = BME280_FILTER_OFF;
    cfg->bc_mode = BME280_MODE_NORMAL;

    cfg->bc_boc[0].boc_type = SENSOR_TYPE_TEMPERATURE;
    cfg->bc_boc[0].boc_oversample = BME280_SAMPLING_NONE;
    cfg->bc_boc[1].boc_type = SENSOR_TYPE_PRESSURE;
    cfg->bc_boc[1].boc_oversample = BME280_SAMPLING_NONE;
    cfg->bc_boc[2].boc_type = SENSOR_TYPE_RELATIVE_HUMIDITY;
    cfg->bc_boc[2].boc_oversample = BME280_SAMPLING_NONE;

    return 0;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with bme280
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
bme280_init(struct os_dev *dev, void *arg)
{
    struct bme280 *bme280;
    struct sensor *sensor;
    int rc;

    bme280 = (struct bme280 *) dev;

    rc = bme280_default_cfg(&bme280->cfg);
    if (rc) {
        goto err;
    }

#if MYNEWT_VAL(BME280_LOG)
    log_register("bme280", &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

    sensor = &bme280->sensor;

#if MYNEWT_VAL(BME280_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_bme280stats),
        STATS_SIZE_INIT_PARMS(g_bme280stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(bme280_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register("bme280", STATS_HDR(g_bme280stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_TEMPERATURE |
                           SENSOR_TYPE_PRESSURE            |
                           SENSOR_TYPE_RELATIVE_HUMIDITY,
                           (struct sensor_driver *) &g_bme280_sensor_driver);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        goto err;
    }

    rc = hal_spi_config(MYNEWT_VAL(BME280_SPINUM), &spi_bme280_settings);
    if (rc) {
        goto err;
    }

    rc = hal_spi_enable(MYNEWT_VAL(BME280_SPINUM));
    if (rc) {
        goto err;
    }

    rc = hal_gpio_init_out(MYNEWT_VAL(BME280_CSPIN), 1);
    if (rc) {
        goto err;
    }

    return (0);
err:
    return (rc);

}

static void *
bme280_sensor_get_interface(struct sensor *sensor, sensor_type_t type)
{
    return (NULL);
}

static int
bme280_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    uint32_t temp;
    uint32_t press;
    uint32_t humid;
    int rc;

    if (!(type & SENSOR_TYPE_PRESSURE)    &&
        !(type & SENSOR_TYPE_TEMPERATURE) &&
        !(type & SENSOR_TYPE_RELATIVE_HUMIDITY)) {
        rc = SYS_EINVAL;
        goto err;
    }

    temp = press = humid = 0;

    /* Get a new pressure sample */
    if (type & SENSOR_TYPE_PRESSURE) {
        rc = bme280_get_pressure(&press);
        if (rc) {
            goto err;
        }

        //lux = bme280_calculate_lux(full, ir, &(bme280->cfg));

        /* Call data function */
        rc = data_func(sensor, data_arg, &press);
        if (rc) {
            goto err;
        }
    }

    /* Get a new temperature sample */
    if (type & SENSOR_TYPE_TEMPERATURE) {
        rc = bme280_get_temperature(&temp);
        if (rc) {
            goto err;
        }

        //lux = bme280_calculate_lux(full, ir, &(bme280->cfg));

        /* Call data function */
        rc = data_func(sensor, data_arg, &temp);
        if (rc) {
            goto err;
        }
    }

    /* Get a new relative humidity sample */
    if (type & SENSOR_TYPE_RELATIVE_HUMIDITY) {
        rc = bme280_get_humidity(&humid);
        if (rc) {
            goto err;
        }

        //lux = bme280_calculate_lux(full, ir, &(bme280->cfg));

        /* Call data function */
        rc = data_func(sensor, data_arg, &humid);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
bme280_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (!(type & SENSOR_TYPE_PRESSURE)    ||
        !(type & SENSOR_TYPE_TEMPERATURE) ||
        !(type & SENSOR_TYPE_RELATIVE_HUMIDITY)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_INT32;

    return (0);
err:
    return (rc);
}

/**
 * Configure BME280 sensor
 *
 * @param Sensor device BME280 structure
 * @param Sensor device BME280 config
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_config(struct bme280 *bme280, struct bme280_cfg *cfg)
{
    int rc;
    uint8_t id;

    /* Check if we can read the chip address */
    rc = bme280_get_chipid(&id);
    if (rc) {
        goto err;
    }

    if (id != BME280_CHIPID && id != BMP280_CHIPID) {
        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        rc = bme280_get_chipid(&id);
        if (rc) {
            goto err;
        }

        if(id != BME280_CHIPID && id != BMP280_CHIPID) {
            rc = SYS_EINVAL;
            goto err;
        }
    }

    rc = bme280_set_iir(cfg->bc_iir);
    if (rc) {
        goto err;
    }

    bme280->cfg.bc_iir = cfg->bc_iir;

    rc = bme280_set_mode(cfg->bc_mode);
    if (rc) {
        goto err;
    }

    bme280->cfg.bc_mode = cfg->bc_mode;

    if (!cfg->bc_boc[0].boc_type) {
        rc = bme280_set_oversample(cfg->bc_boc[0].boc_type,
                                   cfg->bc_boc[0].boc_oversample);
        if (rc) {
            goto err;
        }
    }

    bme280->cfg.bc_boc[0].boc_type = cfg->bc_boc[0].boc_type;
    bme280->cfg.bc_boc[0].boc_oversample = cfg->bc_boc[0].boc_oversample;

    if (!cfg->bc_boc[1].boc_type) {
        rc = bme280_set_oversample(cfg->bc_boc[1].boc_type,
                                   cfg->bc_boc[1].boc_oversample);
        if (rc) {
            goto err;
        }
    }

    bme280->cfg.bc_boc[1].boc_type = cfg->bc_boc[1].boc_type;
    bme280->cfg.bc_boc[1].boc_oversample = cfg->bc_boc[1].boc_oversample;

    if (!cfg->bc_boc[2].boc_type) {
        rc = bme280_set_oversample(cfg->bc_boc[2].boc_type,
                                   cfg->bc_boc[2].boc_oversample);
        if (rc) {
            goto err;
        }
    }

    bme280->cfg.bc_boc[2].boc_type = cfg->bc_boc[2].boc_type;
    bme280->cfg.bc_boc[2].boc_oversample = cfg->bc_boc[2].boc_oversample;

err:
    return (rc);
}

/**
 * Read multiple length data from BME280 sensor over SPI
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to read
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_readlen(uint8_t addr, uint8_t *payload, uint8_t len)
{
    int i;
    uint16_t retval;
    int rc;

    rc = 0;

    /* Select the device */
    hal_gpio_write(MYNEWT_VAL(BME280_CSPIN), 0);

    /* Send the address */
    retval = hal_spi_tx_val(MYNEWT_VAL(BME280_SPINUM),
                            addr | BME280_SPI_READ_CMD_BIT);
    if (retval == 0xFFFF) {
        rc = SYS_EINVAL;
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        retval = hal_spi_tx_val(MYNEWT_VAL(BME280_SPINUM), 0);
        if (retval == 0xFFFF) {
            rc = SYS_EINVAL;
            goto err;
        }
        payload[i] = retval;
    }

err:
    /* De-select the device */
    hal_gpio_write(MYNEWT_VAL(BME280_CSPIN), 1);
    return rc;
}

/**
 * Write multiple length data to BME280 sensor over SPI
 *
 * @param register address
 * @param variable length payload
 * @param length of the payload to write
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_writelen(uint8_t addr, uint8_t *payload, uint8_t len)
{
    int i;
    int rc;

    /* Select the device */
    hal_gpio_write(MYNEWT_VAL(BME280_CSPIN), 0);

    /* Send the address */
    rc = hal_spi_tx_val(MYNEWT_VAL(BME280_SPINUM), addr | BME280_SPI_READ_CMD_BIT);
    if (rc == 0xFFFF) {
        rc = SYS_EINVAL;
        goto err;
    }

    for (i = 0; i < len; i++) {
        /* Read data */
        rc = hal_spi_tx_val(MYNEWT_VAL(BME280_SPINUM), payload[i]);
        if (rc == 0xFFFF) {
            rc = SYS_EINVAL;
            goto err;
        }
    }

    /* De-select the device */
    hal_gpio_write(MYNEWT_VAL(BME280_CSPIN), 1);

    return 0;
err:
    return rc;
}

/**
 * Gets temperature
 *
 * @param temperature
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_get_temperature(uint32_t *temp)
{
    int rc;
    uint8_t tmp[3];

    rc = bme280_readlen(BME280_REG_ADDR_TEMP, tmp, 3);
    if (rc) {
        goto err;
    }

    *temp = (tmp[1] << 8 | tmp[0]) << 4 | (tmp[2] >> 4);

err:
    return rc;
}

/**
 * Gets humidity
 *
 * @param humidity
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_get_humidity(uint32_t *humid)
{
    int rc;
    uint8_t tmp[2];

    rc = bme280_readlen(BME280_REG_ADDR_HUM, tmp, 2);
    if (rc) {
        goto err;
    }

    *humid = (tmp[1] << 8 | tmp[0]);

err:
    return rc;
}

/**
 * Gets pressure
 *
 * @param pressure
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_get_pressure(uint32_t *press)
{
    int rc;
    uint8_t tmp[3];

    rc = bme280_readlen(BME280_REG_ADDR_PRESS, tmp, 2);
    if (rc) {
        goto err;
    }

    *press = (tmp[1] << 8 | tmp[0]) << 4 | (tmp[2] >> 4);

err:
    return rc;
}

/**
 * Resets the BME280 chip
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_reset(void)
{
    uint8_t txdata;

    txdata = 1;

    return bme280_writelen(BME280_REG_ADDR_RESET, &txdata, 1);
}

/**
 * Get IIR filter setting
 *
 * @param ptr to fill up iir setting into
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_get_iir(uint8_t *iir)
{
    int rc;
    uint8_t tmp;

    rc = bme280_readlen(BME280_REG_ADDR_CONFIG, &tmp, 1);
    if (rc) {
        goto err;
    }

    *iir = ((tmp & BME280_REG_CONFIG_FILTER) >> 5);

err:
    return rc;
}

/**
 * Sets IIR filter
 *
 * @param filter setting
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_set_iir(uint8_t iir)
{
    int rc;
    uint8_t cfg;

    rc = bme280_readlen(BME280_REG_ADDR_CONFIG, &cfg, 1);
    if (rc) {
        goto err;
    }

    iir = cfg | ((iir << 5) & BME280_REG_CONFIG_FILTER);

    rc = bme280_writelen(BME280_REG_ADDR_CONFIG, &iir, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the operating mode
 *
 * @param ptr to the mode variable to be filled up
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_get_mode(uint8_t *mode)
{
    int rc;
    uint8_t tmp;

    rc = bme280_readlen(BME280_REG_ADDR_CTRL_MEAS, &tmp, 1);
    if (rc) {
        goto err;
    }

    *mode = (tmp & BME280_REG_CTRL_MEAS_MODE);

err:
    return rc;
}

/**
 * Sets the operating mode
 *
 * @param mode
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_set_mode(uint8_t mode)
{
    int rc;
    uint8_t cfg;

    rc = bme280_readlen(BME280_REG_ADDR_CTRL_MEAS, &cfg, 1);
    if (rc) {
        goto err;
    }

    mode = cfg | (mode & BME280_REG_CTRL_MEAS_MODE);

    rc = bme280_writelen(BME280_REG_ADDR_CTRL_MEAS, &mode, 1);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Gets the current sampling rate for the type of sensor
 *
 * @param Type of sensor to return sampling rate
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_get_oversample(sensor_type_t type, uint8_t *oversample)
{
    int rc;
    uint8_t tmp;

    if (type & SENSOR_TYPE_TEMPERATURE || type & SENSOR_TYPE_PRESSURE) {
        rc = bme280_readlen(BME280_REG_ADDR_CTRL_MEAS, &tmp, 1);
        if (rc) {
            goto err;
        }

        if (type & SENSOR_TYPE_TEMPERATURE) {
            *oversample = ((tmp & BME280_REG_CTRL_MEAS_TOVER) >> 5);
        }

        if (type & SENSOR_TYPE_PRESSURE) {
            *oversample = ((tmp & BME280_REG_CTRL_MEAS_POVER) >> 3);
        }
    }

    if (type & SENSOR_TYPE_RELATIVE_HUMIDITY) {
        rc = bme280_readlen(BME280_REG_ADDR_CTRL_HUM, &tmp, 1);
        if (rc) {
            goto err;
        }
        *oversample = (tmp & BME280_REG_CTRL_HUM_HOVER);
    }

    return 0;
err:
    return rc;
}

/**
 * Sets the sampling rate
 *
 * @param sensor type
 * @param sampling rate
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bme280_set_oversample(sensor_type_t type, uint8_t oversample)
{
    int rc;
    uint8_t cfg;

    if (type & SENSOR_TYPE_TEMPERATURE || type & SENSOR_TYPE_PRESSURE) {
        rc = bme280_readlen(BME280_REG_ADDR_CTRL_MEAS, &cfg, 1);
        if (rc) {
            goto err;
        }

        if (type & SENSOR_TYPE_TEMPERATURE) {
            oversample = cfg | ((oversample << 5) & BME280_REG_CTRL_MEAS_TOVER);
        }

        if (type & SENSOR_TYPE_PRESSURE) {
            oversample = cfg | ((oversample << 3) & BME280_REG_CTRL_MEAS_POVER);
        }

        rc = bme280_writelen(BME280_REG_ADDR_CTRL_MEAS, &oversample, 1);
        if (rc) {
            goto err;
        }
    }

    if (type & SENSOR_TYPE_RELATIVE_HUMIDITY) {
        rc = bme280_readlen(BME280_REG_ADDR_CTRL_HUM, &cfg, 1);
        if (rc) {
            goto err;
        }

        oversample = cfg | (oversample & BME280_REG_CTRL_HUM_HOVER);

        rc = bme280_writelen(BME280_REG_ADDR_CTRL_HUM, &oversample, 1);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

/**
 * Get the chip id
 *
 * @param ptr to fill up the chip id
 *
 * @return 0 on success, non-zero on failure
 */
int
bme280_get_chipid(uint8_t *chipid)
{
    int rc;
    uint8_t tmp;

    rc = bme280_readlen(BME280_REG_ADDR_CHIPID, &tmp, 1);
    if (rc) {
        goto err;
    }

    *chipid = tmp;

err:
    return rc;
}
