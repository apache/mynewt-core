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

#include <stm32_common/mcu.h>

#include <stm32f4xx_hal_dma.h>
#include <stm32f4xx_hal_adc.h>
#include <stm32f4xx_hal_rcc.h>
#include <stm32f4xx_hal_gpio.h>
#include <mcu/stm32_hal.h>
#include <adc_touch.h>

#define ADC_ASYNC   1

static struct adc_pin {
    int pin;
    uint32_t adc_channel;
} const adc_pins[] = {
    {MCU_GPIO_PORTA(0), ADC_CHANNEL_0},
    {MCU_GPIO_PORTA(1), ADC_CHANNEL_1},
    {MCU_GPIO_PORTA(2), ADC_CHANNEL_2},
    {MCU_GPIO_PORTA(3), ADC_CHANNEL_3},
    {MCU_GPIO_PORTA(4), ADC_CHANNEL_4},
    {MCU_GPIO_PORTA(5), ADC_CHANNEL_5},
    {MCU_GPIO_PORTA(6), ADC_CHANNEL_6},
    {MCU_GPIO_PORTA(7), ADC_CHANNEL_7},
    {MCU_GPIO_PORTB(0), ADC_CHANNEL_8},
    {MCU_GPIO_PORTB(1), ADC_CHANNEL_9},
    {MCU_GPIO_PORTC(0), ADC_CHANNEL_10},
    {MCU_GPIO_PORTC(1), ADC_CHANNEL_11},
    {MCU_GPIO_PORTC(2), ADC_CHANNEL_12},
    {MCU_GPIO_PORTC(3), ADC_CHANNEL_13},
    {MCU_GPIO_PORTC(4), ADC_CHANNEL_14},
    {MCU_GPIO_PORTC(5), ADC_CHANNEL_15},
};

#define ADC_CHANNEL_NONE 0xFFFFFFFF

static ADC_ChannelConfTypeDef adc_x = {
    .Channel = ADC_CHANNEL_4,
    .Rank = 1,
    .SamplingTime = ADC_SAMPLETIME_84CYCLES,
    .Offset = 0,
};

static ADC_ChannelConfTypeDef adc_y = {
    .Channel = ADC_CHANNEL_1,
    .Rank = 1,
    .SamplingTime = ADC_SAMPLETIME_84CYCLES,
    .Offset = 0,
};

static int adc_x_pin;
static int adc_y_pin;

ADC_HandleTypeDef adc_handle = {
    .Instance = ADC1,
    .Init = {
        .ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2,
        .Resolution = ADC_RESOLUTION_12B,
        .DataAlign = ADC_DATAALIGN_RIGHT,
        .ScanConvMode = DISABLE,
        .EOCSelection = DISABLE,
        .ContinuousConvMode = DISABLE,
        .NbrOfConversion = 1,
        .DiscontinuousConvMode = DISABLE,
        .NbrOfDiscConversion = 0,
        .ExternalTrigConv = ADC_SOFTWARE_START,
        .ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE,
        .DMAContinuousRequests = DISABLE,
    },
};

void
ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&adc_handle);
}

adc_dev_t
adc_touch_adc_open(int x_pin, int y_pin)
{
    for (int i = 0; i < ARRAY_SIZE(adc_pins); ++i) {
        if (x_pin == adc_pins[i].pin) {
            adc_x.Channel = adc_pins[i].adc_channel;
            adc_x_pin = x_pin;
        }
        if (y_pin == adc_pins[i].pin) {
            adc_y.Channel = adc_pins[i].adc_channel;
            adc_y_pin = y_pin;
        }
    }
    __HAL_RCC_ADC1_CLK_ENABLE();
    HAL_ADC_Init(&adc_handle);
    NVIC_SetVector(ADC_IRQn, (uint32_t)ADC_IRQHandler);
    assert(adc_x.Channel != ADC_CHANNEL_NONE);
    assert(adc_y.Channel != ADC_CHANNEL_NONE);

    return &adc_handle;
}

static struct os_sem adc_sem;

void
HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    (void)hadc;

    os_sem_release(&adc_sem);
}

uint16_t
adc_touch_adc_read(adc_dev_t adc, int pin)
{
    int val = -1;
    int rc;
    ADC_HandleTypeDef *stm_adc = adc;
    ADC_ChannelConfTypeDef *channel_config = NULL;
    GPIO_InitTypeDef gpio_analog_init = {
        .Mode = GPIO_MODE_ANALOG,
        .Pull = GPIO_NOPULL,
        .Alternate = 0
    };

    os_sem_init(&adc_sem, 0);

    if (pin == adc_x_pin) {
        channel_config = &adc_x;
    } else if (pin == adc_y_pin) {
        channel_config = &adc_y;
    }
    if (channel_config) {
        hal_gpio_init_stm(pin, &gpio_analog_init);
        HAL_ADC_ConfigChannel(stm_adc, channel_config);
        NVIC_ClearPendingIRQ(ADC_IRQn);
        if (ADC_ASYNC) {
            rc = HAL_ADC_Start_IT(stm_adc);
            if (rc == HAL_OK) {
                NVIC_EnableIRQ(ADC_IRQn);
                if (os_sem_pend(&adc_sem, 1000) == OS_TIMEOUT) {
                    HAL_ADC_Stop_IT(stm_adc);
                } else {
                    val = (uint16_t) HAL_ADC_GetValue(stm_adc);
                }
                NVIC_DisableIRQ(ADC_IRQn);
            }
        } else {
            rc = HAL_ADC_Start(stm_adc);
            if (rc == HAL_OK) {
                rc = HAL_ADC_PollForConversion(stm_adc, 1000);
                if (rc == HAL_OK) {
                    val = (uint16_t) HAL_ADC_GetValue(stm_adc);
                }
            }
        }
    }

    return (uint16_t)val;
}
