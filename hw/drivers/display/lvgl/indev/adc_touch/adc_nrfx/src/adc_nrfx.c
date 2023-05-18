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

#include <assert.h>

#include <os/os.h>

#include <adc_nrf52/adc_nrf52.h>
#include <nrf_saadc.h>
#include <adc_touch.h>

static adc_dev_t adc_dev;

static struct ain_pin {
    int pin;
    nrf_saadc_input_t ain;
} const ain_pins[8] = {
#if defined (NRF52)
    {2, NRF_SAADC_INPUT_AIN0},
    {3, NRF_SAADC_INPUT_AIN1},
    {4, NRF_SAADC_INPUT_AIN2},
    {5, NRF_SAADC_INPUT_AIN3},
    {28, NRF_SAADC_INPUT_AIN4},
    {29, NRF_SAADC_INPUT_AIN5},
    {30, NRF_SAADC_INPUT_AIN6},
    {31, NRF_SAADC_INPUT_AIN7},
#elif defined (NRF53_SERIES)
    {4, NRF_SAADC_INPUT_AIN0},
    {5, NRF_SAADC_INPUT_AIN1},
    {6, NRF_SAADC_INPUT_AIN2},
    {7, NRF_SAADC_INPUT_AIN3},
    {25, NRF_SAADC_INPUT_AIN4},
    {26, NRF_SAADC_INPUT_AIN5},
    {27, NRF_SAADC_INPUT_AIN6},
    {28, NRF_SAADC_INPUT_AIN7},
#endif
};

static struct adc_chan_cfg adc_x = {
    .acq_time = ADC_ACQTIME_40US,
    .differential = false,
    .gain = ADC_GAIN1_4,
    .pin = NRF_SAADC_INPUT_DISABLED,
    .pin_negative = NRF_SAADC_INPUT_DISABLED,
    .reference = ADC_REFERENCE_VDD_DIV_4,
};

static struct adc_chan_cfg adc_y = {
    .acq_time = ADC_ACQTIME_40US,
    .differential = false,
    .gain = ADC_GAIN1_4,
    .pin = NRF_SAADC_INPUT_DISABLED,
    .pin_negative = NRF_SAADC_INPUT_DISABLED,
    .reference = ADC_REFERENCE_VDD_DIV_4,
};

static int adc_x_pin;
static int adc_y_pin;

adc_dev_t
adc_touch_adc_open(int x_pin, int y_pin)
{
    struct adc_dev_cfg adc_cfg = {
        .calibrate = 1,
        .oversample = ADC_OVERSAMPLE_DISABLED,
        .resolution = ADC_RESOLUTION_14BIT,
    };
    for (int i = 0; i < ARRAY_SIZE(ain_pins); ++i) {
        if (x_pin == ain_pins[i].pin) {
            adc_x.pin = ain_pins[i].ain;
            adc_x_pin = x_pin;
        }
        if (y_pin == ain_pins[i].pin) {
            adc_y.pin = ain_pins[i].ain;
            adc_y_pin = y_pin;
        }
    }
    assert(adc_x.pin != NRF_SAADC_INPUT_DISABLED);
    assert(adc_y.pin != NRF_SAADC_INPUT_DISABLED);
    adc_dev = (struct adc_dev *)os_dev_open("adc0", 0, &adc_cfg);
    adc_chan_config(adc_dev, 0, &adc_x);
    adc_chan_config(adc_dev, 1, &adc_y);

    return adc_dev;
}

uint16_t
adc_touch_adc_read(adc_dev_t adc, int pin)
{
    int val = -1;

    (void)adc;

    if (pin == adc_x_pin) {
        adc_read_channel(adc_dev, 0, &val);
    } else if (pin == adc_y_pin) {
        adc_read_channel(adc_dev, 1, &val);
    }

    return (uint16_t)val;
}
