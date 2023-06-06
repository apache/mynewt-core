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

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_CURSOR
#define LV_ATTRIBUTE_IMG_CURSOR
#endif

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t cursor_normal_map[] = {
    0x00, 0x00, 0x00, 0x00,     /*Color of index 0*/
    0xff, 0xff, 0xff, 0xff,     /*Color of index 1*/
    0x00, 0x00, 0x00, 0xff,     /*Color of index 2*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 3*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 4*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 5*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 6*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 7*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 8*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 9*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 10*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 11*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 12*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 13*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 14*/
    0x00, 0x00, 0x00, 0x00,     /*Color of index 15*/

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x11, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x11, 0x12, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x11, 0x11, 0x20, 0x00, 0x00, 0x00,
    0x02, 0x11, 0x11, 0x12, 0x00, 0x00, 0x00,
    0x02, 0x11, 0x11, 0x11, 0x20, 0x00, 0x00,
    0x02, 0x11, 0x11, 0x11, 0x12, 0x00, 0x00,
    0x02, 0x11, 0x11, 0x11, 0x11, 0x20, 0x00,
    0x02, 0x11, 0x11, 0x11, 0x11, 0x22, 0x00,
    0x02, 0x11, 0x11, 0x11, 0x22, 0x00, 0x00,
    0x02, 0x11, 0x12, 0x22, 0x00, 0x00, 0x00,
    0x02, 0x12, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const lv_img_dsc_t cursor_normal_img_dsc = {
    .header.cf = LV_IMG_CF_INDEXED_4BIT,
    .header.always_zero = 0,
    .header.reserved = 0,
    .header.w = 13,
    .header.h = 17,
    .data_size = 183,
    .data = cursor_normal_map,
};


static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t cursor_drag_map[] = {
    0x00, 0x00, 0x00, 0x00,     /*Color of index 0*/
    0xff, 0xff, 0x00, 0x01,     /*Color of index 1*/
    0x80, 0x80, 0x15, 0x04,     /*Color of index 2*/
    0x9d, 0x9d, 0x34, 0x06,     /*Color of index 3*/
    0xb8, 0xb8, 0x45, 0x12,     /*Color of index 4*/
    0x83, 0x83, 0x30, 0x1a,     /*Color of index 5*/
    0xb8, 0xb8, 0x44, 0x3a,     /*Color of index 6*/
    0x81, 0x82, 0x30, 0x4b,     /*Color of index 7*/
    0xb7, 0xb7, 0x44, 0x79,     /*Color of index 8*/
    0xb7, 0xb7, 0x43, 0x91,     /*Color of index 9*/
    0x81, 0x82, 0x30, 0x69,     /*Color of index 10*/
    0xb8, 0xb8, 0x43, 0xae,     /*Color of index 11*/
    0x80, 0x81, 0x2f, 0x85,     /*Color of index 12*/
    0xb8, 0xb8, 0x43, 0xe8,     /*Color of index 13*/
    0x81, 0x81, 0x30, 0xc8,     /*Color of index 14*/
    0x81, 0x82, 0x30, 0xed,     /*Color of index 15*/

    0x00, 0x00, 0x5c, 0xe7, 0x07, 0xec, 0x50, 0x00, 0x00,
    0x00, 0x2a, 0xfe, 0xa5, 0x05, 0xae, 0xfa, 0x10, 0x00,
    0x01, 0xcf, 0x73, 0x69, 0x99, 0x63, 0x7f, 0xc0, 0x00,
    0x0a, 0xf5, 0x6d, 0xdb, 0x9b, 0xdd, 0x47, 0xfa, 0x00,
    0x5f, 0x76, 0xd9, 0x41, 0x01, 0x49, 0xd4, 0xaf, 0x50,
    0xae, 0x3b, 0xb4, 0x96, 0x06, 0x94, 0xbb, 0x5e, 0xa0,
    0xec, 0x6d, 0x49, 0xd4, 0x04, 0xd8, 0x4d, 0x6c, 0xe0,
    0xf7, 0x8b, 0x4d, 0x60, 0x00, 0x6d, 0x4b, 0x87, 0xe0,
    0xf5, 0x99, 0x6d, 0x40, 0x00, 0x4d, 0x4b, 0x87, 0xf0,
    0xf7, 0x8b, 0x4d, 0x60, 0x00, 0x6d, 0x4b, 0x87, 0xf0,
    0xea, 0x6d, 0x49, 0xd6, 0x46, 0xd9, 0x4d, 0x6a, 0xe0,
    0xae, 0x3d, 0x94, 0x9d, 0xdd, 0x94, 0xbb, 0x5e, 0xa0,
    0x5f, 0xa4, 0xd9, 0x44, 0x64, 0x49, 0xd4, 0xaf, 0x50,
    0x0a, 0xf7, 0x6d, 0xdb, 0x9b, 0xdd, 0x47, 0xf7, 0x00,
    0x01, 0xcf, 0x73, 0x69, 0x99, 0x64, 0x7f, 0xc0, 0x00,
    0x00, 0x2a, 0xfe, 0xa5, 0x05, 0xae, 0xfa, 0x10, 0x00,
    0x00, 0x00, 0x5c, 0xe7, 0x07, 0xec, 0x50, 0x00, 0x00,
};

const lv_img_dsc_t cursor_drag_img_dsc = {
    .header.cf = LV_IMG_CF_INDEXED_4BIT,
    .header.always_zero = 0,
    .header.reserved = 0,
    .header.w = 17,
    .header.h = 17,
    .data_size = 217,
    .data = cursor_drag_map,
};

static lv_style_t style_normal;
static lv_style_t style_pressed;

static lv_obj_t *mouse_cursor;
static lv_obj_t *drag_cursor;
static bool drag_cursor_set;

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
            lv_img_set_src(mouse_cursor, &cursor_drag_img_dsc);
            drag_cursor_set = true;
        }
    } else if (trackball_data.state == RELEASED && drag_cursor_set) {
        lv_img_set_src(mouse_cursor, &cursor_normal_img_dsc);
        drag_cursor_set = false;
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
    hal_gpio_irq_enable(UP_PIN);
    hal_gpio_irq_enable(DOWN_PIN);
    hal_gpio_irq_enable(LEFT_PIN);
    hal_gpio_irq_enable(RIGHT_PIN);
    hal_gpio_irq_enable(BUTTON_PIN);

    /* Register a keypad input device */
    lv_indev_drv_init(&trackball_drv);
    trackball_drv.type = LV_INDEV_TYPE_POINTER;
    trackball_drv.read_cb = trackball_read;
    trackball_dev = lv_indev_drv_register(&trackball_drv);

    lv_style_init(&style_normal);
    lv_style_set_translate_x(&style_normal, 0);
    lv_style_init(&style_pressed);
    lv_style_set_translate_x(&style_pressed, -10);
    lv_style_set_translate_y(&style_pressed, -10);
    mouse_cursor = lv_img_create(lv_scr_act());
    drag_cursor = lv_img_create(lv_scr_act());
    lv_img_set_src(mouse_cursor, &cursor_normal_img_dsc);
    lv_img_set_src(drag_cursor, &cursor_drag_img_dsc);
    lv_obj_add_style(mouse_cursor, &style_normal, LV_STATE_DEFAULT);
    lv_obj_add_style(drag_cursor, &style_pressed, LV_STATE_DEFAULT);
    lv_indev_set_cursor(trackball_dev, mouse_cursor);
}
