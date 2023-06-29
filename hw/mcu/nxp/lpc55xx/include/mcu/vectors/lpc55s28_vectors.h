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
INT_VECTOR(WDT_BOD_IRQHandler)
INT_VECTOR(DMA0_IRQHandler)
INT_VECTOR(GINT0_IRQHandler)
INT_VECTOR(GINT1_IRQHandler)
INT_VECTOR(PIN_INT0_IRQHandler)
INT_VECTOR(PIN_INT1_IRQHandler)
INT_VECTOR(PIN_INT2_IRQHandler)
INT_VECTOR(PIN_INT3_IRQHandler)
INT_VECTOR(UTICK0_IRQHandler)
INT_VECTOR(MRT0_IRQHandler)
INT_VECTOR(CTIMER0_IRQHandler)
INT_VECTOR(CTIMER1_IRQHandler)
INT_VECTOR(SCT0_IRQHandler)
INT_VECTOR(CTIMER3_IRQHandler)
INT_VECTOR(FLEXCOMM0_IRQHandler)
INT_VECTOR(FLEXCOMM1_IRQHandler)
INT_VECTOR(FLEXCOMM2_IRQHandler)
INT_VECTOR(FLEXCOMM3_IRQHandler)
INT_VECTOR(FLEXCOMM4_IRQHandler)
INT_VECTOR(FLEXCOMM5_IRQHandler)
INT_VECTOR(FLEXCOMM6_IRQHandler)
INT_VECTOR(FLEXCOMM7_IRQHandler)
INT_VECTOR(ADC0_IRQHandler)
INT_VECTOR(Reserved39_IRQHandler)
INT_VECTOR(ACMP_IRQHandler)
INT_VECTOR(Reserved41_IRQHandler)
INT_VECTOR(Reserved42_IRQHandler)
INT_VECTOR(USB0_NEEDCLK_IRQHandler)
INT_VECTOR(USB0_IRQHandler)
INT_VECTOR(RTC_IRQHandler)
INT_VECTOR(Reserved46_IRQHandler)
INT_VECTOR(Reserved47_IRQHandler)
INT_VECTOR(PIN_INT4_IRQHandler)
INT_VECTOR(PIN_INT5_IRQHandler)
INT_VECTOR(PIN_INT6_IRQHandler)
INT_VECTOR(PIN_INT7_IRQHandler)
INT_VECTOR(CTIMER2_IRQHandler)
INT_VECTOR(CTIMER4_IRQHandler)
INT_VECTOR(OS_EVENT_IRQHandler)
INT_VECTOR(Reserved55_IRQHandler)
INT_VECTOR(Reserved56_IRQHandler)
INT_VECTOR(Reserved57_IRQHandler)
INT_VECTOR(SDIO_IRQHandler)
INT_VECTOR(Reserved59_IRQHandler)
INT_VECTOR(Reserved60_IRQHandler)
INT_VECTOR(Reserved61_IRQHandler)
INT_VECTOR(USB1_PHY_IRQHandler)
INT_VECTOR(USB1_IRQHandler)
INT_VECTOR(USB1_NEEDCLK_IRQHandler)
INT_VECTOR(SEC_HYPERVISOR_CALL_IRQHandler)
INT_VECTOR(SEC_GPIO_INT0_IRQ0_IRQHandler)
INT_VECTOR(SEC_GPIO_INT0_IRQ1_IRQHandler)
INT_VECTOR(PLU_IRQHandler)
INT_VECTOR(SEC_VIO_IRQHandler)
INT_VECTOR(HASHCRYPT_IRQHandler)
INT_VECTOR(CASER_IRQHandler)
INT_VECTOR(PUF_IRQHandler)
INT_VECTOR(PQ_IRQHandler)
INT_VECTOR(DMA1_IRQHandler)
INT_VECTOR(FLEXCOMM8_IRQHandler)
