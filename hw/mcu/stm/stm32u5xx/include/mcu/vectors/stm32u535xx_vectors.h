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
INT_VECTOR_SECUREFAULT_HANDLER(SecureFault_Handler)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR_SVC_HANDLER(SVC_Handler)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR_PENDSV_HANDLER(PendSV_Handler)
INT_VECTOR_SYSTICK_HANDLER(SysTick_Handler)
INT_VECTOR(WWDG_IRQHandler)
INT_VECTOR(PVD_PVM_IRQHandler)
INT_VECTOR(RTC_IRQHandler)
INT_VECTOR(RTC_S_IRQHandler)
INT_VECTOR(TAMP_IRQHandler)
INT_VECTOR(RAMCFG_IRQHandler)
INT_VECTOR(FLASH_IRQHandler)
INT_VECTOR(FLASH_S_IRQHandler)
INT_VECTOR(GTZC_IRQHandler)
INT_VECTOR(RCC_IRQHandler)
INT_VECTOR(RCC_S_IRQHandler)
INT_VECTOR(EXTI0_IRQHandler)
INT_VECTOR(EXTI1_IRQHandler)
INT_VECTOR(EXTI2_IRQHandler)
INT_VECTOR(EXTI3_IRQHandler)
INT_VECTOR(EXTI4_IRQHandler)
INT_VECTOR(EXTI5_IRQHandler)
INT_VECTOR(EXTI6_IRQHandler)
INT_VECTOR(EXTI7_IRQHandler)
INT_VECTOR(EXTI8_IRQHandler)
INT_VECTOR(EXTI9_IRQHandler)
INT_VECTOR(EXTI10_IRQHandler)
INT_VECTOR(EXTI11_IRQHandler)
INT_VECTOR(EXTI12_IRQHandler)
INT_VECTOR(EXTI13_IRQHandler)
INT_VECTOR(EXTI14_IRQHandler)
INT_VECTOR(EXTI15_IRQHandler)
INT_VECTOR(IWDG_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(GPDMA1_Channel0_IRQHandler)
INT_VECTOR(GPDMA1_Channel1_IRQHandler)
INT_VECTOR(GPDMA1_Channel2_IRQHandler)
INT_VECTOR(GPDMA1_Channel3_IRQHandler)
INT_VECTOR(GPDMA1_Channel4_IRQHandler)
INT_VECTOR(GPDMA1_Channel5_IRQHandler)
INT_VECTOR(GPDMA1_Channel6_IRQHandler)
INT_VECTOR(GPDMA1_Channel7_IRQHandler)
INT_VECTOR(ADC1_IRQHandler)
INT_VECTOR(DAC1_IRQHandler)
INT_VECTOR(FDCAN1_IT0_IRQHandler)
INT_VECTOR(FDCAN1_IT1_IRQHandler)
INT_VECTOR(TIM1_BRK_IRQHandler)
INT_VECTOR(TIM1_UP_IRQHandler)
INT_VECTOR(TIM1_TRG_COM_IRQHandler)
INT_VECTOR(TIM1_CC_IRQHandler)
INT_VECTOR(TIM2_IRQHandler)
INT_VECTOR(TIM3_IRQHandler)
INT_VECTOR(TIM4_IRQHandler)
INT_VECTOR(TIM5_IRQHandler)
INT_VECTOR(TIM6_IRQHandler)
INT_VECTOR(TIM7_IRQHandler)
INT_VECTOR(TIM8_BRK_IRQHandler)
INT_VECTOR(TIM8_UP_IRQHandler)
INT_VECTOR(TIM8_TRG_COM_IRQHandler)
INT_VECTOR(TIM8_CC_IRQHandler)
INT_VECTOR(I2C1_EV_IRQHandler)
INT_VECTOR(I2C1_ER_IRQHandler)
INT_VECTOR(I2C2_EV_IRQHandler)
INT_VECTOR(I2C2_ER_IRQHandler)
INT_VECTOR(SPI1_IRQHandler)
INT_VECTOR(SPI2_IRQHandler)
INT_VECTOR(USART1_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(USART3_IRQHandler)
INT_VECTOR(UART4_IRQHandler)
INT_VECTOR(UART5_IRQHandler)
INT_VECTOR(LPUART1_IRQHandler)
INT_VECTOR(LPTIM1_IRQHandler)
INT_VECTOR(LPTIM2_IRQHandler)
INT_VECTOR(TIM15_IRQHandler)
INT_VECTOR(TIM16_IRQHandler)
INT_VECTOR(TIM17_IRQHandler)
INT_VECTOR(COMP_IRQHandler)
INT_VECTOR(USB_IRQHandler)
INT_VECTOR(CRS_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(OCTOSPI1_IRQHandler)
INT_VECTOR(PWR_S3WU_IRQHandler)
INT_VECTOR(SDMMC1_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(GPDMA1_Channel8_IRQHandler)
INT_VECTOR(GPDMA1_Channel9_IRQHandler)
INT_VECTOR(GPDMA1_Channel10_IRQHandler)
INT_VECTOR(GPDMA1_Channel11_IRQHandler)
INT_VECTOR(GPDMA1_Channel12_IRQHandler)
INT_VECTOR(GPDMA1_Channel13_IRQHandler)
INT_VECTOR(GPDMA1_Channel14_IRQHandler)
INT_VECTOR(GPDMA1_Channel15_IRQHandler)
INT_VECTOR(I2C3_EV_IRQHandler)
INT_VECTOR(I2C3_ER_IRQHandler)
INT_VECTOR(SAI1_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(TSC_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(RNG_IRQHandler)
INT_VECTOR(FPU_IRQHandler)
INT_VECTOR(HASH_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(LPTIM3_IRQHandler)
INT_VECTOR(SPI3_IRQHandler)
INT_VECTOR(I2C4_ER_IRQHandler)
INT_VECTOR(I2C4_EV_IRQHandler)
INT_VECTOR(MDF1_FLT0_IRQHandler)
INT_VECTOR(MDF1_FLT1_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR(ICACHE_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR(LPTIM4_IRQHandler)
INT_VECTOR(DCACHE1_IRQHandler)
INT_VECTOR(ADF1_IRQHandler)
INT_VECTOR(ADC4_IRQHandler)
INT_VECTOR(LPDMA1_Channel0_IRQHandler)
INT_VECTOR(LPDMA1_Channel1_IRQHandler)
INT_VECTOR(LPDMA1_Channel2_IRQHandler)
INT_VECTOR(LPDMA1_Channel3_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR(DCMI_PSSI_IRQHandler)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR_UNUSED(0)
INT_VECTOR(CORDIC_IRQHandler)
INT_VECTOR(FMAC_IRQHandler)
INT_VECTOR(LSECSSD_IRQHandler)
