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
#include <hal/lv_hal_disp.h>
#include <hal/hal_gpio.h>
#include <lcd_itf.h>

/*
 * Fundamental Command Table
 */
#define SSD1306_SET_LOWER_COL_ADDRESS           0x00
#define SSD1306_SET_LOWER_COL_ADDRESS_MASK      0x0f

#define SSD1306_SET_HIGHER_COL_ADDRESS          0x10
#define SSD1306_SET_HIGHER_COL_ADDRESS_MASK     0x0f

#define SSD1306_SET_MEM_ADDRESSING_MODE         0x20
#define SSD1306_SET_MEM_ADDRESSING_HORIZONTAL   0x00
#define SSD1306_SET_MEM_ADDRESSING_VERTICAL     0x01
#define SSD1306_SET_MEM_ADDRESSING_PAGE         0x02

#define SSD1306_SET_COLUMN_ADDRESS              0x21

#define SSD1306_SET_PAGE_ADDRESS                0x22

#define SSD1306_HORIZONTAL_SCROLL_SETUP         0x26

#define SSD1306_CONTINUOUS_VERTICAL_AND_HORIZONTAL_SCROLL_SETUP     0x29

#define SSD1306_DEACTIVATE_SCROLL               0x2e

#define SSD1306_ACTIVATE_SCROLL                 0x2f

#define SSD1306_SET_START_LINE                  0x40
#define SSD1306_SET_START_LINE_MASK             0x3f

#define SSD1306_SET_CONTRAST_CTRL               0x81

#define SDD1406_CHARGE_PUMP_SETTING             0x8d
#define SDD1406_CHARGE_PUMP_SETTING_DISABLE     0x10
#define SDD1406_CHARGE_PUMP_SETTING_ENABLE      0x14

#define SSD1306_SET_SEGMENT_MAP_NORMAL          0xa0
#define SSD1306_SET_SEGMENT_MAP_REMAPED         0xa1

#define SSD1306_SET_VERTICAL_SCROLL_AREA        0xa3

#define SSD1306_SET_ENTIRE_DISPLAY_OFF          0xa4
#define SSD1306_SET_ENTIRE_DISPLAY_ON           0xa5

#define SSD1306_SET_NORMAL_DISPLAY              0xa6
#define SSD1306_SET_REVERSE_DISPLAY             0xa7

#define SSD1306_SET_MULTIPLEX_RATIO             0xa8

#define SSD1306_DISPLAY_OFF                     0xae
#define SSD1306_DISPLAY_ON                      0xaf

#define SSD1306_SET_PAGE_START_ADDRESS          0xb0
#define SSD1306_SET_PAGE_START_ADDRESS_MASK     0x07

#define SSD1306_SET_COM_OUTPUT_SCAN_NORMAL      0xc0
#define SSD1306_SET_COM_OUTPUT_SCAN_FLIPPED     0xc8

#define SSD1306_SET_DISPLAY_OFFSET              0xd3

#define SSD1306_SET_CLOCK_DIV_RATIO             0xd5

#define SSD1306_SET_CHARGE_PERIOD               0xd9

#define SSD1306_SET_PADS_HW_CONFIG              0xda

#define SSD1306_SET_VCOM_DESELECT_LEVEL         0xdb

#define SSD1306_NOP                             0xe3

#define SSD1306_SET_PADS_HW_SEQUENTIAL          0x02
#define SSD1306_SET_PADS_HW_ALTERNATIVE         0x12

#define SSD1306_SET_CHARGE_PUMP_ON              0x8d
#define SSD1306_SET_CHARGE_PUMP_ON_DISABLED     0x10
#define SSD1306_SET_CHARGE_PUMP_ON_ENABLED      0x14

#define SH1106_SET_DCDC_MODE                    0xad
#define SH1106_SET_DCDC_DISABLED                0x8a
#define SH1106_SET_DCDC_ENABLED                 0x8b

#define SSD1306_SET_PUMP_VOLTAGE_64             0x30
#define SSD1306_SET_PUMP_VOLTAGE_74             0x31
#define SSD1306_SET_PUMP_VOLTAGE_80             0x32
#define SSD1306_SET_PUMP_VOLTAGE_90             0x33

#define SSD1306_READ_MODIFY_WRITE_START         0xe0
#define SSD1306_READ_MODIFY_WRITE_END           0xee

#define SSD1306_CLOCK_DIV_RATIO                 0x0
#define SSD1306_CLOCK_FREQUENCY                 0x8
#define SSD1306_PANEL_VCOM_DESEL_LEVEL          0x20
#define SSD1306_PANEL_PUMP_VOLTAGE              SSD1306_SET_PUMP_VOLTAGE_90

#if MYNEWT_VAL_CHOICE(SSD1306_ADDRESSING_MODE, horizontal)
#define ADDRESSING_MODE SSD1306_SET_MEM_ADDRESSING_HORIZONTAL
#else
#define ADDRESSING_MODE SSD1306_SET_MEM_ADDRESSING_PAGE
#endif

LCD_SEQUENCE(init_cmds)
    LCD_SEQUENCE_LCD_CS_INACTIVATE(),
    LCD_SEQUENCE_LCD_DC_DATA(),
    /* command length, command, args */
    1, SSD1306_DISPLAY_OFF,
    2, SSD1306_SET_CLOCK_DIV_RATIO, (SSD1306_CLOCK_FREQUENCY << 4) | SSD1306_CLOCK_DIV_RATIO,
    2, SSD1306_SET_MULTIPLEX_RATIO, 0x3f,
    2, SSD1306_SET_DISPLAY_OFFSET, 0,
    1, SSD1306_SET_START_LINE + 0,
    2, SDD1406_CHARGE_PUMP_SETTING, SDD1406_CHARGE_PUMP_SETTING_ENABLE,
    2, SSD1306_SET_MEM_ADDRESSING_MODE, ADDRESSING_MODE,
    1, SSD1306_SET_SEGMENT_MAP_REMAPED,
    1, SSD1306_SET_COM_OUTPUT_SCAN_FLIPPED,

    2, SSD1306_SET_PADS_HW_CONFIG, 0x12,
    2, SSD1306_SET_CONTRAST_CTRL, 0xcf,

    2, SSD1306_SET_CHARGE_PERIOD, 0xF1,

    2, SSD1306_SET_VCOM_DESELECT_LEVEL, 0x40,

    1, SSD1306_DEACTIVATE_SCROLL,

    1, SSD1306_SET_ENTIRE_DISPLAY_OFF,
    1, SSD1306_SET_NORMAL_DISPLAY,
    1, SSD1306_SET_START_LINE + 0,

    3, SSD1306_SET_HIGHER_COL_ADDRESS, 0, 0xb0,

    1, SSD1306_DISPLAY_ON,
LCD_SEQUENCE_END

/**
 * Initialize the SSD1306 display controller
 */
void
ssd1306_init(lv_disp_drv_t *driver)
{
    lcd_command_sequence(init_cmds);
}

#if MYNEWT_VAL_CHOICE(SSD1306_ADDRESSING_MODE, horizontal)
void
ssd1306_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint8_t page1 = area->y1 >> 3;
    uint8_t page2 = area->y2 >> 3;
    uint8_t *buf = (uint8_t *)color_p;
    uint8_t b[3];

    b[0] = SSD1306_SET_COLUMN_ADDRESS;
    b[1] = (uint8_t)area->x1;
    b[2] = (uint8_t)area->x2;
    lcd_ift_write_cmd(b, 3);

    b[0] = SSD1306_SET_PAGE_ADDRESS;
    b[1] = page1;
    b[2] = page2;
    lcd_ift_write_cmd(b, 3);

    lcd_itf_write_color_data(area->x1, area->y1, area->x2, area->y2, buf);

    lv_disp_flush_ready(drv);
}
#else
void
ssd1306_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint8_t page1 = area->y1 >> 3;
    uint8_t page2 = area->y2 >> 3;

    uint8_t *buf = (uint8_t *)color_p;
    int width = area->x2 - area->x1 + 1;
    uint8_t b[3];

    for (; page1 <= page2; ++page1, buf += width) {
        b[0] = SSD1306_SET_PAGE_START_ADDRESS + page1;
        lcd_ift_write_cmd(b, 1);
        b[0] = SSD1306_SET_LOWER_COL_ADDRESS + (area->x1 & 0xF);
        lcd_ift_write_cmd(b, 1);
        b[0] = SSD1306_SET_HIGHER_COL_ADDRESS + ((area->x1 >> 4) & 0xF);
        lcd_ift_write_cmd(b, 1);

        lcd_itf_write_color_data(area->x1, area->x2, page1 << 3, ((page1 + 1) << 3) - 1, buf);
    }
    lv_disp_flush_ready(drv);
}
#endif

void
ssd1306_set_px_cb(struct _lv_disp_drv_t *disp_drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
                  lv_color_t color, lv_opa_t opa)
{
    uint16_t byte_index = x + ((y >> 3) * buf_w);
    uint8_t bit_index = y & 0x7;

    if (color.full == 0) {
        buf[byte_index] |= 1 << bit_index;
    } else {
        buf[byte_index] &= ~(1 << bit_index);
    }
}

void
mynewt_lv_drv_init(lv_disp_drv_t *driver)
{
    if (MYNEWT_VAL(LCD_RESET_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_RESET_PIN), 1);
    }
    lcd_itf_init();
    driver->flush_cb = ssd1306_flush;
    driver->set_px_cb = ssd1306_set_px_cb;
    driver->hor_res = MYNEWT_VAL(LVGL_DISPLAY_HORIZONTAL_RESOLUTION);
    driver->ver_res = MYNEWT_VAL(LVGL_DISPLAY_VERTICAL_RESOLUTION);

    ssd1306_init(driver);
}
