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
#include <modlog/modlog.h>

#define FT6X36_ADDR                     0x38

#define FT6X36_REG_DEVICE_MODE          0x00
#define FT6X36_REG_GESTURE_ID           0x01
#define FT6X36_REG_NUM_TOUCHES          0x02
#define FT6X36_REG_P1_XH                0x03
#define FT6X36_REG_P1_XL                0x04
#define FT6X36_REG_P1_YH                0x05
#define FT6X36_REG_P1_YL                0x06
#define FT6X36_REG_P1_WEIGHT            0x07
#define FT6X36_REG_P1_MISC              0x08
#define FT6X36_REG_P2_XH                0x09
#define FT6X36_REG_P2_XL                0x0A
#define FT6X36_REG_P2_YH                0x0B
#define FT6X36_REG_P2_YL                0x0C
#define FT6X36_REG_P2_WEIGHT            0x0D
#define FT6X36_REG_P2_MISC              0x0E
#define FT6X36_REG_THRESHHOLD           0x80
#define FT6X36_REG_FILTER_COEF          0x85
#define FT6X36_REG_CTRL                 0x86
#define FT6X36_REG_TIME_ENTER_MONITOR   0x87
#define FT6X36_REG_TOUCHRATE_ACTIVE     0x88
#define FT6X36_REG_TOUCHRATE_MONITOR    0x89
#define FT6X36_REG_RADIAN_VALUE         0x91
#define FT6X36_REG_OFFSET_LEFT_RIGHT    0x92
#define FT6X36_REG_OFFSET_UP_DOWN       0x93
#define FT6X36_REG_DISTANCE_LEFT_RIGHT  0x94
#define FT6X36_REG_DISTANCE_UP_DOWN     0x95
#define FT6X36_REG_DISTANCE_ZOOM        0x96
#define FT6X36_REG_LIB_VERSION_H        0xA1
#define FT6X36_REG_LIB_VERSION_L        0xA2
#define FT6X36_REG_CHIPID               0xA3
#define FT6X36_REG_INTERRUPT_MODE       0xA4
#define FT6X36_REG_POWER_MODE           0xA5
#define FT6X36_REG_FIRMWARE_VERSION     0xA6
#define FT6X36_REG_PANEL_ID             0xA8
#define FT6X36_REG_STATE                0xBC

#define FT6X36_PMODE_ACTIVE             0x00
#define FT6X36_PMODE_MONITOR            0x01
#define FT6X36_PMODE_STANDBY            0x02
#define FT6X36_PMODE_HIBERNATE          0x03

#define FT6X36_VENDID                   0x11
#define FT6206_CHIPID                   0x06
#define FT6236_CHIPID                   0x36
#define FT6336_CHIPID                   0x64

#define FT6X36_DEFAULT_THRESHOLD        22

static struct bus_i2c_node touch;
static struct bus_i2c_node_cfg touch_i2c_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(FT6X36_I2C_DEV_NAME),
    .addr = FT6X36_ADDR,
    .freq = MYNEWT_VAL(FT6X36_I2C_FREQ),
};

struct touch_screen_data {
    int last_x;
    int last_y;
};

struct os_dev *touch_dev;
static lv_indev_drv_t ft6x36drv;
static lv_indev_t *ft6x36dev;
static struct touch_screen_data touch_screen_data;
static bool ft6x36_notify;

static int
ft6x36_read_registers(struct os_dev *dev, uint8_t reg, uint8_t *buf, size_t buf_size)
{
    return bus_node_simple_write_read_transact(dev, &reg, 1, buf, buf_size);
}

static int
ft6x36_read_register(struct os_dev *dev, uint8_t reg, uint8_t *reg_val)
{
    return bus_node_simple_write_read_transact(dev, &reg, 1, reg_val, 1);
}

static int
ft6x36_write_register(struct os_dev *dev, uint8_t reg, uint8_t reg_val)
{
    uint8_t buf[2] = { reg, reg_val };

    return bus_node_simple_write(dev, buf, 2);
}

static void
ft6x36_int_isr(void *arg)
{
    ft6x36_notify = true;
}

/**
 * Get the current position and state of the touchpad
 * @param data store the read data here
 */
static void
ft6x36read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    struct os_dev *dev = indev_drv->user_data;
    uint8_t buf[5];
    int16_t x;
    int16_t y;
    int16_t raw_x;
    int16_t raw_y;
    bool pressed;

    if (MYNEWT_VAL(FT6X36_INT_PIN) < 0 || ft6x36_notify) {
        ft6x36_notify = false;
        ft6x36_read_registers(dev, FT6X36_REG_NUM_TOUCHES, buf, sizeof(buf));
        raw_x = ((buf[1] << 8) | buf[2]);
        raw_y = ((buf[3] << 8) | buf[4]);
        pressed = buf[0] != 0;
        if (pressed) {
            x = raw_x & 0xFFF;
            y = raw_y & 0xFFF;

            if (x != touch_screen_data.last_x || y != touch_screen_data.last_y) {
                FT6X36_LOG_DEBUG("Touch x=%d y=%d\n", x, y);
            }

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
}

/**
 * Initialize the FT6X36
 */
void
ft6x36_register_lv_indev(void)
{
    uint8_t panel_id;
    uint8_t chip_id;
    int ret;

    if (MYNEWT_VAL(FT6X36_RESET_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(FT6X36_RESET_PIN), 1);
    }
    do {
        ret = ft6x36_read_register(touch_dev, FT6X36_REG_PANEL_ID, &panel_id);
        if (ret || panel_id != FT6X36_VENDID) {
            FT6X36_LOG_ERROR("Touchscreen not detected.\n");
            break;
        }
        ret = ft6x36_read_register(touch_dev, FT6X36_REG_CHIPID, &chip_id);
        if (ret || chip_id != FT6336_CHIPID) {
            FT6X36_LOG_ERROR("Touchscreen not detected.\n");
            break;
        }
        if (MYNEWT_VAL(FT6X36_INT_PIN) >= 0) {
            hal_gpio_irq_init(MYNEWT_VAL(FT6X36_INT_PIN),
                              ft6x36_int_isr, NULL, HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_NONE);
        }
        ft6x36_write_register(touch_dev, FT6X36_REG_DEVICE_MODE, 0x00);
        ft6x36_write_register(touch_dev, FT6X36_REG_THRESHHOLD, MYNEWT_VAL(FT6X36_THRESHOLD));
        ft6x36_write_register(touch_dev, FT6X36_REG_TOUCHRATE_ACTIVE, 0x0E);

        /* Register a keypad input device */
        lv_indev_drv_init(&ft6x36drv);
        ft6x36drv.type = LV_INDEV_TYPE_POINTER;
        ft6x36drv.read_cb = ft6x36read;
        ft6x36drv.user_data = touch_dev;
        ft6x36dev = lv_indev_drv_register(&ft6x36drv);
        FT6X36_LOG_INFO("Touchscreen registered\n");
    } while (0);
}

void
ft6x36_os_dev_create(void)
{
    struct bus_node_callbacks cbs = {0};
    int rc;

    if (MYNEWT_VAL(FT6X36_RESET_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(FT6X36_RESET_PIN), 1);
    }

    bus_node_set_callbacks((struct os_dev *)&touch, &cbs);

    rc = bus_i2c_node_create("touch", &touch, &touch_i2c_cfg, NULL);
    assert(rc == 0);
    touch_dev = os_dev_open("touch", 0, NULL);
}
