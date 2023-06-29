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
#include "fsl_iocon.h"
#include "fsl_pint.h"

void PIN_INT0_IRQHandler(void);
void PIN_INT1_IRQHandler(void);
void PIN_INT2_IRQHandler(void);
void PIN_INT3_IRQHandler(void);
void PIN_INT4_IRQHandler(void);
void PIN_INT5_IRQHandler(void);
void PIN_INT6_IRQHandler(void);
void PIN_INT7_IRQHandler(void);

static void (*pin_int_handlers[])(void) = {
    PIN_INT0_IRQHandler,
    PIN_INT1_IRQHandler,
    PIN_INT2_IRQHandler,
    PIN_INT3_IRQHandler,
    PIN_INT4_IRQHandler,
    PIN_INT5_IRQHandler,
    PIN_INT6_IRQHandler,
    PIN_INT7_IRQHandler,
};

struct hal_gpio_irq {
    hal_gpio_irq_handler_t func;
    void *arg;
    int pin;
    pint_pin_enable_t trigger;
};

/* Each GPIO port has pins from 0 to 31 */
#define GPIO_INDEX(pin)     ((pin) & 0x1F)
#define GPIO_PORT(pin)      (((pin) >> 5) & 0x07)
#define GPIO_MASK(pin)      (1 << GPIO_INDEX(pin))
#define GPIO_PIN(port, pin)  ((((port) & 0x07) << 5) | ((pin) & 0x1F))
#define HAL_GPIO_MAX_IRQ    FSL_FEATURE_PINT_NUMBER_OF_CONNECTED_OUTPUTS

static GPIO_Type *const s_gpioBases[] = GPIO_BASE_PTRS;
static clock_ip_name_t const gpio_clocks[] = { kCLOCK_Gpio0, kCLOCK_Gpio1 };
static IRQn_Type const pint_irqs[] = PINT_IRQS;
static struct hal_gpio_irq hal_gpio_irqs[HAL_GPIO_MAX_IRQ] = {0};

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    gpio_pin_config_t gconfig = {
        .pinDirection = kGPIO_DigitalInput,
    };
    uint32_t modefunc = IOCON_FUNC0;

    if (pull == HAL_GPIO_PULL_DOWN) {
        modefunc = IOCON_FUNC0 | IOCON_MODE_PULLDOWN;
    } else if (pull == HAL_GPIO_PULL_UP) {
        modefunc = IOCON_FUNC0 | IOCON_MODE_PULLUP;
    } else {
        modefunc = IOCON_FUNC0;
    }

    CLOCK_EnableClock(gpio_clocks[GPIO_PORT(pin)]);
    GPIO_PinInit(GPIO, GPIO_PORT(pin), GPIO_INDEX(pin), &gconfig);
    IOCON_PinMuxSet(IOCON, GPIO_PORT(pin), GPIO_INDEX(pin), modefunc);

    return 0;
}

int
hal_gpio_init_out(int pin, int val)
{
    gpio_pin_config_t gconfig = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = (uint8_t)val,
    };

    CLOCK_EnableClock(gpio_clocks[GPIO_PORT(pin)]);
    GPIO_PinInit(GPIO, GPIO_PORT(pin), GPIO_INDEX(pin), &gconfig);
    IOCON_PinMuxSet(IOCON, GPIO_PORT(pin), GPIO_INDEX(pin), IOCON_FUNC0);

    return 0;
}

void
hal_gpio_write(int pin, int val)
{
    GPIO_PinWrite(s_gpioBases[0], GPIO_PORT(pin), GPIO_INDEX(pin), val);
}

int
hal_gpio_read(int pin)
{
    return (int)GPIO_PinRead(s_gpioBases[0], GPIO_PORT(pin), GPIO_INDEX(pin));
}

int
hal_gpio_toggle(int pin)
{
    GPIO_PortToggle(s_gpioBases[0], GPIO_PORT(pin), 1 << GPIO_INDEX(pin));

    return 0;
}

/*
 * Find out whether we have a GPIO event slot available.
 */
static int
hal_gpio_find_empty_slot(void)
{
    int i;
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func == NULL) {
            return i;
        }
    }
    return -1;
}

/*
 * Find a GPIO pin on the irq list.
 */
static int
find_irq_by_pin(int pin)
{
    int i;
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func != NULL && hal_gpio_irqs[i].pin == pin) {
            return i;
        }
    }
    return -1;
}

static void
pint_callback(pint_pin_int_t pintr, uint32_t pmatch_status)
{
    hal_gpio_irqs[pintr].func(hal_gpio_irqs[pintr].arg);
}

/**
 * Initialize a given pin to trigger a GPIO IRQ callback.
 *
 * @param pin     The pin to trigger GPIO interrupt on
 * @param handler The handler function to call
 * @param arg     The argument to provide to the IRQ handler
 * @param trig    The trigger mode (e.g. rising, falling)
 * @param pull    The mode of the pin (e.g. pullup, pulldown)
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
hal_gpio_irq_init(int pin,
                  hal_gpio_irq_handler_t handler,
                  void *arg,
                  hal_gpio_irq_trig_t trig,
                  hal_gpio_pull_t pull)
{
    int i = hal_gpio_find_empty_slot();
    if (i < 0) {
        return -1;
    }
    hal_gpio_irqs[i].pin = pin;
    hal_gpio_irqs[i].func = handler;
    hal_gpio_irqs[i].arg = arg;

    switch (trig) {
    case HAL_GPIO_TRIG_RISING:
        hal_gpio_irqs[i].trigger = kPINT_PinIntEnableRiseEdge;
        break;
    case HAL_GPIO_TRIG_FALLING:
        hal_gpio_irqs[i].trigger = kPINT_PinIntEnableFallEdge;
        break;
    case HAL_GPIO_TRIG_BOTH:
        hal_gpio_irqs[i].trigger = kPINT_PinIntEnableBothEdges;
        break;
    case HAL_GPIO_TRIG_LOW:
        hal_gpio_irqs[i].trigger = kPINT_PinIntEnableLowLevel;
        break;
    case HAL_GPIO_TRIG_HIGH:
        hal_gpio_irqs[i].trigger = kPINT_PinIntEnableHighLevel;
        break;
    default:
        return -1;
    }
    NVIC_SetVector(pint_irqs[i], (uint32_t)pin_int_handlers[i]);
    PINT_PinInterruptConfig(PINT, (pint_pin_int_t)i, kPINT_PinIntEnableNone, pint_callback);
    return hal_gpio_init_in(pin, pull);
}


/**
 * Release a pin from being configured to trigger IRQ on state change.
 *
 * @param pin The pin to release
 */
void
hal_gpio_irq_release(int pin)
{
    int i = find_irq_by_pin(pin);
    if (i >= 0) {
        PINT_DisableCallbackByIndex(PINT, (pint_pin_int_t)i);
        hal_gpio_irqs[i].func = NULL;
    }
}

/**
 * Enable IRQs on the passed pin
 *
 * @param pin The pin to enable IRQs on
 */
void
hal_gpio_irq_enable(int pin)
{
    int i = find_irq_by_pin(pin);
    if (i >= 0) {
        PINT_EnableCallbackByIndex(PINT, (pint_pin_int_t)i);
    }
}

/**
 * Disable IRQs on the passed pin
 *
 * @param pin The pin to disable IRQs on
 */
void
hal_gpio_irq_disable(int pin)
{
    int i = find_irq_by_pin(pin);
    if (i >= 0) {
        PINT_DisableCallbackByIndex(PINT, (pint_pin_int_t)i);
    }
}
