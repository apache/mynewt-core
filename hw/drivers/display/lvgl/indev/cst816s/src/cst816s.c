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

#include <stddef.h>
#include <os/os_time.h>
#include <hal/hal_gpio.h>
#include <bus/drivers/i2c_common.h>
#include <bsp/bsp.h>

#include <hal/lv_hal_indev.h>
#include <core/lv_disp.h>

#define CST816S_CHIP_ID 0xB4

#define CST816S_REG_DATA                0x00
#define CST816S_REG_GESTURE_ID          0x01
#define CST816S_REG_FINGER_NUM          0x02
#define CST816S_REG_XPOS_H              0x03
#define CST816S_REG_XPOS_L              0x04
#define CST816S_REG_YPOS_H              0x05
#define CST816S_REG_YPOS_L              0x06
#define CST816S_REG_BPC0H               0xB0
#define CST816S_REG_BPC0L               0xB1
#define CST816S_REG_BPC1H               0xB2
#define CST816S_REG_BPC1L               0xB3
#define CST816S_REG_POWER_MODE          0xA5
#define CST816S_REG_CHIP_ID             0xA7
#define CST816S_REG_PROJ_ID             0xA8
#define CST816S_REG_FW_VERSION          0xA9
#define CST816S_REG_MOTION_MASK         0xEC
#define CST816S_REG_IRQ_PULSE_WIDTH     0xED
#define CST816S_REG_NOR_SCAN_PER        0xEE
#define CST816S_REG_MOTION_S1_ANGLE     0xEF
#define CST816S_REG_LP_SCAN_RAW1H       0xF0
#define CST816S_REG_LP_SCAN_RAW1L       0xF1
#define CST816S_REG_LP_SCAN_RAW2H       0xF2
#define CST816S_REG_LP_SCAN_RAW2L       0xF3
#define CST816S_REG_LP_AUTO_WAKEUP_TIME 0xF4
#define CST816S_REG_LP_SCAN_TH          0xF5
#define CST816S_REG_LP_SCAN_WIN         0xF6
#define CST816S_REG_LP_SCAN_FREQ        0xF7
#define CST816S_REG_LP_SCAN_I_DAC       0xF8
#define CST816S_REG_AUTOSLEEP_TIME      0xF9
#define CST816S_REG_IRQ_CTL             0xFA
#define CST816S_REG_DEBOUNCE_TIME       0xFB
#define CST816S_REG_LONG_PRESS_TIME     0xFC
#define CST816S_REG_IOCTL               0xFD
#define CST816S_REG_DIS_AUTO_SLEEP      0xFE

static struct bus_i2c_node touch;
static struct bus_i2c_node_cfg touch_i2c_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(CST816S_I2C_DEV_NAME),
    .addr = 0x15,
    .freq = MYNEWT_VAL(CST816S_I2C_FREQ),
};
struct os_dev *touch_dev;

static lv_indev_drv_t cst816s_drv;
static lv_indev_t *cst816s_dev;

struct touch_screen_data {
    int last_x;
    int last_y;
};

static struct touch_screen_data touch_screen_data;

/**
 * Get the current position and state of the touchpad
 * @param data store the read data here
 * @return false: because no ore data to be read
 */
static void
cst816s_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint8_t reg[1] = {CST816S_REG_DATA};
    uint8_t buf[4];
    int16_t x;
    int16_t y;
    int16_t raw_x;
    int16_t raw_y;
    bool pressed;

    reg[0] = CST816S_REG_XPOS_H;
    bus_node_simple_write_read_transact(touch_dev, reg, 1, buf, sizeof(buf));
    raw_x = (buf[0] << 8) | buf[1];
    raw_y = (buf[2] << 8) | buf[3];
    pressed = (4 >> (buf[0] >> 6)) & 1;
    if (pressed) {
        x = raw_x & 0x3FF;
        y = raw_y & 0x3FF;

        touch_screen_data.last_x = x;
        touch_screen_data.last_y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        x = touch_screen_data.last_x;
        y = touch_screen_data.last_y;
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = x;
    data->point.y = y;
}

/**
 * Initialize the CST816S
 */
void
cst816s_register_lv_indev(void)
{
    hal_gpio_init_in(MYNEWT_VAL(CST816S_INT_PIN), HAL_GPIO_PULL_NONE);
    /* Register a keypad input device */
    lv_indev_drv_init(&cst816s_drv);
    cst816s_drv.type = LV_INDEV_TYPE_POINTER;
    cst816s_drv.read_cb = cst816s_read;
    cst816s_dev = lv_indev_drv_register(&cst816s_drv);
}

void
cst816s_os_dev_create(void)
{
    struct bus_node_callbacks cbs = { 0 };
    int rc;

    if (MYNEWT_VAL(CST816S_RESET_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(CST816S_RESET_PIN), 1);
    }

    bus_node_set_callbacks((struct os_dev *)&touch, &cbs);

    rc = bus_i2c_node_create("touch", &touch, &touch_i2c_cfg, NULL);
    assert(rc == 0);
    touch_dev = os_dev_open("touch", 0, NULL);
}
