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
#ifndef STM32_COMMON_HAL_H
#define STM32_COMMON_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mcu/stm32_hal.h>

#define UART_CNT (((uint8_t)(MYNEWT_VAL(UART_0) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_1) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_2) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_3) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_4) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_5) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_6) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_7) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_8) != 0)) + \
                  ((uint8_t)(MYNEWT_VAL(UART_9) != 0)))

#define PWM_CNT (((uint8_t)(MYNEWT_VAL(PWM_0) != 0)) + \
                 ((uint8_t)(MYNEWT_VAL(PWM_1) != 0)) + \
                 ((uint8_t)(MYNEWT_VAL(PWM_2) != 0)))

uint32_t stm32_hal_timer_get_freq(void *timx);
void stm32_periph_create(void);

#ifndef STM32_HAL_TIMER_TIM1_IRQ
#define STM32_HAL_TIMER_TIM1_IRQ    TIM1_UP_TIM10_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM2_IRQ
#define STM32_HAL_TIMER_TIM2_IRQ    TIM2_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM3_IRQ
#define STM32_HAL_TIMER_TIM3_IRQ    TIM3_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM4_IRQ
#define STM32_HAL_TIMER_TIM4_IRQ    TIM4_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM6_IRQ
#define STM32_HAL_TIMER_TIM6_IRQ    TIM6_DAC_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM7_IRQ
#define STM32_HAL_TIMER_TIM7_IRQ    TIM7_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM8_IRQ
#define STM32_HAL_TIMER_TIM8_IRQ    TIM8_UP_TIM13_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM9_IRQ
#define STM32_HAL_TIMER_TIM9_IRQ    TIM1_BRK_TIM9_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM10_IRQ
#define STM32_HAL_TIMER_TIM10_IRQ   TIM1_UP_TIM10_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM11_IRQ
#define STM32_HAL_TIMER_TIM11_IRQ   TIM1_TRG_COM_TIM11_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM12_IRQ
#define STM32_HAL_TIMER_TIM12_IRQ   TIM8_BRK_TIM12_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM13_IRQ
#define STM32_HAL_TIMER_TIM13_IRQ   TIM8_UP_TIM13_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM14_IRQ
#define STM32_HAL_TIMER_TIM14_IRQ   TIM8_TRG_COM_TIM14_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM15_IRQ
#define STM32_HAL_TIMER_TIM15_IRQ   TIM1_BRK_TIM15_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM16_IRQ
#define STM32_HAL_TIMER_TIM16_IRQ   TIM1_UP_TIM16_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM17_IRQ
#define STM32_HAL_TIMER_TIM17_IRQ   TIM1_TRG_COM_TIM17_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM21_IRQ
#define STM32_HAL_TIMER_TIM21_IRQ   TIM21_IRQn
#endif
#ifndef STM32_HAL_TIMER_TIM22_IRQ
#define STM32_HAL_TIMER_TIM22_IRQ   TIM22_IRQn
#endif

#ifdef __cplusplus
}
#endif

#endif
