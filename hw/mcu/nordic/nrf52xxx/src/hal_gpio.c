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
#include "bsp/cmsis_nvic.h"
#include "os/os_trace_api.h"
#include <stdlib.h>
#include "nrf.h"
#include <assert.h>

/* XXX:
 * 1) The code probably does not handle "re-purposing" gpio very well.
 * "Re-purposing" means changing a gpio from input to output, or calling
 * gpio_init_in and expecting previously enabled interrupts to be stopped.
 *
 */

/*
 * GPIO pin mapping
 *
 * The logical GPIO pin numbers (0 to N) are mapped to ports in the following
 * manner:
 *  pins 0 - 31: Port 0
 *  pins 32 - 48: Port 1.
 *
 *  The nrf52832 has only one port with 32 pins. The nrf52840 has 48 pins and
 *  uses two ports.
 *
 *  NOTE: in order to save code space, there is no checking done to see if the
 *  user specifies a pin that is not used by the processor. If an invalid pin
 *  number is used unexpected and/or erroneous behavior will result.
 */
#ifdef NRF52
#define HAL_GPIO_PORT(pin)      (NRF_P0)
#define HAL_GPIO_MASK(pin)      (1 << pin)
#define HAL_GPIOTE_PIN_MASK     GPIOTE_CONFIG_PSEL_Msk
#endif

#ifdef NRF52840_XXAA
#define HAL_GPIO_INDEX(pin)     ((pin) & 0x1F)
#define HAL_GPIO_PORT(pin)      ((pin) > 31 ? NRF_P1 : NRF_P0)
#define HAL_GPIO_MASK(pin)      (1 << HAL_GPIO_INDEX(pin))
#define HAL_GPIOTE_PIN_MASK     (0x3FUL << GPIOTE_CONFIG_PSEL_Pos)
#endif

/* GPIO interrupts */
#define HAL_GPIO_MAX_IRQ        8

/* Storage for GPIO callbacks. */
struct hal_gpio_irq {
    hal_gpio_irq_handler_t func;
    void *arg;
};

static struct hal_gpio_irq hal_gpio_irqs[HAL_GPIO_MAX_IRQ];

/**
 * gpio init in
 *
 * Initializes the specified pin as an input
 *
 * @param pin   Pin number to set as input
 * @param pull  pull type
 *
 * @return int  0: no error; -1 otherwise.
 */
int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    uint32_t conf;
    NRF_GPIO_Type *port;

    switch (pull) {
    case HAL_GPIO_PULL_UP:
        conf = GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos;
        break;
    case HAL_GPIO_PULL_DOWN:
        conf = GPIO_PIN_CNF_PULL_Pulldown << GPIO_PIN_CNF_PULL_Pos;
        break;
    case HAL_GPIO_PULL_NONE:
    default:
        conf = 0;
        break;
    }

    port = HAL_GPIO_PORT(pin);
    port->PIN_CNF[pin] = conf;
    port->DIRCLR = HAL_GPIO_MASK(pin);

    return 0;
}

/**
 * gpio init out
 *
 * Initialize the specified pin as an output, setting the pin to the specified
 * value.
 *
 * @param pin Pin number to set as output
 * @param val Value to set pin
 *
 * @return int  0: no error; -1 otherwise.
 */
int
hal_gpio_init_out(int pin, int val)
{
    NRF_GPIO_Type *port;

    port = HAL_GPIO_PORT(pin);
    if (val) {
        port->OUTSET = HAL_GPIO_MASK(pin);
    } else {
        port->OUTCLR = HAL_GPIO_MASK(pin);
    }
    port->PIN_CNF[pin] = GPIO_PIN_CNF_DIR_Output;
    port->DIRSET = HAL_GPIO_MASK(pin);

    return 0;
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
    NRF_GPIO_Type *port;

    port = HAL_GPIO_PORT(pin);
    if (val) {
        port->OUTSET = HAL_GPIO_MASK(pin);
    } else {
        port->OUTCLR = HAL_GPIO_MASK(pin);
    }
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
    NRF_GPIO_Type *port;

    port = HAL_GPIO_PORT(pin);
    return ((port->IN & HAL_GPIO_MASK(pin)) != 0);
}

/**
 * gpio toggle
 *
 * Toggles the specified pin
 *
 * @param pin Pin number to toggle
 *
 * @return current pin state int 0: low, 1: high
 */
int
hal_gpio_toggle(int pin)
{
    int pin_state = (hal_gpio_read(pin) == 0);
    hal_gpio_write(pin, pin_state);
    return pin_state;
}

/*
 * GPIO irq handler
 *
 * Handles the gpio interrupt attached to a gpio pin.
 *
 * @param index
 */
static void
hal_gpio_irq_handler(void)
{
    int i;

    os_trace_enter_isr();

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (NRF_GPIOTE->EVENTS_IN[i]) {
            NRF_GPIOTE->EVENTS_IN[i] = 0;
            if (hal_gpio_irqs[i].func) {
                hal_gpio_irqs[i].func(hal_gpio_irqs[i].arg);
            }
        }
    }

    os_trace_exit_isr();
}

/*
 * Register IRQ handler for GPIOTE, and enable it.
 * Only executed once, during first registration.
 */
static void
hal_gpio_irq_setup(void)
{
    static uint8_t irq_setup = 0;

    if (!irq_setup) {
        NVIC_SetVector(GPIOTE_IRQn, (uint32_t)hal_gpio_irq_handler);
        NVIC_EnableIRQ(GPIOTE_IRQn);
        irq_setup = 1;
    }
}

/*
 * Find out whether we have an GPIOTE pin event to use.
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
 * Find the GPIOTE event which handles this pin.
 */
static int
hal_gpio_find_pin(int pin)
{
    int i;

    pin = pin << GPIOTE_CONFIG_PSEL_Pos;

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func &&
           (NRF_GPIOTE->CONFIG[i] & HAL_GPIOTE_PIN_MASK) == pin) {
            return i;
        }
    }
    return -1;
}

/**
 * gpio irq init
 *
 * Initialize an external interrupt on a gpio pin
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
    uint32_t conf;
    int i;

    hal_gpio_irq_setup();
    i = hal_gpio_find_empty_slot();
    if (i < 0) {
        return -1;
    }
    hal_gpio_init_in(pin, pull);

    switch (trig) {
    case HAL_GPIO_TRIG_RISING:
        conf = GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos;
        break;
    case HAL_GPIO_TRIG_FALLING:
        conf = GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos;
        break;
    case HAL_GPIO_TRIG_BOTH:
        conf = GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos;
        break;
    default:
        return -1;
    }
    conf |= pin << GPIOTE_CONFIG_PSEL_Pos;
    conf |= GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos;

    NRF_GPIOTE->CONFIG[i] = conf;

    hal_gpio_irqs[i].func = handler;
    hal_gpio_irqs[i].arg = arg;

    return 0;
}

/**
 * gpio irq release
 *
 * No longer interrupt when something occurs on the pin. NOTE: this function
 * does not change the GPIO push/pull setting.
 * It also does not disable the NVIC interrupt enable setting for the irq.
 *
 * @param pin
 */
void
hal_gpio_irq_release(int pin)
{
    int i;

    i = hal_gpio_find_pin(pin);
    if (i < 0) {
        return;
    }
    hal_gpio_irq_disable(i);

    NRF_GPIOTE->CONFIG[i] = 0;
    NRF_GPIOTE->EVENTS_IN[i] = 0;

    hal_gpio_irqs[i].arg = NULL;
    hal_gpio_irqs[i].func = NULL;
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
    int i;

    i = hal_gpio_find_pin(pin);
    if (i < 0) {
        return;
    }
    NRF_GPIOTE->EVENTS_IN[i] = 0;
    NRF_GPIOTE->INTENSET = 1 << i;
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
    int i;

    i = hal_gpio_find_pin(pin);
    if (i < 0) {
        return;
    }
    NRF_GPIOTE->INTENCLR = 1 << i;
}
