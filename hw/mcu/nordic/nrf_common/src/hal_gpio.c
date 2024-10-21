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
#include "os/mynewt.h"
#include "hal/hal_gpio.h"
#include "nrf.h"
#include "nrf_hal.h"
#include "nrfx_config.h"
#include "nrfx_gpiote.h"

/* GPIO interrupts */
#define HAL_GPIO_MAX_IRQ        GPIOTE_CH_NUM

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
#define HAL_GPIO_SENSE_TRIG_NONE    0x00 /* just 0 */
#define HAL_GPIO_SENSE_TRIG_BOTH    0x01 /* something else than both below */
#define HAL_GPIO_SENSE_TRIG_HIGH    0x02 /* GPIO_PIN_CNF_SENSE_High */
#define HAL_GPIO_SENSE_TRIG_LOW     0x03 /* GPIO_PIN_CNF_SENSE_Low */
#endif

#if defined(NRF5340_XXAA_APPLICATION) || defined(NRF9160_XXAA)
#define GPIOTE_IRQn GPIOTE0_IRQn
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
    switch (pull) {
    case HAL_GPIO_PULL_UP:
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
        break;
    case HAL_GPIO_PULL_DOWN:
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
        break;
    case HAL_GPIO_PULL_NONE:
    default:
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
        break;
    }

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
    nrf_gpio_cfg_output(pin);
    nrf_gpio_pin_write(pin, val);

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
    nrf_gpio_cfg_default(pin);

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
    nrf_gpio_pin_write(pin, val);
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
    if (nrf_gpio_pin_dir_get(pin) == NRF_GPIO_PIN_DIR_OUTPUT) {
        return nrf_gpio_pin_out_read(pin);
    } else {
        return nrf_gpio_pin_read(pin);
    }
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
    nrf_gpio_pin_toggle(pin);
    return hal_gpio_read(pin);
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
// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//     uint8_t sense_trig;
// #endif
//     int i;
//     os_trace_isr_enter();

// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//     nrf_gpiote_event_clear(NRF_GPIOTE0, NRF_GPIOTE_EVENT_PORT);
// #endif

//     for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//         if ((!hal_gpio_irqs[i].func) ||
//             (hal_gpio_irqs[i].sense_trig == HAL_GPIO_SENSE_TRIG_NONE)) {
//             continue;
//         }

//         /* Store current SENSE setting */
//         sense_trig = nrf_gpio_pin_sense_get(hal_gpio_irqs[i].pin);

//         if (sense_trig == HAL_GPIO_SENSE_TRIG_NONE) {
//             continue;
//         }

//         /*
//          * SENSE values are 0x02 for high and 0x03 for low, so bit #0 is the
//          * opposite of state which triggers interrupt (thus its value should be
//          * different than pin state).
//          */
//         if (nrf_gpio_pin_read(hal_gpio_irqs[i].pin) == (sense_trig & 0x01)) {
//             continue;
//         }

//         /*
//          * Toggle sense to clear interrupt or allow detection of opposite edge
//          * when trigger on both edges is requested.
//          */
//         if (sense_trig == HAL_GPIO_SENSE_TRIG_HIGH) {
//             nrf_gpio_cfg_sense_set(hal_gpio_irqs[i].pin, HAL_GPIO_SENSE_TRIG_LOW);
//         } else {
//             nrf_gpio_cfg_sense_set(hal_gpio_irqs[i].pin, HAL_GPIO_SENSE_TRIG_HIGH);
//         }

//         /*
//          * Call handler in case SENSE configuration matches requested one or
//          * trigger on both edges is requested.
//          */
//         if ((hal_gpio_irqs[i].sense_trig == HAL_GPIO_SENSE_TRIG_BOTH) ||
//             (hal_gpio_irqs[i].sense_trig == sense_trig)) {
//             hal_gpio_irqs[i].func(hal_gpio_irqs[i].arg);
//         }
// #else
//         if (nrf_gpiote_event_check(NRF_GPIOTE0, nrf_gpiote_in_event_get(i)) &&
//             nrf_gpiote_int_enable_check(NRF_GPIOTE0, 1 << i)) {
//             nrf_gpiote_event_clear(NRF_GPIOTE0, nrf_gpiote_in_event_get(i));
//             if (hal_gpio_irqs[i].func) {
//                 hal_gpio_irqs[i].func(hal_gpio_irqs[i].arg);
//             }
//         }
// #endif
//     }

//     os_trace_isr_exit();
}

/*
 * Register IRQ handler for GPIOTE, and enable it.
 * Only executed once, during first registration.
 */
static void
hal_gpio_irq_setup(void)
{
//     static uint8_t irq_setup = 0;

//     if (!irq_setup) {
//         NVIC_SetVector(GPIOTE_IRQn, (uint32_t)hal_gpio_irq_handler);
//         NVIC_EnableIRQ(GPIOTE_IRQn);
//         irq_setup = 1;

// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//         nrf_gpiote_int_disable(NRF_GPIOTE0, GPIOTE_INTENCLR_PORT_Msk);
//         nrf_gpiote_event_clear(NRF_GPIOTE0, NRF_GPIOTE_EVENT_PORT);
// #endif
//     }
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
hal_gpio_get_gpiote_num(int pin)
{
    int i;

#if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func && hal_gpio_irqs[i].pin == pin) {
            return i;
        }
    }
#else
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func &&
           (nrf_gpiote_event_pin_get(NRF_GPIOTE0, i) == pin)) {
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
    int i, sr;

    assert(0);
//     __HAL_DISABLE_INTERRUPTS(sr);

//     hal_gpio_irq_setup();
//     i = hal_gpio_find_empty_slot();
//     if (i < 0) {
//         __HAL_ENABLE_INTERRUPTS(sr);
//         return -1;
//     }
//     hal_gpio_init_in(pin, pull);

// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//     hal_gpio_irqs[i].pin = pin;

//     switch (trig) {
//     case HAL_GPIO_TRIG_RISING:
//         hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_HIGH;
//         break;
//     case HAL_GPIO_TRIG_FALLING:
//         hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_LOW;
//         break;
//     case HAL_GPIO_TRIG_BOTH:
//         hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_BOTH;
//         break;
//     default:
//         hal_gpio_irqs[i].sense_trig = HAL_GPIO_SENSE_TRIG_NONE;
//         __HAL_ENABLE_INTERRUPTS(sr);
//         return -1;
//     }
// #else
//     switch (trig) {
//     case HAL_GPIO_TRIG_RISING:
//         nrf_gpiote_event_configure(NRF_GPIOTE0, i, pin, GPIOTE_CONFIG_POLARITY_LoToHi);
//         break;
//     case HAL_GPIO_TRIG_FALLING:
//         nrf_gpiote_event_configure(NRF_GPIOTE0, i, pin, GPIOTE_CONFIG_POLARITY_HiToLo);
//         break;
//     case HAL_GPIO_TRIG_BOTH:
//         nrf_gpiote_event_configure(NRF_GPIOTE0, i, pin, GPIOTE_CONFIG_POLARITY_Toggle);
//         break;
//     default:
//         __HAL_ENABLE_INTERRUPTS(sr);
//         return -1;
//     }

//     nrf_gpiote_event_enable(NRF_GPIOTE0, i);
// #endif

//     hal_gpio_irqs[i].func = handler;
//     hal_gpio_irqs[i].arg = arg;

//     __HAL_ENABLE_INTERRUPTS(sr);

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
//     int i, sr;

//     __HAL_DISABLE_INTERRUPTS(sr);

//     i = hal_gpio_get_gpiote_num(pin);
//     if (i < 0) {
//         __HAL_ENABLE_INTERRUPTS(sr);
//         return;
//     }
//     hal_gpio_irq_disable(pin);

// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//     hal_gpio_irqs[i].sense_trig = NRF_GPIO_PIN_NOSENSE;
// #else
//     nrf_gpiote_te_default(NRF_GPIOTE0, i);
//     nrf_gpiote_event_clear(NRF_GPIOTE0, nrf_gpiote_in_event_get(i));
// #endif

//     hal_gpio_irqs[i].arg = NULL;
//     hal_gpio_irqs[i].func = NULL;

//     __HAL_ENABLE_INTERRUPTS(sr);
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
    int i, sr;

//     __HAL_DISABLE_INTERRUPTS(sr);

//     i = hal_gpio_get_gpiote_num(pin);
//     if (i < 0) {
//         __HAL_ENABLE_INTERRUPTS(sr);
//         return;
//     }

// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//     /*
//      * Always set initial SENSE to opposite of current pin state to avoid
//      * triggering immediately
//      */
//     if (nrf_gpio_pin_read(pin)) {
//         nrf_gpio_cfg_sense_set(pin, GPIO_PIN_CNF_SENSE_Low);
//     } else {
//         nrf_gpio_cfg_sense_set(pin, GPIO_PIN_CNF_SENSE_High);
//     }

//     nrf_gpiote_int_enable(NRF_GPIOTE0, GPIOTE_INTENSET_PORT_Msk);
// #else
//     nrf_gpiote_event_clear(NRF_GPIOTE0, nrf_gpiote_in_event_get(i));
//     nrf_gpiote_int_enable(NRF_GPIOTE0, 1 << i);
// #endif

//     __HAL_ENABLE_INTERRUPTS(sr);
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
// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//     bool sense_enabled = false;
// #endif
//     int i, sr;

//     __HAL_DISABLE_INTERRUPTS(sr);

//     i = hal_gpio_get_gpiote_num(pin);
//     if (i < 0) {
//         __HAL_ENABLE_INTERRUPTS(sr);
//         return;
//     }

// #if MYNEWT_VAL(MCU_GPIO_USE_PORT_EVENT)
//     for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
//         if (hal_gpio_irqs[i].sense_trig != HAL_GPIO_SENSE_TRIG_NONE) {
//             sense_enabled = true;
//             break;
//         }
//     }

//     if (!sense_enabled) {
//         nrf_gpiote_int_disable(NRF_GPIOTE0, GPIOTE_INTENSET_PORT_Msk);
//     }
// #else
//     nrf_gpiote_int_disable(NRF_GPIOTE0, 1 << i);
// #endif
//     __HAL_ENABLE_INTERRUPTS(sr);
}
