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

#include <os/mynewt.h>
#include <mcu/stm32_hal.h>
#include <stm32_common/mcu.h>

#define STM32_PIN_FUN_CFG(port_n, pin_n, pf, af) \
const struct stm32_pin_cfg _P##port_n##pin_n##_##pf = {\
    .pin = MCU_GPIO_PORT##port_n(pin_n),\
    .hal_init = {\
        .Pin = 1 << (pin_n), \
        .Mode = GPIO_MODE_AF_PP, \
        .Pull = GPIO_NOPULL, \
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH, \
        .Alternate = af, \
    }\
};\
static stm32_pin_cfg_t const P##port_n##pin_n##_##pf __unused = &_P##port_n##pin_n##_##pf;

#define STM32_PIN_FUN_OD_CFG(port_n, pin_n, pf, af) \
const struct stm32_pin_cfg _P##port_n##pin_n##_##pf##_OD = {\
    .pin = MCU_GPIO_PORT##port_n(pin_n),\
    .hal_init = {\
        .Pin = 1 << (pin_n), \
        .Mode = GPIO_MODE_AF_OD, \
        .Pull = GPIO_NOPULL, \
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH, \
        .Alternate = af, \
    }\
};\
static stm32_pin_cfg_t const P##port_n##pin_n##_##pf##_OD __unused = &_P##port_n##pin_n##_##pf##_OD;

#define STM32_PIN_FUN_OD_PU_CFG(port_n, pin_n, pf, af) \
const struct stm32_pin_cfg _P##port_n##pin_n##_##pf##_OD_PU = {\
    .pin = MCU_GPIO_PORT##port_n(pin_n),\
    .hal_init = {\
        .Pin = 1 << (pin_n), \
        .Mode = GPIO_MODE_AF_OD, \
        .Pull = GPIO_PULLUP, \
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH, \
        .Alternate = af, \
    }\
};\
static stm32_pin_cfg_t const P##port_n##pin_n##_##pf##_OD_PU __unused = &_P##port_n##pin_n##_##pf##_OD_PU;

#define STM32_PIN_DEF(port_n, pin_n, pin_mode) \
const struct stm32_pin_cfg _P##port_n##pin_n##_##pin_mode = {\
    .pin = MCU_GPIO_PORT##port_n(pin_n),\
    .hal_init = {\
        .Pin = 1 << (pin_n), \
        .Mode = GPIO_MODE_##pin_mode, \
        .Pull = GPIO_NOPULL, \
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH, \
    }\
};\
const stm32_pin_t P##port_n##pin_n##_##pin_mode __unused = &_P##port_n##pin_n##_##pin_mode;

#include <mcu/stm32_pin_cfg.h>

void
stm32_pin_config(stm32_pin_cfg_t cfg)
{
    if (cfg) {
        hal_gpio_init_stm(cfg->pin, (GPIO_InitTypeDef *)&cfg->hal_init);
    }
}

void
stm32_pin_write(stm32_pin_cfg_t cfg, uint8_t val)
{
    if (cfg) {
        hal_gpio_write(cfg->pin, val);
    }
}

uint8_t
stm32_pin_read(stm32_pin_cfg_t cfg)
{
    return cfg ? hal_gpio_read(cfg->pin) : 0;
}

void
stm32_pin_pulse(hal_gpio_pin_t pin)
{
    hal_gpio_write(pin, 1);
    hal_gpio_write(pin, 0);
}

void
stm32_pin_pulse_n(hal_gpio_pin_t pin, int pulse_count)
{
    for (int i = 0; i < pulse_count; i++) {
        stm32_pin_pulse(pin);
    }
}
