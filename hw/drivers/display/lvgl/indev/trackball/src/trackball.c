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

#include <os/os_time.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>

#include <hal/lv_hal_indev.h>
#include <core/lv_disp.h>
#include <core/lv_indev.h>
#include <lvgl.h>
#include <bsp/bsp.h>

static lv_indev_drv_t trackball_drv;
static lv_indev_t *trackball_dev;

#define UP_PIN MYNEWT_VAL(TRACKBALL_UP_PIN)
#define DOWN_PIN MYNEWT_VAL(TRACKBALL_DOWN_PIN)
#define LEFT_PIN MYNEWT_VAL(TRACKBALL_LEFT_PIN)
#define RIGHT_PIN MYNEWT_VAL(TRACKBALL_RIGHT_PIN)
#define BUTTON_PIN MYNEWT_VAL(TRACKBALL_BUTTON_PIN)
#define BUTTON_PIN_PULL MYNEWT_VAL(TRACKBALL_BUTTON_PIN_PULL)
#define BUTTON_PIN_VALUE MYNEWT_VAL(TRACKBALL_BUTTON_PIN_VALUE)
#define DRAG_PIN MYNEWT_VAL(TRACKBALL_DRAG_PIN)
#define DRAG_PIN_VALUE MYNEWT_VAL(TRACKBALL_DRAG_PIN_VALUE)
#define HOLD_TIME MYNEWT_VAL(TRACKBALL_HOLD_TO_DRAG_TIME_MS)

struct accel {
    uint32_t limit;
    int increment;
};

static const struct accel accel_steps[] = {
    { 10, 4 },
    { 15, 3 },
    { 30, 2 },
    { 0, 1 },
};
struct trackball_data {
    uint32_t up_time;
    uint32_t down_time;
    uint32_t left_time;
    uint32_t right_time;
    uint32_t press_time;
    enum {
        RELEASED,
        PRESSED_WAITING_FOR_HOLD,
        PRESSED,
        PRESS_HELD,
    } state;
    bool hold;
    bool reported;
    /* Current value for X, Y detected */
    int x;
    int y;
};

static struct trackball_data trackball_data;

static void
movement_detected(void)
{
    if (trackball_data.state == PRESSED_WAITING_FOR_HOLD) {
        trackball_data.state = PRESSED;
    }
}

static int
increment(uint32_t *t)
{
    uint32_t now = os_time_ticks_to_ms32(os_time_get());
    uint32_t diff = now - *t;
    *t = now;
    int i;

    for (i = 0; accel_steps[i].limit; ++i) {
        if (diff < accel_steps[i].limit) {
            break;
        }
    }
    movement_detected();
    return accel_steps[i].increment;
}

static void
trackball_up(void *arg)
{
    (void)arg;

    trackball_data.y -= increment(&trackball_data.up_time);
}

static void
trackball_down(void *arg)
{
    (void)arg;

    trackball_data.y += increment(&trackball_data.down_time);
}

static void
trackball_left(void *arg)
{
    (void)arg;

    trackball_data.x -= increment(&trackball_data.left_time);
}

static void
trackball_right(void *arg)
{
    (void)arg;

    trackball_data.x += increment(&trackball_data.right_time);
}

static void
trackball_button(void *arg)
{
    (void)arg;
    bool pressed = hal_gpio_read(BUTTON_PIN) == BUTTON_PIN_VALUE;

    switch (trackball_data.state) {
    case RELEASED:
        if (pressed) {
            trackball_data.state = PRESSED_WAITING_FOR_HOLD;
            trackball_data.press_time = os_time_ticks_to_ms32(os_time_get());
        }
        break;
    case PRESSED_WAITING_FOR_HOLD:
    case PRESSED:
        if (!pressed) {
            trackball_data.state = RELEASED;
        }
        break;
    case PRESS_HELD:
        if (pressed) {
            trackball_data.state = PRESSED_WAITING_FOR_HOLD;
            trackball_data.press_time = os_time_ticks_to_ms32(os_time_get());
        }
        break;
    }
}

/**
 * Get the current position and state of the touchpad
 * @param data store the read data here
 */
static void
trackball_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint32_t now = os_time_ticks_to_ms32(os_time_get());

    if (trackball_data.state == PRESSED_WAITING_FOR_HOLD) {
        if ((now - trackball_data.press_time) > HOLD_TIME) {
            trackball_data.state = PRESS_HELD;
        }
    }
    if (DRAG_PIN >= 0) {
        hal_gpio_write(LED_1, trackball_data.state == PRESS_HELD ? DRAG_PIN_VALUE : !DRAG_PIN_VALUE);
    }
    data->state = trackball_data.state != RELEASED ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (trackball_data.x < 0) {
        trackball_data.x = 0;
    } else if (trackball_data.x >= LV_HOR_RES) {
        trackball_data.x = LV_HOR_RES - 1;
    }
    if (trackball_data.y < 0) {
        trackball_data.y = 0;
    } else if (trackball_data.y >= LV_VER_RES) {
        trackball_data.y = LV_VER_RES - 1;
    }
    data->point.x = trackball_data.x;
    data->point.y = trackball_data.y;
}

/**
 * Initialize the trackball indev
 */
void
trackball_register_lv_indev(void)
{
    hal_gpio_irq_init(UP_PIN, trackball_up, NULL, HAL_GPIO_TRIG_BOTH, HAL_GPIO_PULL_NONE);
    hal_gpio_irq_init(DOWN_PIN, trackball_down, NULL, HAL_GPIO_TRIG_BOTH, HAL_GPIO_PULL_NONE);
    hal_gpio_irq_init(LEFT_PIN, trackball_left, NULL, HAL_GPIO_TRIG_BOTH, HAL_GPIO_PULL_NONE);
    hal_gpio_irq_init(RIGHT_PIN, trackball_right, NULL, HAL_GPIO_TRIG_BOTH, HAL_GPIO_PULL_NONE);
    hal_gpio_irq_init(BUTTON_PIN, trackball_button, NULL, HAL_GPIO_TRIG_BOTH, BUTTON_PIN_PULL);
    if (DRAG_PIN >= 0) {
        hal_gpio_init_out(DRAG_PIN, !DRAG_PIN_VALUE);
    }
    /* Register a keypad input device */
    lv_indev_drv_init(&trackball_drv);
    trackball_drv.type = LV_INDEV_TYPE_POINTER;
    trackball_drv.read_cb = trackball_read;
    trackball_dev = lv_indev_drv_register(&trackball_drv);

    /*Set cursor. For simplicity set a HOME symbol now.*/
    lv_obj_t *mouse_cursor = lv_img_create(lv_scr_act());
    lv_img_set_src(mouse_cursor, LV_SYMBOL_BLUETOOTH);
    lv_indev_set_cursor(trackball_dev, mouse_cursor);
}
