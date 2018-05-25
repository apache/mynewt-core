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

#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

#include "syscfg/syscfg.h"

#define NRFX_CLOCK_ENABLED 0
#define NRFX_GPIOTE_ENABLED 0
#define NRFX_LPCOMP_ENABLED 0
#define NRFX_POWER_ENABLED 0
#define NRFX_PPI_ENABLED 0
#define NRFX_PRS_ENABLED 0
#define NRFX_QDEC_ENABLED 0
#define NRFX_RNG_ENABLED 0
#define NRFX_RTC_ENABLED 0
#define NRFX_RTC0_ENABLED 0
#define NRFX_RTC1_ENABLED 0
#define NRFX_SPIS_ENABLED 0
#define NRFX_SPIS1_ENABLED 0
#define NRFX_SPI_ENABLED 0
#define NRFX_SPI0_ENABLED 0
#define NRFX_SPI1_ENABLED 0
#define NRFX_SWI_ENABLED 0
#define NRFX_SWI0_DISABLED 0
#define NRFX_SWI1_DISABLED 0
#define NRFX_SWI2_DISABLED 0
#define NRFX_SWI3_DISABLED 0
#define NRFX_SWI4_DISABLED 0
#define NRFX_SWI5_DISABLED 0
#define NRFX_TIMER_ENABLED 0
#define NRFX_TIMER0_ENABLED 0
#define NRFX_TIMER1_ENABLED 0
#define NRFX_TIMER2_ENABLED 0
#define NRFX_TWI_ENABLED 0
#define NRFX_TWI0_ENABLED 0
#define NRFX_TWI1_ENABLED 0
#define NRFX_UART_ENABLED 0
#define NRFX_UART0_ENABLED 0
#define NRFX_WDT_ENABLED 0

#if MYNEWT_VAL(ADC_0)
#define NRFX_ADC_ENABLED 1
#define NRFX_ADC_CONFIG_IRQ_PRIORITY 3
#define NRFX_ADC_CONFIG_LOG_ENABLED 0
#define NRFX_ADC_CONFIG_LOG_LEVEL 3
#define NRFX_ADC_CONFIG_INFO_COLOR 0
#define NRFX_ADC_CONFIG_DEBUG_COLOR 0
#endif

#endif // NRFX_CONFIG_H__
