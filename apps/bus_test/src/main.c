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

#include "os/mynewt.h"
#include "bus/bus.h"
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#include "console/console.h"
#include "hal/hal_gpio.h"
#if MYNEWT_VAL(APP_USE_BME280_SENSOR)
#include "sensor/sensor.h"
#include "sensor/temperature.h"
#endif

#if MYNEWT_VAL(APP_USE_LIS2DH_NODE)
#include "lis2dh_node/lis2dh_node.h"
#endif
#if MYNEWT_VAL(APP_USE_BME280_SENSOR)
#include "bme280/bme280.h"
#endif

#if MYNEWT_VAL(APP_USE_BME280_SENSOR)
static struct sensor_itf g_bme280_sensor_itf;

#if MYNEWT_VAL(BME280_NODE_BUS_TYPE) == 0
static const struct bus_i2c_node_cfg g_bme280_i2c_node_cfg = {
    .node_cfg = {
        .bus_name = MYNEWT_VAL(BME280_NODE_BUS_NAME),
    },
    .addr = MYNEWT_VAL(BME280_NODE_I2C_ADDRESS),
    .freq = MYNEWT_VAL(BME280_NODE_I2C_FREQUENCY),
};
#elif MYNEWT_VAL(BME280_NODE_BUS_TYPE) == 1
static const struct bus_spi_node_cfg g_bme280_spi_node_cfg = {
    .node_cfg = {
        .bus_name = MYNEWT_VAL(BME280_NODE_BUS_NAME),
    },
    .pin_cs = MYNEWT_VAL(BME280_NODE_SPI_CS_PIN),
    .mode = BUS_SPI_MODE_0,
    .data_order = BUS_SPI_DATA_ORDER_MSB,
    .freq = MYNEWT_VAL(BME280_NODE_SPI_FREQUENCY),
};
#endif

static struct os_dev *g_bme280_node;
#endif

#if MYNEWT_VAL(APP_USE_LIS2DH_NODE)
static const struct bus_i2c_node_cfg g_lis2dh_node_i2c_cfg = {
    .node_cfg = {
        .bus_name = MYNEWT_VAL(LIS2DH_NODE_BUS_NAME),
    },
    .addr = MYNEWT_VAL(LIS2DH_NODE_I2C_ADDRESS),
    .freq = MYNEWT_VAL(LIS2DH_NODE_I2C_FREQUENCY),
};

static struct os_dev *g_lis2dh_node;
#endif

#if MYNEWT_VAL(APP_USE_BME280_SENSOR)
static struct bme280 bme280;
static struct sensor *bme280_sensor;
struct sensor_listener bme280_listener;

#endif

static struct os_callout co_read;

static void
co_read_cb(struct os_event *ev)
{
#if MYNEWT_VAL(APP_USE_LIS2DH_NODE)
    struct lis2dh_node_pos pos;
#endif
    int rc;

#if MYNEWT_VAL(APP_USE_LIS2DH_NODE)
    rc = lis2dh_node_read(g_lis2dh_node, &pos);
    assert(rc == 0);
    console_printf("X=%04x Y=%04x Z=%04x\n", pos.x, pos.y, pos.z);
#endif

    rc = os_callout_reset(&co_read, os_time_ms_to_ticks32(1000));
    assert(rc == 0);
}

#if MYNEWT_VAL(APP_USE_BME280_SENSOR)
static int
bme280_sensor_listener_cb(struct sensor *sensor, void *arg,
                          void *data, sensor_type_t type)
{
    struct sensor_temp_data *std;

    std = data;

    console_printf("T=%f (valid %d)\n", std->std_temp, std->std_temp_is_valid);

    return 0;
}


static void
bme280_sensor_configure(void)
{
    struct os_dev *dev;
    struct bme280_cfg cfg;
    int rc;

    dev = os_dev_open("bme280", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    memset(&cfg, 0, sizeof(cfg));

    cfg.bc_mode = BME280_MODE_FORCED;
    cfg.bc_iir = BME280_FILTER_OFF;
    cfg.bc_sby_dur = BME280_STANDBY_MS_0_5;
    cfg.bc_boc[0].boc_type = SENSOR_TYPE_RELATIVE_HUMIDITY;
    cfg.bc_boc[1].boc_type = SENSOR_TYPE_PRESSURE;
    cfg.bc_boc[2].boc_type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    cfg.bc_boc[0].boc_oversample = BME280_SAMPLING_X1;
    cfg.bc_boc[1].boc_oversample = BME280_SAMPLING_X1;
    cfg.bc_boc[2].boc_oversample = BME280_SAMPLING_X1;
    cfg.bc_s_mask = SENSOR_TYPE_AMBIENT_TEMPERATURE|
                       SENSOR_TYPE_PRESSURE|
                       SENSOR_TYPE_RELATIVE_HUMIDITY;

    rc = bme280_config((struct bme280 *)dev, &cfg);
    assert(rc == 0);

    os_dev_close(dev);
}
#endif

int
main(int argc, char **argv)
{
    int rc;

    sysinit();

#if MYNEWT_VAL(APP_USE_LIS2DH_NODE)
    rc = lis2dh_node_i2c_create("lis2dh", &g_lis2dh_node_i2c_cfg);
    assert(rc == 0);

    g_lis2dh_node = os_dev_open("lis2dh", 0, NULL);
    assert(g_lis2dh_node);
#endif

#if MYNEWT_VAL(APP_USE_BME280_SENSOR)
#if MYNEWT_VAL(BME280_NODE_BUS_TYPE) == 0
    /* For I2C SDO pin is address select pin */
    hal_gpio_init_out(MYNEWT_VAL(SPI_1_MASTER_PIN_MISO), 0);
    /* Make sure CSB is not set to low state as it will switch BME280 to SPI */
    hal_gpio_init_out(MYNEWT_VAL(BME280_NODE_SPI_CS_PIN), 1);

    rc = bme280_create_i2c_sensor_dev(&bme280.i2c_node, "bme280",
                                      &g_bme280_i2c_node_cfg,
                                      &g_bme280_sensor_itf);
    assert(rc == 0);
#elif MYNEWT_VAL(BME280_NODE_BUS_TYPE) == 1
    rc = bme280_create_spi_sensor_dev(&bme280.spi_node, "bme280",
                                      &g_bme280_spi_node_cfg,
                                      &g_bme280_sensor_itf);
    assert(rc == 0);
#endif

    g_bme280_node = os_dev_open("bme280", 0, NULL);
    assert(g_bme280_node);

    bme280_sensor_configure();

    bme280_sensor = sensor_mgr_find_next_bydevname("bme280", NULL);
    assert(bme280_sensor);

    bme280_listener.sl_sensor_type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    bme280_listener.sl_func = bme280_sensor_listener_cb;
    bme280_listener.sl_arg = NULL;

    rc = sensor_register_listener(bme280_sensor, &bme280_listener);
    assert(rc == 0);

    rc = sensor_set_poll_rate_ms("bme280", 500);
    assert(rc == 0);
#endif

    os_callout_init(&co_read, os_eventq_dflt_get(), co_read_cb, NULL);

#if MYNEWT_VAL(APP_USE_LIS2DH_NODE)
    rc = os_callout_reset(&co_read, os_time_ms_to_ticks32(1000));
    assert(rc == 0);
#endif

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return (0);
}
