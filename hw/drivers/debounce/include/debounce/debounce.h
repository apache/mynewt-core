/**
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
#ifndef _DEBOUNCE_DEBOUNCE_H_
#define _DEBOUNCE_DEBOUNCE_H_

/*
 * debounce
 *
 * Driver to debounce a pin and optionally call a callback
 * when the state changes.
 *
 * The implementation registers an IRQ callback for the pin, once triggered
 * a periodic timer is used to check the state of the pin until it becomes
 * stable for a certain number of times.
 *
 * This way debouncing doesn't consume any processing resources unless its
 * pin actually changes and it is not susceptible to the thundering herd
 * problem of IRQs that can be triggered by noisy inputs. See
 * \c debounce_set_params for a detailed description of how the parameters
 * influence the minimum time a signal is required to remain stable for it
 * to be detected.
 *
 * The API relies on a structure of type \c debounce_pin_t to remain valid for the
 * lifetime of debouncing a pin. The structure can be dynamically allocated or
 * statically defined.
 *
 * // ---------------------- Example begin --------------------------
 *
 * void buttonPressed(debounce_pin_t *d) {
 *     if (debounce_state(d)) {
 *         ... code to process button press event
 *     } else {
 *         ... code to process button depress event
 *     }
 * }
 *
 * debounce_pin_t button;
 *
 * int main(int argc, char *argv[]) {
 *     ...
 *     debounce_init(&button, BUTTON_PIN, HAL_GPIO_PULL_UP, 0);
 *     debounce_start(&button, DEBOUNCE_CALLBACK_EVENT_ANY, buttonPressed, NULL);
 *     ...
 * }
 *
 * // ---------------------- Example end --------------------------
 *
 * The driver relies on
 *   hal_timer ... at least one HW timer needs to be configured
 *                 and running
 *   hal_gpio  ... specifically the irq interface is used in order
 *                 to detect the initial trigger for debouncing
 */

#include "hal/hal_gpio.h"
#include "hal/hal_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal structure for debouncing a pin. Application code should only
 * use the API to access it's data members and not directly manipulate or
 * retrieve any of them.
 */
typedef struct debounce_pin {
    uint32_t pin     : 24;
    uint32_t state   :  1;
    uint32_t on_rise :  1;
    uint32_t on_fall :  1;
    uint32_t         :  0;
    uint32_t ticks   : 16;
    uint32_t count   :  8;
    uint32_t accu    :  8;
    void (*on_change)(struct debounce_pin*);
    void *arg;
    struct hal_timer timer;
} debounce_pin_t;

/*
 * A callback can be invoked when the debounced pin's state changes. It
 * can either be invoked when the pin rises, falls or on any state change.
 * If either \c DEBOUNCE_CALLBACK_NEVER is specified or the callbak itself
 * is set to \c NULL no callback is invoked.
 */
typedef enum debounce_callback_event {
    DEBOUNCE_CALLBACK_NEVER      = 0, /* no callback invocation */
    DEBOUNCE_CALLBACK_EVENT_RISE = 1, /* callbak when signal rises */
    DEBOUNCE_CALLBACK_EVENT_FALL = 2, /* callbak when signal falls */
    DEBOUNCE_CALLBACK_EVENT_ANY  = 3  /* callbak when signal changes */
} debounce_callback_event_t;

/*
 * Prototype for the callback function. The callback has access to the pin,
 * current state and the initially provided argument through the API.
 * See also
 *  - debounce_arg
 *  - debounce_pin
 *  - debounce_state
 */
typedef void (*debounce_callback_t)(debounce_pin_t *);

/**
 * debounce init
 *
 * Initializes the specified pin to be debounced. Has to be called before
 * any other debouncing function on the given \c debounce structure.
 *
 * @param d         Structure to manage debouncing
 * @param pin       Pin number to set as input
 * @param pull      Pull type, see hal_gpio.h
 * @param timer     The HW timer number to be used by the debouncer.
 *                  The timer has to be configured and setup properly by the
 *                  application before this call.
 *
 * @return int  0: no error; -1 otherwise.
 */
int debounce_init(debounce_pin_t *d, int pin, hal_gpio_pull_t pull, int timer);

/**
 * debounce set params
 *
 * Can optionally be used to tune the debouncing parameters.
 * Once there is a pin change detected debouncing the checks the pin's state
 * periodically and requires it to be identical for a certain number of times.
 *
 * The critical time introduced by debouncing can be calculated by
 *   critical_time = timer_tick_period * ticks * count
 * This value describes the minim latency introduce by debouncing a pin. It
 * also represents the minimum time a pin has to be asserted or not for the
 * change to be propagated.
 *
 * The default values are defined by package values
 *   DEBOUNCE_PARAM_TICKS 1
 *   DEBOUNCE_PARAM_COUNT 10
 *
 *
 * @param d         Structure to manage debouncing
 * @param ticks     The # ticks used as the time interval for debouncing
 * @param count     The # times the pin state has to be stable for
 *                  debouncing to complete
 *
 * @return int  0: no error; -1 otherwise.
 */
int debounce_set_params(debounce_pin_t *d, uint16_t ticks, uint8_t count);

/**
 * debounce start
 *
 * Starts the debouncing algorithm an a previously initialized structure.
 *
 * @param d         Structure to manage debouncing
 * @param event     On which event(s) the callback should be invoked
 * @param cb        The callback
 * @param arg       Transparent argument available to the callback
 *
 * @return int  0: no error; -1 otherwise.
 */
int debounce_start(debounce_pin_t *d,
                   debounce_callback_event_t event,
                   debounce_callback_t cb,
                   void *arg);

/**
 * debounce stop
 *
 * Stops debouncing of a pin
 *
 * @param d         Structure to manage debouncing
 *
 * @return int  0: no error; -1 otherwise.
 */
int debounce_stop(debounce_pin_t*);

/**
 * debounce pin
 *
 * @param d         Structure to manage debouncing
 *
 * @return int  pin
 */
static inline int
debounce_pin(debounce_pin_t *d)
{
    return d->pin;
}

/**
 * debounce state
 *
 * @param d         Structure to manage debouncing
 *
 * @return int  state
 */
static inline int
debounce_state(debounce_pin_t *d)
{
    return d->state;
}

/**
 * debounce arg
 *
 * @param d         Structure to manage debouncing
 *
 * @return void*  arg
 */
static inline void*
debounce_arg(debounce_pin_t *d)
{
    return d->arg;
}

#ifdef __cplusplus
}
#endif

#endif /* _DEBOUNCE_DEBOUNCE_H_ */
