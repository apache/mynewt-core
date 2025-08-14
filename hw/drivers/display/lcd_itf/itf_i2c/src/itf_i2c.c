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

#include <bsp/bsp.h>
#if MYNEWT_PKG_apache_mynewt_core__hw_bus_drivers_i2c_common
#include <bus/drivers/i2c_common.h>
#else
#include <hal/hal_i2c.h>
#endif

#include <lv_conf.h>
#include <lcd_itf.h>

#if MYNEWT_PKG_apache_mynewt_core__hw_bus_drivers_i2c_common
static struct bus_i2c_node lcd;
static struct bus_i2c_node_cfg lcd_i2c_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(LCD_I2C_DEV_NAME),
    .addr = MYNEWT_VAL(LCD_I2C_ADDR),
    .freq = MYNEWT_VAL(LCD_I2C_FREQ),
};
static struct os_dev *lcd_dev;
#else
#endif

void
lcd_itf_write_color_data(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, void *pixels)
{
    uint8_t *color_data = pixels;
    size_t size = ((x2 - x1 + 1) * (y2 - y1 + 1)) >> 3;

#if MYNEWT_PKG_apache_mynewt_core__hw_bus_drivers_i2c_common
    color_data[-1] = 0x40;
    bus_node_write(lcd_dev, color_data - 1, size + 1, 1000, 0);
#else
    color_data[-1] = 0x40;
    struct hal_i2c_master_data data = {
            .address = MYNEWT_VAL(LCD_I2C_ADDR),
            .buffer = color_data - 1,
            .len = size + 1,
    };
    hal_i2c_master_write(0, &data, 1000, 1);
#endif
}

void
lcd_ift_write_cmd(const uint8_t *cmd, int cmd_length)
{
    uint8_t buf[cmd_length + 1];

    buf[0] = 0;
    for (int i = 0; i < cmd_length; ++i) {
        buf[i + 1] = cmd[i];
    }
#if MYNEWT_PKG_apache_mynewt_core__hw_bus_drivers_i2c_common
    bus_node_write(lcd_dev, buf, cmd_length + 1, 1000, 0);
#else
    struct hal_i2c_master_data data = {
            .address = MYNEWT_VAL(LCD_I2C_ADDR),
            .buffer = buf,
            .len = cmd_length + 1,
    };
    hal_i2c_master_write(0, &data, 1000, 1);
#endif
}


void
lcd_itf_init(void)
{
#if MYNEWT_PKG_apache_mynewt_core__hw_bus_drivers_i2c_common
    int rc;
    struct bus_node_callbacks cbs = {};

    bus_node_set_callbacks((struct os_dev *)&lcd, &cbs);

    rc = bus_i2c_node_create("display", &lcd, &lcd_i2c_cfg, NULL);
    assert(rc == 0);
    lcd_dev = os_dev_open("display", 0, NULL);
#endif
}
