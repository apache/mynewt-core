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

#ifndef __ADC_APOLLO3_H__
#define __ADC_APOLLO3_H__

#include <adc/adc.h>
#include "am_mcu_apollo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_APOLLO3_TMR_NUM

enum apollo3_adc_clock_num_e {
    APOLLO3_ADC_CLOCK_0 = 0,
    APOLLO3_ADC_CLOCK_1,
    APOLLO3_ADC_CLOCK_2,
    APOLLO3_ADC_CLOCK_3,
    APOLLO3_ADC_CLOCK_CNT
};

enum apollo3_adc_timer_ab_e {
    APOLLO3_ADC_TIMER_A = 0,
    APOLLO3_ADC_TIMER_B,
    APOLLO3_ADC_TIMER_BOTH
};
#define APOLLO3_ADC_TIMER_AB_CNT APOLLO3_ADC_TIMER_BOTH

enum apollo3_adc_timer_func_e {
    APOLLO3_ADC_TIMER_FUNC_ONCE = 0,
    APOLLO3_ADC_TIMER_FUNC_REPEAT,
    APOLLO3_ADC_TIMER_FUNC_PWM_ONCE,
    APOLLO3_ADC_TIMER_FUNC_PWM_REPEAT,
    APOLLO3_ADC_TIMER_FUNC_CONTINUOUS
};

struct apollo3_adc_clk_cfg {
    uint32_t clk_freq; /* Desired freq of clock */
    uint32_t clk_period; /* Period for ADC clock */
    uint32_t clk_on_time; /* Number of clocks in which output signal is high */
    uint8_t clk_num:3; /* Clock number to use for ADC */
    uint8_t timer_ab:2; /* A and B each hold 16 bits, you can use both for 32 bits */
    uint8_t timer_func:3; /* Timers on apollo3 have 5 functions to choose from */
};

struct adc_cfg {
    am_hal_adc_config_t adc_cfg;
    am_hal_adc_slot_config_t adc_slot_cfg;
    am_hal_adc_dma_config_t adc_dma_cfg;
    struct apollo3_adc_clk_cfg clk_cfg;
};

int apollo3_adc_dev_init(struct os_dev *, void *);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_APOLLO3_H__ */
