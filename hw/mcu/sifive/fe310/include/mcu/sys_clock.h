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

#ifndef __MCU_SYS_CLOCK_H_
#define __MCU_SYS_CLOCK_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t frq;
  uint8_t xosc:1;
  uint8_t pll:1;
  uint8_t osc_div:6;
  uint8_t pll_div_r:2;
  uint8_t pll_mul_f:6;
  uint8_t pll_div_q:2;
  uint8_t pll_outdiv1:1;
  uint8_t pll_out_div:6;
} clock_config_t;

void select_clock(const clock_config_t *cfg);
uint32_t get_cpu_freq(void);
unsigned long get_timer_freq(void);
uint32_t mtime_lo(void);

extern const clock_config_t HFROSC;
extern const clock_config_t HFROSC_DIV_2;
extern const clock_config_t HFROSC_DIV_3;
extern const clock_config_t HFROSC_DIV_4;
extern const clock_config_t HFROSC_DIV_6;
extern const clock_config_t HFROSC_DIV_12;
extern const clock_config_t HFROSC_DIV_24;
extern const clock_config_t HFROSC_DIV_36;
extern const clock_config_t HFROSC_DIV_64;
extern const clock_config_t HFROSC_72_MHZ;
extern const clock_config_t HFXOSC_PLL_320_MHZ;
extern const clock_config_t HFXOSC_PLL_256_MHZ;
extern const clock_config_t HFXOSC_PLL_128_MHZ;
extern const clock_config_t HFXOSC_PLL_64_MHZ;
extern const clock_config_t HFXOSC_PLL_32_MHZ;
extern const clock_config_t HFXOSC_16_MHZ;
extern const clock_config_t HFXOSC_8_MHZ;
extern const clock_config_t HFXOSC_4_MHZ;
extern const clock_config_t HFXOSC_2_MHZ;
extern const clock_config_t HFXOSC_1_MHZ;

#ifdef __cplusplus
}
#endif

#endif /* __MCU_SYS_CLOCK_H_ */
