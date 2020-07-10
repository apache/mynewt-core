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


/* define LED routines if LED pins are defined for the target */
#include <bsp/bsp.h>
#ifdef LED_BLINK_PIN
#include <stats/stats.h>
#include <hal/hal_system.h>
#include <hal/hal_gpio.h>
#include <log/log.h>

static int g_led_pin;

STATS_SECT_START(gpio_stats)
STATS_SECT_ENTRY(toggles)
STATS_SECT_END

static STATS_NAME_START(gpio_stats)
STATS_NAME(gpio_stats, toggles)
STATS_NAME_END(gpio_stats)

static STATS_SECT_DECL(gpio_stats) g_stats_gpio_toggle;

void
init_led_stats(void)
{
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    stats_init(STATS_HDR(g_stats_gpio_toggle),
               STATS_SIZE_INIT_PARMS(g_stats_gpio_toggle, STATS_SIZE_32),
               STATS_NAME_INIT_PARMS(gpio_stats));

    stats_register("gpio_toggle", STATS_HDR(g_stats_gpio_toggle));
}

void
toggle_led(void)
{
    int prev_pin_state, curr_pin_state;

    prev_pin_state = hal_gpio_read(g_led_pin);
    curr_pin_state = hal_gpio_toggle(g_led_pin);
    DFLT_LOG_INFO("GPIO toggle from %u to %u",
                    prev_pin_state, curr_pin_state);
    STATS_INC(g_stats_gpio_toggle, toggles);
}

#else
void
init_led_stats(void)
{
}

void
toggle_led(void)
{
}
#endif
