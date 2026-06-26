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

#ifndef _NRF54L_RTC_H_
#define _NRF54L_RTC_H_

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(NRF_RTC10)
#include "core_cm33.h"
/**
  * @brief Real-time counter
  */
typedef struct {                                     /*!< RTC Structure                                                        */
    __OM uint32_t TASKS_START;                       /*!< (@ 0x00000000) Start RTC counter                                     */
    __OM uint32_t TASKS_STOP;                        /*!< (@ 0x00000004) Stop RTC counter                                      */
    __OM uint32_t TASKS_CLEAR;                       /*!< (@ 0x00000008) Clear RTC counter                                     */
    __OM uint32_t TASKS_TRIGOVRFLW;                  /*!< (@ 0x0000000C) Set counter to 0xFFFFF0                               */
    __IM uint32_t RESERVED[12];
    __OM uint32_t TASKS_CAPTURE[4];                  /*!< (@ 0x00000040) Capture RTC counter to CC[n] register                 */
    __IM uint32_t RESERVED1[12];
    __IOM uint32_t SUBSCRIBE_START;                  /*!< (@ 0x00000080) Subscribe configuration for task START                */
    __IOM uint32_t SUBSCRIBE_STOP;                   /*!< (@ 0x00000084) Subscribe configuration for task STOP                 */
    __IOM uint32_t SUBSCRIBE_CLEAR;                  /*!< (@ 0x00000088) Subscribe configuration for task CLEAR                */
    __IOM uint32_t SUBSCRIBE_TRIGOVRFLW;             /*!< (@ 0x0000008C) Subscribe configuration for task TRIGOVRFLW           */
    __IM uint32_t RESERVED2[12];
    __IOM uint32_t SUBSCRIBE_CAPTURE[4];             /*!< (@ 0x000000C0) Subscribe configuration for task CAPTURE[n]           */
    __IM uint32_t RESERVED3[12];
    __IOM uint32_t EVENTS_TICK;                      /*!< (@ 0x00000100) Event on counter increment                            */
    __IOM uint32_t EVENTS_OVRFLW;                    /*!< (@ 0x00000104) Event on counter overflow                             */
    __IM uint32_t RESERVED4[14];
    __IOM uint32_t EVENTS_COMPARE[4];                /*!< (@ 0x00000140) Compare event on CC[n] match                          */
    __IM uint32_t RESERVED5[12];
    __IOM uint32_t PUBLISH_TICK;                     /*!< (@ 0x00000180) Publish configuration for event TICK                  */
    __IOM uint32_t PUBLISH_OVRFLW;                   /*!< (@ 0x00000184) Publish configuration for event OVRFLW                */
    __IM uint32_t RESERVED6[14];
    __IOM uint32_t PUBLISH_COMPARE[4];               /*!< (@ 0x000001C0) Publish configuration for event COMPARE[n]            */
    __IM uint32_t RESERVED7[12];
    __IOM uint32_t SHORTS;                           /*!< (@ 0x00000200) Shortcuts between local events and tasks              */
    __IM uint32_t RESERVED8[64];
    __IOM uint32_t INTENSET;                         /*!< (@ 0x00000304) Enable interrupt                                      */
    __IOM uint32_t INTENCLR;                         /*!< (@ 0x00000308) Disable interrupt                                     */
    __IM uint32_t RESERVED9[13];
    __IOM uint32_t EVTEN;                            /*!< (@ 0x00000340) Enable or disable event routing                       */
    __IOM uint32_t EVTENSET;                         /*!< (@ 0x00000344) Enable event routing                                  */
    __IOM uint32_t EVTENCLR;                         /*!< (@ 0x00000348) Disable event routing                                 */
    __IM uint32_t RESERVED10[110];
    __IM uint32_t COUNTER;                           /*!< (@ 0x00000504) Current counter value                                 */
    __IOM uint32_t PRESCALER;                        /*!< (@ 0x00000508) 12-bit prescaler for counter frequency (32768 /
                                                                         (PRESCALER + 1)). Must be written when RTC is stopped.*/
    __IM uint32_t RESERVED11[13];
    __IOM uint32_t CC[4];                            /*!< (@ 0x00000540) Compare register n                                    */
} NRF_RTC_Type;                                      /*!< Size = 1360 (0x550)                                                  */

#define NRF_RTC10_NS_BASE    0x40086000UL
#define NRF_RTC10_S_BASE     0x50086000UL
#define NRF_RTC10_NS         ((NRF_RTC_Type*)NRF_RTC10_NS_BASE)
#define NRF_RTC10_S          ((NRF_RTC_Type*)NRF_RTC10_S_BASE)
#define NRF_RTC10            NRF_RTC10_S

#define RTC10_IRQn (134)
#define RTC_INTENCLR_TICK_Pos (0UL)                /*!< Position of TICK field.                                              */
#define RTC_INTENCLR_TICK_Msk (0x1UL << RTC_INTENCLR_TICK_Pos) /*!< Bit mask of TICK field.                                  */
#define RTC_INTENSET_TICK_Pos (0UL)                /*!< Position of TICK field.                                              */
#define RTC_INTENSET_TICK_Msk (0x1UL << RTC_INTENSET_TICK_Pos) /*!< Bit mask of TICK field.                                  */
#define RTC_INTENSET_OVRFLW_Pos (1UL)              /*!< Position of OVRFLW field.                                            */
#define RTC_INTENSET_OVRFLW_Msk (0x1UL << RTC_INTENSET_OVRFLW_Pos) /*!< Bit mask of OVRFLW field.                            */
#define RTC_EVTENSET_COMPARE0_Pos (16UL)           /*!< Position of COMPARE0 field.                                          */
#define RTC_EVTENSET_COMPARE0_Msk (0x1UL << RTC_EVTENSET_COMPARE0_Pos) /*!< Bit mask of COMPARE0 field.                      */

static inline void nrf_rtc_event_enable(NRF_RTC_Type * p_reg, uint32_t mask)
{
    p_reg->EVTENSET = mask;
}

static inline void nrf_rtc_event_disable(NRF_RTC_Type * p_reg, uint32_t mask)
{
    p_reg->EVTENCLR = mask;
}

static inline void nrf_rtc_cc_set(NRF_RTC_Type * p_reg, uint32_t ch, uint32_t cc_val)
{
    p_reg->CC[ch] = cc_val;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* _NRF54L_RTC_H_ */
