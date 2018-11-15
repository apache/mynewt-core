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
#include "console/console.h"
#include "bus/bus.h"
#include "bus/i2c.h"
#include "bme280_node/bme280_node.h"
#include "ext/bme280.h"

#define BME280_ODEV(_node) ((struct os_dev *)(&(_node)->i2c_node))

struct bme280_node {
    struct bme280_dev bme280_dev;
    struct bus_i2c_node i2c_node;
};

static struct bme280_node g_bme280_node;

struct reg_val {
    uint8_t addr;
    uint8_t val[32];
};

static int8_t
bme280_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    int rc;

    rc = bus_node_simple_write_read_transact(BME280_ODEV(&g_bme280_node),
                                             &reg_addr, 1, data, len);

    return rc;
}

static int8_t
bme280_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    struct reg_val reg;
    int rc;

    assert(len <= sizeof(reg.val));

    reg.addr = reg_addr;
    memcpy(reg.val, data, len);

    rc = bus_node_simple_write(BME280_ODEV(&g_bme280_node), &reg.addr, 1 + len);

    return rc;
}

static void
bme280_delay_ms(uint32_t period)
{
    os_cputime_delay_usecs(period * 1000);
}

static void
open_node_cb(struct bus_node *node)
{
    struct bme280_node *bme280_node = CONTAINER_OF(node, struct bme280_node, i2c_node);
    struct bme280_dev *bme280_dev = &bme280_node->bme280_dev;
    int rc;

    console_printf("%s: node %p\n", __func__, node);

    bme280_dev->dev_id = 0x76;
    bme280_dev->intf = BME280_I2C_INTF;
    bme280_dev->read = bme280_read;
    bme280_dev->write = bme280_write;
    bme280_dev->delay_ms = bme280_delay_ms;
    rc = bme280_init(bme280_dev);
    SYSINIT_PANIC_ASSERT(rc == 0);

    bme280_dev->settings.osr_h = BME280_OVERSAMPLING_1X;
    bme280_dev->settings.osr_p = BME280_OVERSAMPLING_16X;
    bme280_dev->settings.osr_t = BME280_OVERSAMPLING_2X;
    bme280_dev->settings.filter = BME280_FILTER_COEFF_16;

    rc = bme280_set_sensor_settings(BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL |
                                    BME280_OSR_HUM_SEL | BME280_FILTER_SEL,
                                    bme280_dev);
    assert(rc == 0);
}

static void
close_node_cb(struct bus_node *node)
{
    console_printf("%s: node %p\n", __func__, node);
}

int
bme280_node_read(struct os_dev *dev, struct bme280_node_measurement *measurement)
{
    struct bme280_node *bme280_node = CONTAINER_OF(dev, struct bme280_node, i2c_node);
    struct bme280_data sensor_data;
    int rc;

    assert(measurement);

    rc = bme280_set_sensor_mode(BME280_FORCED_MODE, &bme280_node->bme280_dev);
    assert(rc == 0);

    rc = bme280_get_sensor_data(0x07, &sensor_data, &bme280_node->bme280_dev);
    assert(rc == 0);

    measurement->temperature = sensor_data.temperature / 100.0f;
    measurement->pressure = sensor_data.pressure / 10000.0f;
    measurement->humidity = sensor_data.humidity / 1000.0f;

    return 0;
}

int
bme280_node_i2c_create(const char *name, const struct bus_i2c_node_cfg *cfg)
{
    struct bus_node_callbacks cbs = {
        .init = NULL,
        .open = open_node_cb,
        .close = close_node_cb,
    };
    int rc;

    bus_node_set_callbacks((struct os_dev *)&g_bme280_node.i2c_node, &cbs);

    rc = bus_i2c_node_create(name, &g_bme280_node.i2c_node,
                             (struct bus_i2c_node_cfg *)cfg);

    return rc;
}
