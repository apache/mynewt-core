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

#ifndef __ADC_NRF52_H__
#define __ADC_NRF52_H__

#include <adc/adc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADC_RESOLUTION_8BIT = 0,
    ADC_RESOLUTION_10BIT,
    ADC_RESOLUTION_12BIT,
    ADC_RESOLUTION_14BIT
} adc_resolution_t;

typedef enum {
    ADC_OVERSAMPLE_DISABLED = 0,
    ADC_OVERSAMPLE_2X,
    ADC_OVERSAMPLE_4X,
    ADC_OVERSAMPLE_8X,
    ADC_OVERSAMPLE_16X,
    ADC_OVERSAMPLE_32X,
    ADC_OVERSAMPLE_64X,
    ADC_OVERSAMPLE_128X,
    ADC_OVERSAMPLE_256X
} adc_oversample_t;

struct nrf52_adc_dev_cfg {
    uint16_t nadc_refmv;         /* Reference VDD in mV */
};

struct adc_dev_cfg {
    adc_resolution_t resolution;
    adc_oversample_t oversample; /* Only works on single channel mode */
    bool calibrate;              /* Calibrate before sampling */
};

typedef enum {
    ADC_REFERENCE_INTERNAL = 0,
    ADC_REFERENCE_VDD_DIV_4
} adc_ref_t;

typedef enum {
    ADC_GAIN1_6 = 0,
    ADC_GAIN1_5,
    ADC_GAIN1_4,
    ADC_GAIN1_3,
    ADC_GAIN1_2,
    ADC_GAIN1,
    ADC_GAIN2,
    ADC_GAIN4
} adc_gain_t;

typedef enum {
    ADC_ACQTIME_3US = 0,
    ADC_ACQTIME_5US,
    ADC_ACQTIME_10US,
    ADC_ACQTIME_15US,
    ADC_ACQTIME_20US,
    ADC_ACQTIME_40US
} adc_acq_time_t;

struct adc_chan_cfg {
    adc_gain_t gain;
    adc_ref_t reference;
    adc_acq_time_t acq_time;
    uint8_t pin;
    bool differential;
    uint8_t pin_negative;
};

int nrf52_adc_dev_init(struct os_dev *, void *);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_NRF52_H__ */
