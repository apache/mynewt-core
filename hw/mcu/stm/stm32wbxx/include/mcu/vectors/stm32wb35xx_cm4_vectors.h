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

INT_VECTOR_STACK_TOP(__StackTop)
INT_VECTOR_RESET_HANDLER(Reset_Handler)
INT_VECTOR_NMI_HANDLER(NMI_Handler)
INT_VECTOR_HARDFAULT_HANDLER(HardFault_Handler)
INT_VECTOR_MEMMANAGE_HANDLER(MemManage_Handler)
INT_VECTOR_BUSFAULT_HANDLER(BusFault_Handler)
INT_VECTOR_USAGEFAULT_HANDLER(UsageFault_Handler)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR_SVC_HANDLER(SVC_Handler)
INT_VECTOR_DEBUGMON_HANDLER(DebugMon_Handler)
INT_VECTOR_UNUSED(0)
INT_VECTOR_PENDSV_HANDLER(PendSV_Handler)
INT_VECTOR_SYSTICK_HANDLER(SysTick_Handler)
INT_VECTOR(WWDG_IRQHandler)
INT_VECTOR(PVD_PVM_IRQHandler)
INT_VECTOR(TAMP_STAMP_LSECSS_IRQHandler)
INT_VECTOR(RTC_WKUP_IRQHandler)
INT_VECTOR(FLASH_IRQHandler)
INT_VECTOR(RCC_IRQHandler)
INT_VECTOR(EXTI0_IRQHandler)
INT_VECTOR(EXTI1_IRQHandler)
INT_VECTOR(EXTI2_IRQHandler)
INT_VECTOR(EXTI3_IRQHandler)
INT_VECTOR(EXTI4_IRQHandler)
INT_VECTOR(DMA1_Channel1_IRQHandler)
INT_VECTOR(DMA1_Channel2_IRQHandler)
INT_VECTOR(DMA1_Channel3_IRQHandler)
INT_VECTOR(DMA1_Channel4_IRQHandler)
INT_VECTOR(DMA1_Channel5_IRQHandler)
INT_VECTOR(DMA1_Channel6_IRQHandler)
INT_VECTOR(DMA1_Channel7_IRQHandler)
INT_VECTOR(ADC1_IRQHandler)
INT_VECTOR(USB_HP_IRQHandler)
INT_VECTOR(USB_LP_IRQHandler)
INT_VECTOR(C2SEV_PWR_C2H_IRQHandler)
INT_VECTOR(COMP_IRQHandler)
INT_VECTOR(EXTI9_5_IRQHandler)
INT_VECTOR(TIM1_BRK_IRQHandler)
INT_VECTOR(TIM1_UP_TIM16_IRQHandler)
INT_VECTOR(TIM1_TRG_COM_TIM17_IRQHandler)
INT_VECTOR(TIM1_CC_IRQHandler)
INT_VECTOR(TIM2_IRQHandler)
INT_VECTOR(PKA_IRQHandler)
INT_VECTOR(I2C1_EV_IRQHandler)
INT_VECTOR(I2C1_ER_IRQHandler)
INT_VECTOR(I2C3_EV_IRQHandler)
INT_VECTOR(I2C3_ER_IRQHandler)
INT_VECTOR(SPI1_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(USART1_IRQHandler)
INT_VECTOR(LPUART1_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR(EXTI15_10_IRQHandler)
INT_VECTOR(RTC_Alarm_IRQHandler)
INT_VECTOR(CRS_IRQHandler)
INT_VECTOR(PWR_SOTF_BLEACT_802ACT_RFPHASE_IRQHandler)
INT_VECTOR(IPCC_C1_RX_IRQHandler)
INT_VECTOR(IPCC_C1_TX_IRQHandler)
INT_VECTOR(HSEM_IRQHandler)
INT_VECTOR(LPTIM1_IRQHandler)
INT_VECTOR(LPTIM2_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(QUADSPI_IRQHandler)
INT_VECTOR(AES1_IRQHandler)
INT_VECTOR(AES2_IRQHandler)
INT_VECTOR(RNG_IRQHandler)
INT_VECTOR(FPU_IRQHandler)
INT_VECTOR(DMA2_Channel1_IRQHandler)
INT_VECTOR(DMA2_Channel2_IRQHandler)
INT_VECTOR(DMA2_Channel3_IRQHandler)
INT_VECTOR(DMA2_Channel4_IRQHandler)
INT_VECTOR(DMA2_Channel5_IRQHandler)
INT_VECTOR(DMA2_Channel6_IRQHandler)
INT_VECTOR(DMA2_Channel7_IRQHandler)
INT_VECTOR(DMAMUX1_OVR_IRQHandler)
