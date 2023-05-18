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
#include <hal/hal_gpio.h>
#include <bus/drivers/spi_common.h>

#include <lv_conf.h>
#include <lcd_itf.h>

static struct bus_spi_node lcd;
static struct bus_spi_node_cfg lcd_spi_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(LCD_SPI_DEV_NAME),
    .pin_cs = MYNEWT_VAL(LCD_CS_PIN),
    .mode = MYNEWT_VAL(LCD_SPI_MODE),
    .data_order = HAL_SPI_MSB_FIRST,
    .freq = MYNEWT_VAL(LCD_SPI_FREQ),
};
static struct os_dev *lcd_dev;

static uint8_t *lv_color_16_swap_buffer;
static size_t lv_color_16_swap_buffer_size;

void
lcd_itf_write_color_data(const void *pixels, size_t size)
{
    const void *color_data = pixels;
    size_t i;

    LCD_DC_PIN_DATA();
    LCD_CS_PIN_ACTIVE();
    if (LV_COLOR_16_SWAP == 0) {
        if (size > lv_color_16_swap_buffer_size) {
            lv_color_16_swap_buffer = realloc(lv_color_16_swap_buffer, size);
            if (lv_color_16_swap_buffer) {
                lv_color_16_swap_buffer_size = size;
            } else {
                lv_color_16_swap_buffer_size = 0;
            }
        }
        if (lv_color_16_swap_buffer) {
            color_data = lv_color_16_swap_buffer;
            for (i = 0; i < size; i += 2) {
                lv_color_16_swap_buffer[i] = ((uint8_t *)pixels)[i + 1];
                lv_color_16_swap_buffer[i + 1] = ((uint8_t *)pixels)[i];
            }
        }
    }
    bus_node_write(lcd_dev, color_data, size, 1000, BUS_F_NOSTOP);
    LCD_CS_PIN_INACTIVE();
}

void
lcd_ift_write_cmd(const uint8_t *cmd, int cmd_length)
{
#if MYNEWT_VAL(LCD_SPI_WITH_SHIFT_REGISTER)
    uint16_t buf[cmd_length];
#else
    uint8_t buf[cmd_length];
#endif

    if (MYNEWT_VAL(LCD_SPI_WITH_SHIFT_REGISTER)) {
        for (int i = 0; i < cmd_length; ++i) {
            buf[i] = htobe16(cmd[i]);
        }
    } else {
        memcpy(buf, cmd, cmd_length);
    }
    LCD_DC_PIN_COMMAND();
    LCD_CS_PIN_ACTIVE();
    bus_node_write(lcd_dev, buf, sizeof(buf[0]), 1000, cmd_length == 1 ? BUS_F_NOSTOP : 0);
    if (cmd_length > 1) {
        LCD_DC_PIN_DATA();
        bus_node_write(lcd_dev, buf + 1, (cmd_length - 1) * sizeof(buf[0]), 1000, 0);
    }
    LCD_CS_PIN_INACTIVE();
}


void
lcd_itf_init(void)
{
    int rc;
    struct bus_node_callbacks cbs = {};

    hal_gpio_init_out(MYNEWT_VAL(LCD_DC_PIN), 0);
    if (MYNEWT_VAL(LCD_CS_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_CS_PIN), 1);
    }
    bus_node_set_callbacks((struct os_dev *)&lcd, &cbs);

    rc = bus_spi_node_create("display", &lcd, &lcd_spi_cfg, NULL);
    assert(rc == 0);
    lcd_dev = os_dev_open("display", 0, NULL);
}
