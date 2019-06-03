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

#include <stdlib.h>
#include <assert.h>
#include "os/mynewt.h"
#include "hal/hal_gpio.h"
#include "mcu/cmsis_nvic.h"
#include "nrf.h"
#include "mcu/nrf52_hal.h"

/* XXX:
 * 1) The code probably does not handle "re-purposing" gpio very well.
 * "Re-purposing" means changing a gpio from input to output, or calling
 * gpio_init_in and expecting previously enabled interrupts to be stopped.
 *
 */

/* GPIO interrupts */
#define HAL_GPIO_MAX_IRQ        8

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
#define HAL_GPIO_SENSE_TRIG_NONE    0x00 /* just 0 */
#define HAL_GPIO_SENSE_TRIG_BOTH    0x01 /* something else than both below */
#define HAL_GPIO_SENSE_TRIG_HIGH    0x02 /* GPIO_PIN_CNF_SENSE_High */
#define HAL_GPIO_SENSE_TRIG_LOW     0x03 /* GPIO_PIN_CNF_SENSE_Low */
#endif

/* Storage for GPIO callbacks. */
struct hal_gpio_irq {
    hal_gpio_irq_handler_t func;
    void *arg;
#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    int pin;
    uint8_t sense_trig;
#endif
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
    int pin_index = HAL_GPIO_INDEX(pin);

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
    port->PIN_CNF[pin_index] = conf;
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
    int pin_index = HAL_GPIO_INDEX(pin);

    port = HAL_GPIO_PORT(pin);
    if (val) {
        port->OUTSET = HAL_GPIO_MASK(pin);
    } else {
        port->OUTCLR = HAL_GPIO_MASK(pin);
    }
    port->PIN_CNF[pin_index] = GPIO_PIN_CNF_DIR_Output |
        (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
    port->DIRSET = HAL_GPIO_MASK(pin);

    return 0;
}

/**
 * Deinitialize the specified pin to revert to default configuration
 *
 * @param pin Pin number to unset
 *
 * @return int  0: no error; -1 otherwise.
 */
int
hal_gpio_deinit(int pin)
{
    uint32_t conf;
    NRF_GPIO_Type *port;
    int pin_index = HAL_GPIO_INDEX(pin);

    conf = GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos;
    port = HAL_GPIO_PORT(pin);
    port->PIN_CNF[pin_index] = conf;

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

    return (port->DIR & HAL_GPIO_MASK(pin)) ?
        (port->OUT >> HAL_GPIO_INDEX(pin)) & 1UL :
        (port->IN >> HAL_GPIO_INDEX(pin)) & 1UL;
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
#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    NRF_GPIO_Type *nrf_gpio;
    int pin_index;
    int pin_state;
    uint8_t sense_trig;
#if NRF52840_XXAA
    uint64_t gpio_state;
#else
    uint32_t gpio_state;
#endif
#endif
    int i;

    os_trace_isr_enter();

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    NRF_GPIOTE->EVENTS_PORT = 0;

    gpio_state = NRF_P0->IN;
#if NRF52840_XXAA
    gpio_state |= (uint64_t)NRF_P1->IN << 32;
#endif
#endif

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
        if ((!hal_gpio_irqs[i].func) ||
            (hal_gpio_irqs[i].sense_trig == HAL_GPIO_SENSE_TRIG_NONE)) {
            continue;
        }

        nrf_gpio = HAL_GPIO_PORT(hal_gpio_irqs[i].pin);
        pin_index = HAL_GPIO_INDEX(hal_gpio_irqs[i].pin);

        /* Store current SENSE setting */
        sense_trig = (nrf_gpio->PIN_CNF[pin_index] & GPIO_PIN_CNF_SENSE_Msk) >> GPIO_PIN_CNF_SENSE_Pos;

        if (!sense_trig) {
            continue;
        }

        /*
         * SENSE values are 0x02 for high and 0x03 for low, so bit #0 is the
         * opposite of state which triggers interrupt (thus its value should be
         * different than pin state).
         */
        pin_state = (gpio_state >> hal_gpio_irqs[i].pin) & 0x01;
        if (pin_state == (sense_trig & 0x01)) {
            continue;
        }

        /*
         * Toggle sense to clear interrupt or allow detection of opposite edge
         * when trigger on both edges is requested.
         */
        nrf_gpio->PIN_CNF[pin_index] &= ~GPIO_PIN_CNF_SENSE_Msk;
        if (sense_trig == HAL_GPIO_SENSE_TRIG_HIGH) {
            nrf_gpio->PIN_CNF[pin_index] |= GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos;
        } else {
            nrf_gpio->PIN_CNF[pin_index] |= GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos;
        }

        /*
         * Call handler in case SENSE configuration matches requested one or
         * trigger on both edges is requested.
         */
        if ((hal_gpio_irqs[i].sense_trig == HAL_GPIO_SENSE_TRIG_BOTH) ||
            (hal_gpio_irqs[i].sense_trig == sense_trig)) {
            hal_gpio_irqs[i].func(hal_gpio_irqs[i].arg);
        }
#else
        if (NRF_GPIOTE->EVENTS_IN[i] && (NRF_GPIOTE->INTENSET & (1 << i))) {
            NRF_GPIOTE->EVENTS_IN[i] = 0;
            if (hal_gpio_irqs[i].func) {
                hal_gpio_irqs[i].func(hal_gpio_irqs[i].arg);
            }
        }
#endif
    }

    os_trace_isr_exit();
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

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
        NRF_GPIOTE->INTENCLR = GPIOTE_INTENCLR_PORT_Msk;
        NRF_GPIOTE->EVENTS_PORT = 0;
#endif
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

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func && hal_gpio_irqs[i].pin == pin) {
            return i;
        }
    }
#else
    pin = pin << GPIOTE_CONFIG_PSEL_Pos;

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func &&
           (NRF_GPIOTE->CONFIG[i] & HAL_GPIOTE_PIN_MASK) == pin) {
            return i;
        }
    }
#endif

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

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    (void)conf;
    hal_gpio_irqs[i].pin = pin;

    switch (trig) {
    case HAL_GPIO_TRIG_RISING:
        hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_HIGH;
        break;
    case HAL_GPIO_TRIG_FALLING:
        hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_LOW;
        break;
    case HAL_GPIO_TRIG_BOTH:
        hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_BOTH;
        break;
    default:
        hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_NONE;
        return -1;
    }
#else
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
#endif

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
    hal_gpio_irq_disable(pin);

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_NONE;
#else
    NRF_GPIOTE->CONFIG[i] = 0;
    NRF_GPIOTE->EVENTS_IN[i] = 0;
#endif

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
#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    NRF_GPIO_Type *nrf_gpio;
    int pin_index;
#endif
    int i;

    i = hal_gpio_find_pin(pin);
    if (i < 0) {
        return;
    }

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    nrf_gpio = HAL_GPIO_PORT(pin);
    pin_index = HAL_GPIO_INDEX(pin);

    nrf_gpio->PIN_CNF[pin_index] &= ~GPIO_PIN_CNF_SENSE_Msk;

    /*
     * Always set initial SENSE to opposite of current pin state to avoid
     * triggering immediately
     */
    if (nrf_gpio->IN & (1 << pin_index)) {
        nrf_gpio->PIN_CNF[pin_index] |= GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos;
    } else {
        nrf_gpio->PIN_CNF[pin_index] |= GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos;
    }

    NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Msk;
#else
    NRF_GPIOTE->EVENTS_IN[i] = 0;
    NRF_GPIOTE->INTENSET = 1 << i;
#endif
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
#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    NRF_GPIO_Type *nrf_gpio;
    int pin_index;
    bool sense_enabled = false;
#endif
    int i;

    i = hal_gpio_find_pin(pin);
    if (i < 0) {
        return;
    }

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    nrf_gpio = HAL_GPIO_PORT(pin);
    pin_index = HAL_GPIO_INDEX(pin);

    nrf_gpio->PIN_CNF[pin_index] &= ~GPIO_PIN_CNF_SENSE_Msk;

    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].sense_trig != HAL_GPIO_SENSE_TRIG_NONE) {
            sense_enabled = true;
            break;
        }
    }

    if (!sense_enabled) {
        NRF_GPIOTE->INTENCLR = GPIOTE_INTENCLR_PORT_Msk;
    }
#else
    NRF_GPIOTE->INTENCLR = 1 << i;
#endif
}
