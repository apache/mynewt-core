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

#include <string.h>
#include "os/mynewt.h"
#include "debounce/debounce.h"

static void
debounce_check(void *arg)
{
    debounce_pin_t *d = (debounce_pin_t*)arg;

    int32_t btn = hal_gpio_read(d->pin) ? +1 : -1;
    int32_t integrate = (int32_t)d->accu + btn;

    if (integrate >= d->count) {
        if (0 == d->state) {
            d->state = 1;
            if (d->on_rise && d->on_change) {
                d->on_change(d);
            }
        }
        integrate = d->count;
    } else if ( integrate <= 0) {
        if (1 == d->state) {
            d->state = 0;
            if (d->on_fall && d->on_change) {
                d->on_change(d);
            }
        }
        integrate = 0;
    }
    d->accu = integrate;

    if (0 == integrate || d->count == integrate) {
        /* debouncing complete, no point in periodically updating */
        hal_gpio_irq_enable(d->pin);
    } else {
        /* no decision yet, continue debouncing */
        hal_timer_start(&d->timer, d->ticks);
    }
}

static void
debounce_trigger(void *arg)
{
    debounce_pin_t *d = (debounce_pin_t*)arg;

    /* once triggered, switch to periodic checks */
    hal_gpio_irq_disable(d->pin);
    hal_timer_start(&d->timer, d->ticks);
}


int
debounce_init(debounce_pin_t *d, int pin, hal_gpio_pull_t pull, int timer)
{
    memset(d, 0, sizeof(debounce_pin_t));
    d->pin = pin;
    d->ticks = MYNEWT_VAL(DEBOUNCE_PARAM_TICKS);
    d->count = MYNEWT_VAL(DEBOUNCE_PARAM_COUNT);

    if (hal_timer_set_cb(timer, &d->timer, debounce_check, d)) {
        return -1;
    }

    if (hal_gpio_irq_init(pin, debounce_trigger, d, HAL_GPIO_TRIG_BOTH, pull)) {
        return -1;
    }

    if (hal_gpio_read(pin)) {
        d->state = 1;
        d->accu = d->count;
    }

    return 0;
}

int
debounce_set_params(debounce_pin_t *d, uint16_t ticks, uint8_t count)
{
    d->ticks = ticks;
    d->count = count;
    if (d->state) {
        d->accu = count;
    }

    return 0;
}

int
debounce_start(debounce_pin_t *d,
               debounce_callback_event_t event,
               debounce_callback_t cb,
               void *arg)
{
    d->on_rise = (event & DEBOUNCE_CALLBACK_EVENT_RISE) ? 1 : 0;
    d->on_fall = (event & DEBOUNCE_CALLBACK_EVENT_FALL) ? 1 : 0;
    d->on_change = cb;
    d->arg = arg;

    hal_gpio_irq_enable(d->pin);
    return 0;
}

int
debounce_stop(debounce_pin_t *d)
{
    hal_gpio_irq_disable(d->pin);
    hal_timer_stop(&d->timer);
    return 0;
}
