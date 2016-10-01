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

#ifndef __ADC_STM32F4_H__
#define __ADC_STM32F4_H__

#include <adc/adc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mynewt HAL Port pin mapping */
#define ADC123_CH0_PIN    0
#define ADC123_CH1_PIN    1
#define ADC123_CH2_PIN    2
#define ADC123_CH3_PIN    3
#define ADC12_CH4_PIN     4
#define ADC12_CH5_PIN     5
#define ADC12_CH6_PIN     6
#define ADC12_CH7_PIN     7
#define ADC12_CH8_PIN     16
#define ADC12_CH9_PIN     17
#define ADC3_CH4_PIN      86
#define ADC3_CH5_PIN      87
#define ADC3_CH6_PIN      88
#define ADC3_CH7_PIN      89
#define ADC3_CH8_PIN      90
#define ADC3_CH9_PIN      83
#define ADC123_CH10_PIN   32
#define ADC123_CH11_PIN   33
#define ADC123_CH12_PIN   34
#define ADC123_CH13_PIN   35
#define ADC12_CH14_PIN    36
#define ADC12_CH15_PIN    37
#define ADC3_CH14_PIN     84
#define ADC3_CH15_PIN     85

struct stm32f4_adc_dev_cfg {
    uint8_t sac_chan_count;
    void *sac_chans;
    void *primarybuf;
    void *secondarybuf;
    int buflen;
    ADC_HandleTypeDef *sac_adc_handle;
};

int stm32f4_adc_dev_init(struct os_dev *, void *);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */
