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

#include <mcu/cmsis_nvic.h>

#include <assert.h>
#include <compiler.h>
#include "port.h"
#include "extint.h"

/* XXX: Notes
 * 4) The code probably does not handle "re-purposing" gpio very well.
 * "Re-purposing" means changing a gpio from input to output, or calling
 * gpio_init_in and expecting previously enabled interrupts to be stopped.
 *
 */

/*
 * GPIO pin mapping
 *
 * The samD 21G has 48 pins and 38 GPIO.
 *
 * They are split among 2 ports A (PA0-PA25) and B
 *
 * , PB8, PB9,
 *  PB10, PB11,
 * PB3 PB2,  PB23, PB22,
 *
 * The port A Pins exposed on the CHIP are
 *  PA0,  PA1,  PA2,  PA3,  PA4,  PA5,  PA6,  PA7,
 *  PA8,  PA9, PA10, PA11, PA12, PA13, PA14, PA15,
 * PA16, PA17, PA18, PA19, PA20, PA21, PA22, PA23,
 * PA24, PA25,       PA27, PA28,       PA30, PA31,
 *
 * The port B pins exposed the chip are
 *              PB2,  PB3,
 *  PB8,  PB9, PB10,  PB11,
 *
 *                                     PB22, PB23,
 *
 *
 * We keep the mapping where ports A0-A31 are pins 0 - 31
 * and ports B0-B31 are pins 32 - 64;
 */

#define GPIO_PORT(pin)           (pin / 32)
#define GPIO_PIN(pin)            (pin % 32)

#define GPIO_MAX_PORT   (1)

/* this defines which pins are valid for the ports */
static const int valid_pins[GPIO_MAX_PORT + 1] = {
    0xdbffffff,
    0xc0000f0c,
};

/*
 * Map from pin to external interrupt channel.
 */
static const int8_t hal_gpio_pin_exti_tbl[] = {
    0,  /* PA0 */
    1,  /* PA1 */
    2,  /* PA2 */
    3,  /* PA3 */
    4,  /* PA4 */
    5,  /* PA5 */
    6,  /* PA6 */
    7,  /* PA7 */
    -1, /* PA8 */ /* NMI */
    9,  /* PA9 */
    10, /* PA10 */
    11, /* PA11 */
    12, /* PA12 */
    13, /* PA13 */
    14, /* PA14 */
    15, /* PA15 */
    0,  /* PA16 */
    1,  /* PA17 */
    2,  /* PA18 */
    3,  /* PA19 */
    4,  /* PA20 */
    5,  /* PA21 */
    6,  /* PA22 */
    7,  /* PA23 */
    12, /* PA24 */
    13, /* PA25 */
    -1, /* PA26 */
    15, /* PA27 */
    8,  /* PA28 */
    -1, /* PA29 */
    10, /* PA30 */
    11, /* PA31 */
    0,  /* PB0 */
    1,  /* PB1 */
    2,  /* PB2 */
    3,  /* PB3 */
    4,  /* PB4 */
    5,  /* PB5 */
    6,  /* PB6 */
    7,  /* PB7 */
    8,  /* PB8 */
    9,  /* PB9 */
    10, /* PB10 */
    11, /* PB11 */
    12, /* PB12 */
    13, /* PB13 */
    14, /* PB14 */
    15, /* PB15 */
    0,  /* PB16 */
    1,  /* PB17 */
    -1, /* PB18 */
    -1, /* PB19 */
    -1, /* PB20 */
    -1, /* PB21 */
    6,  /* PB22 */
    7,  /* PB23 */
    -1, /* PB24 */
    -1, /* PB25 */
    -1, /* PB26 */
    -1, /* PB27 */
    -1, /* PB28 */
    -1, /* PB29 */
    14, /* PB30 */
    15, /* PB31 */
};

/*
 * Registered interrupt handlers.
 */
struct gpio_irq {
    hal_gpio_irq_handler_t func;
    void *arg;
} hal_gpio_irqs[EIC_NUMBER_OF_INTERRUPTS];

int
hal_gpio_init_out(int pin, int val)
{
    struct port_config cfg;

    int port = GPIO_PORT(pin);
    int port_pin = GPIO_PIN(pin);

    if (port > GPIO_MAX_PORT) {
        return -1;
    }

    if ((port_pin & valid_pins[port]) == 0) {
        return -1;
    }

    cfg.direction = PORT_PIN_DIR_OUTPUT_WTH_READBACK;
    cfg.input_pull = SYSTEM_PINMUX_PIN_PULL_NONE;
    cfg.powersave = false;

    port_pin_set_config(pin, &cfg);

    hal_gpio_write(pin, val);

    return 0;
}

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    struct port_config cfg;

    int port = GPIO_PORT(pin);
    int port_pin = GPIO_PIN(pin);

    if (port > GPIO_MAX_PORT) {
        return -1;
    }

    if ((port_pin & valid_pins[port]) == 0) {
        return -1;
    }

    cfg.direction = PORT_PIN_DIR_INPUT;
    switch (pull) {
    case HAL_GPIO_PULL_NONE:
        cfg.input_pull = PORT_PIN_PULL_NONE;
        break;
    case HAL_GPIO_PULL_UP:
        cfg.input_pull = HAL_GPIO_PULL_UP;
        break;
    case HAL_GPIO_PULL_DOWN:
        cfg.input_pull = HAL_GPIO_PULL_DOWN;
        break;
    default:
        return -1;
    }

    cfg.powersave = false;

    port_pin_set_config(pin, &cfg);

    return 0;
}

/**
 * gpio read
 *
 * Reads the specified pin.
 *
 * @param pin Pin number to read
 *
 * @return int 0: low, 1: high
 */
int
hal_gpio_read(int pin)
{
    int rc;
    int port = GPIO_PORT(pin);
    int port_pin = GPIO_PIN(pin);

    assert(port <= GPIO_MAX_PORT);
    assert(((1 << port_pin) & valid_pins[port]) != 0);

    rc = port_pin_get_input_level(pin);
    return rc;
}

/**
 * gpio write
 *
 * Write a value (either high or low) to the specified pin.
 *
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void
hal_gpio_write(int pin, int val)
{
    if (val) {
        port_pin_set_output_level(pin, true);
    } else {
        port_pin_set_output_level(pin, false);
    }
}

/**
 * gpio toggle
 *
 * Toggles the specified pin
 *
 * @param pin Pin number to toggle
 */
int
hal_gpio_toggle(int pin)
{
    int pin_state;

    pin_state = (hal_gpio_read(pin) == 0);
    hal_gpio_write(pin, pin_state);
    return hal_gpio_read(pin);
}

/*
 * Interrupt handler for gpio.
 */
static void
hal_gpio_irq(void)
{
    int i;
    struct gpio_irq *irq;

    for (i = 0; i < EIC_NUMBER_OF_INTERRUPTS; i++) {
        if (extint_chan_is_detected(i)) {
            extint_chan_clear_detected(i);
            irq = &hal_gpio_irqs[i];
            if (irq->func) {
                irq->func(irq->arg);
            }
        }
    }
}

/*
 * Validate pin, and return extint channel pin belongs to.
 */
static int
hal_gpio_irq_eic(int pin)
{
    int8_t eic;
    int port;
    int port_pin;

    port = GPIO_PORT(pin);
    port_pin = GPIO_PIN(pin);

    if ((port_pin & valid_pins[port]) == 0) {
        return -1;
    }
    eic = hal_gpio_pin_exti_tbl[pin];
    return eic;
}

/**
 * gpio irq init
 *
 * Initialize an external interrupt on a gpio pin.
 *
 * @param pin       Pin number to enable gpio.
 * @param handler   Interrupt handler
 * @param arg       Argument to pass to interrupt handler
 * @param trig      Trigger mode of interrupt
 * @param pull      Push/pull mode of input.
 *
 * @return int
 */
int
hal_gpio_irq_init(int pin, hal_gpio_irq_handler_t handler, void *arg,
                  hal_gpio_irq_trig_t trig, hal_gpio_pull_t pull)
{
    struct extint_chan_conf cfg;
    int rc;
    int8_t eic;

    NVIC_SetVector(EIC_IRQn, (uint32_t)hal_gpio_irq);
    NVIC_EnableIRQ(EIC_IRQn);

    extint_chan_get_config_defaults(&cfg);

    /* Configure the gpio for an external interrupt */
    rc = 0;
    switch (trig) {
    case HAL_GPIO_TRIG_NONE:
        rc = -1;
        break;
    case HAL_GPIO_TRIG_RISING:
        cfg.detection_criteria = EXTINT_DETECT_RISING;
        break;
    case HAL_GPIO_TRIG_FALLING:
        cfg.detection_criteria = EXTINT_DETECT_FALLING;
        break;
    case HAL_GPIO_TRIG_BOTH:
        cfg.detection_criteria = EXTINT_DETECT_BOTH;
        break;
    case HAL_GPIO_TRIG_LOW:
        cfg.detection_criteria = EXTINT_DETECT_LOW;
        break;
    case HAL_GPIO_TRIG_HIGH:
        cfg.detection_criteria = EXTINT_DETECT_HIGH;
        break;
    default:
        rc = -1;
        break;
    }
    if (rc) {
        return rc;
    }

    switch (pull) {
    case HAL_GPIO_PULL_NONE:
        cfg.gpio_pin_pull = EXTINT_PULL_NONE;
        break;
    case HAL_GPIO_PULL_UP:
        cfg.gpio_pin_pull = EXTINT_PULL_UP;
        break;
    case HAL_GPIO_PULL_DOWN:
        cfg.gpio_pin_pull = EXTINT_PULL_DOWN;
        break;
    default:
        rc = -1;
        break;
    }
    if (rc) {
        return rc;
    }

    cfg.gpio_pin = pin;
    cfg.gpio_pin_mux = 0;

    eic = hal_gpio_irq_eic(pin);
    if (eic < 0) {
        return -1;
    }
    if (hal_gpio_irqs[eic].func) {
        return -1;
    }
    hal_gpio_irqs[eic].func = handler;
    hal_gpio_irqs[eic].arg = arg;

    extint_chan_set_config(eic, &cfg);
    return 0;
}

/**
 * gpio irq release
 *
 * No longer interrupt when something occurs on the pin. NOTE: this function
 * does not change the GPIO push/pull setting.
 *
 * @param pin
 */
void
hal_gpio_irq_release(int pin)
{
    int8_t eic;

    eic = hal_gpio_irq_eic(pin);
    if (eic < 0) {
        return;
    }
    hal_gpio_irq_disable(pin);
    hal_gpio_irqs[eic].func = NULL;
}

/**
 * gpio irq enable
 *
 * Enable the irq on the specified pin
 *
 * @param pin
 */
void
hal_gpio_irq_enable(int pin)
{
    int8_t eic;

    eic = hal_gpio_irq_eic(pin);
    if (eic < 0) {
        return;
    }

    extint_chan_enable_callback(eic, EXTINT_CALLBACK_TYPE_DETECT);
}

/**
 * gpio irq disable
 *
 *
 * @param pin
 */
void
hal_gpio_irq_disable(int pin)
{
    int8_t eic;

    eic = hal_gpio_irq_eic(pin);
    if (eic < 0) {
        return;
    }
    extint_chan_disable_callback(eic, EXTINT_CALLBACK_TYPE_DETECT);
}
