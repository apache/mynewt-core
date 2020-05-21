/*
 * Copyright 2020 Casper Meijn <casper@meijn.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "temp_nrf52/temp_nrf52.h"
#include <nrf_temp.h>

static struct temperature_dev *global_temp_dev;

static temperature_t
nrf52_temp_convert(int32_t raw_measurement)
{
    return (raw_measurement * 100) / 4;
}

int
nrf52_temp_sample(struct temperature_dev * temp_dev)
{
    nrf_temp_task_trigger(NRF_TEMP, NRF_TEMP_TASK_START);
    return 0;
}

static void
nrf52_temp_irq_handler(void)
{
    if (nrf_temp_event_check(NRF_TEMP, NRF_TEMP_EVENT_DATARDY)) {
        nrf_temp_event_clear(NRF_TEMP, NRF_TEMP_EVENT_DATARDY);

        int32_t raw_measurement = nrf_temp_result_get(NRF_TEMP);
        temperature_t temperature = nrf52_temp_convert(raw_measurement);
        temp_sample_completed(global_temp_dev, temperature);
    }
}

int
nrf52_temp_dev_init(struct os_dev *dev, void *arg)
{
    struct temperature_dev *temp_dev;

    temp_dev = (struct temperature_dev *)dev;
    assert(temp_dev);

    temp_dev->temp_funcs.temp_sample = nrf52_temp_sample;

    global_temp_dev = temp_dev;
    NVIC_SetPriority(TEMP_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(TEMP_IRQn, (uint32_t) nrf52_temp_irq_handler);
    NVIC_EnableIRQ(TEMP_IRQn);
    nrf_temp_int_enable(NRF_TEMP, 1);

    return 0;
}
