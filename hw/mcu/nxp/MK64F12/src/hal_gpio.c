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
#include "hal/hal_gpio.h"
#include "mcu/cmsis_nvic.h"
#include <stdlib.h>
#include <assert.h>

#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_port.h"

/* Each GPIO port has pins from 0 to 31 */
#define GPIO_INDEX(pin)     ((pin) & 0x1F)
#define GPIO_PORT(pin)      (((pin) >> 5) & 0x07)
#define GPIO_MASK(pin)      (1 << GPIO_INDEX(pin))
#define GPIO_PIN(port, pin)  ((((port) & 0x07) << 5) | ((pin) & 0x1F))

static GPIO_Type *const s_gpioBases[] = GPIO_BASE_PTRS;
static PORT_Type *const s_portBases[] = PORT_BASE_PTRS;
static clock_ip_name_t const s_portClocks[] = PORT_CLOCKS;

uint16_t
hal_to_fsl_pull(hal_gpio_pull_t pull)
{
    switch ((int)pull)
    {
    case HAL_GPIO_PULL_UP:
        return kPORT_PullUp;
    case HAL_GPIO_PULL_DOWN:
        return kPORT_PullDown;
    default:
        return kPORT_PullDisable;
    }
}

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    gpio_pin_config_t gconfig;
    port_pin_config_t pconfig;

    gconfig.pinDirection = kGPIO_DigitalInput;
    pconfig.pullSelect = hal_to_fsl_pull(pull);
    pconfig.mux = kPORT_MuxAsGpio;

    CLOCK_EnableClock(s_portClocks[GPIO_PORT(pin)]);
    PORT_SetPinConfig(s_portBases[GPIO_PORT(pin)], GPIO_INDEX(pin), &pconfig);
    GPIO_PinInit(s_gpioBases[GPIO_PORT(pin)], GPIO_INDEX(pin), &gconfig);

    return 0;
}

int
hal_gpio_init_out(int pin, int val)
{
    gpio_pin_config_t gconfig;
    port_pin_config_t pconfig;

    gconfig.pinDirection = kGPIO_DigitalOutput;
    pconfig.mux = kPORT_MuxAsGpio;

    CLOCK_EnableClock(s_portClocks[GPIO_PORT(pin)]);
    PORT_SetPinConfig(s_portBases[GPIO_PORT(pin)], GPIO_INDEX(pin), &pconfig);
    GPIO_PinInit(s_gpioBases[GPIO_PORT(pin)], GPIO_INDEX(pin), &gconfig);

    return 0;
}

void
hal_gpio_write(int pin, int val)
{
    GPIO_WritePinOutput(s_gpioBases[GPIO_PORT(pin)], GPIO_INDEX(pin), val);
}

int
hal_gpio_read(int pin)
{
    return (int)GPIO_ReadPinInput(s_gpioBases[GPIO_PORT(pin)], GPIO_INDEX(pin));
}

int
hal_gpio_toggle(int pin)
{
    GPIO_TogglePinsOutput(s_gpioBases[GPIO_PORT(pin)], 1 << GPIO_INDEX(pin));

    return 0;
}
