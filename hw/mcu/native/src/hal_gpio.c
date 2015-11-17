/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hal/hal_gpio.h"

#include <stdio.h>

#define HAL_GPIO_NUM_PINS 8

static struct {
    int val;
    enum {
        INPUT,
        OUTPUT
    } dir;
} hal_gpio[HAL_GPIO_NUM_PINS];

int
gpio_init_in(int pin, gpio_pull_t pull)
{
    if (pin >= HAL_GPIO_NUM_PINS) {
        return -1;
    }
    hal_gpio[pin].dir = INPUT;
    switch (pull) {
    case GPIO_PULL_UP:
        hal_gpio[pin].val = 1;
        break;
    default:
        hal_gpio[pin].val = 0;
        break;
    }
    return 0;
}

int
gpio_init_out(int pin, int val)
{
    if (pin >= HAL_GPIO_NUM_PINS) {
        return -1;
    }
    hal_gpio[pin].dir = OUTPUT;
    hal_gpio[pin].val = (val != 0);
    return 0;
}

void
gpio_set(int pin)
{
    gpio_write(pin, 1);
}

void
gpio_clear(int pin)
{
    gpio_write(pin, 0);
}

void gpio_write(int pin, int val)
{
    if (pin >= HAL_GPIO_NUM_PINS) {
        return;
    }
    if (hal_gpio[pin].dir != OUTPUT) {
        return;
    }
    hal_gpio[pin].val = (val != 0);
}

int
gpio_read(int pin)
{
    if (pin >= HAL_GPIO_NUM_PINS) {
        return -1;
    }
    return hal_gpio[pin].val;
}

void
gpio_toggle(int pin)
{
    gpio_write(pin, gpio_read(pin) != 1);
}
