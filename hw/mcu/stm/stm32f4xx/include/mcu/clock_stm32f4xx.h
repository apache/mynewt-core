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

#ifndef __MCU_CLOCK_STM32F4XX_H_
#define __MCU_CLOCK_STM32F4XX_H_

#include <stdint.h>
#include <stm32_common/mcu.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return PLL VCO frequency
 *
 * @return PLL VCO frequency
 */
uint32_t stm32f4xx_pll_vco_freq(void);

/**
 * Return PLLI2S VCO frequency
 *
 * @return PLLI2S VCO frequency
 */
uint32_t stm32f4xx_plli2s_vco_freq(void);

/**
 * Return PLL P frequency (can be SYSCLK)
 *
 * @return PLL P frequency
 */
uint32_t stm32f4xx_pll_p_freq(void);

/**
 * Return PLL Q frequency (usually 48MHz)
 *
 * @return PLL Q frequency
 */
uint32_t stm32f4xx_pll_q_freq(void);

/**
 * Return PLL R frequency
 *
 * @return PLL R frequency
 */
uint32_t stm32f4xx_pll_r_freq(void);

/**
 * Return PLLI2S Q frequency
 *
 * @return PLLI2S Q frequency
 */
uint32_t stm32f4xx_plli2s_q_freq(void);

/**
 * Return PLLI2S R frequency
 *
 * @return PLLI2S R frequency
 */
uint32_t stm32f4xx_plli2s_r_freq(void);

#ifdef __cplusplus
}
#endif

#endif /* __MCU_CLOCK_STM32F4XX_H_ */
