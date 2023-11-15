/**
 *
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

    .syntax unified
    .cpu cortex-m0
    .fpu softvfp
    .thumb

    .global g_pfnVectors
    .global Default_Handler
    .globl  Reset_Handler

    .section .text.Reset_Handler
    .type   Reset_Handler, %function
    .func   Reset_Handler

Reset_Handler:
    movs    r0, #0
    msr     primask, r0

    ldr     r1, =_estack
    mov     sp, r1          /* set stack pointer */

    /* Clear .bss section */
    ldr     r2, =__bss_start__
    ldr     r3, =__bss_end__
.L_bss_zero_loop:
    cmp     r2, r3
    bhs     .L_bss_zero_loop_end
    stmia   r2!, {r0}
    b       .L_bss_zero_loop
.L_bss_zero_loop_end:

    /* Copy .data section to RAM */
    ldr    r1, =_sidata
    ldr    r2, =__data_start__
    ldr    r3, =__data_end__

.L_data_copy_loop:
    cmp     r2, r3
    bhs     .L_data_copy_loop_end
    ldmia   r1!, {r0}
    stmia   r2!, {r0}
    b       .L_data_copy_loop
.L_data_copy_loop_end:

    ldr     r0, =__HeapBase
    ldr     r1, =__HeapLimit
    bl      _sbrkInit

    /* Call the clock system initialization function.*/
    bl      SystemInit

    bl      hal_system_init

    /* Call the application's entry point.*/
    bl      _start

    .size Reset_Handler, .-Reset_Handler
    .endfunc

/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.  This simply enters an infinite loop, preserving
 *         the system state for examination by a debugger.
 *
 * @param  None
 * @retval : None
*/
    .section .text.Default_Handler,"ax",%progbits
    .globl  Default_Handler
    .type   Default_Handler, %function
Default_Handler:
    b       .
    .size   Default_Handler, . - Default_Handler

    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .globl __isr_vector
__isr_vector:

    .word __StackTop
    .word  Reset_Handler
    .word  NMI_Handler
    .word  HardFault_Handler
    .word  0
    .word  0
    .word  0
    .word  0
    .word  0
    .word  0
    .word  0
    .word  SVC_Handler
    .word  0
    .word  0
    .word  PendSV_Handler
    .word  SysTick_Handler

    /* External Interrupts */
    .word  PM_Handler
    .word  SYSCTRL_Handler
    .word  WDT_Handler
    .word  RTC_Handler
    .word  EIC_Handler
    .word  NVMCTRL_Handler
    .word  DMAC_Handler
    .word  USB_Handler
    .word  EVSYS_Handler
    .word  SERCOM0_Handler
    .word  SERCOM1_Handler
    .word  SERCOM2_Handler
    .word  SERCOM3_Handler
    .word  SERCOM4_Handler
    .word  SERCOM5_Handler
    .word  TCC0_Handler
    .word  TCC1_Handler
    .word  TCC2_Handler
    .word  TC3_Handler
    .word  TC4_Handler
    .word  TC5_Handler
    .word  TC6_Handler
    .word  TC7_Handler
    .word  ADC_Handler
    .word  AC_Handler
    .word  DAC_Handler
    .word  PTC_Handler
    .word  I2S_Handler
    .word  AC1_Handler
    .word  TCC3_Handler

    .size    __isr_vector, . - __isr_vector


    .weak       NMI_Handler
    .thumb_set  NMI_Handler, Default_Handler

    .weak       HardFault_Handler
    .thumb_set  HardFault_Handler, Default_Handler

    .weak       SVC_Handler
    .thumb_set  SVC_Handler, Default_Handler

    .weak       PendSV_Handler
    .thumb_set  PendSV_Handler, Default_Handler

    .weak       SysTick_Handler
    .thumb_set  SysTick_Handler, Default_Handler

    .weak       PM_Handler,
    .thumb_set  PM_Handler, Default_Handler

    .weak       SYSCTRL_Handler,
    .thumb_set  SYSCTRL_Handler, Default_Handler

    .weak       WDT_Handler,
    .thumb_set  WDT_Handler, Default_Handler

    .weak       RTC_Handler,
    .thumb_set  RTC_Handler, Default_Handler

    .weak       EIC_Handler,
    .thumb_set  EIC_Handler, Default_Handler

    .weak       NVMCTRL_Handler,
    .thumb_set  NVMCTRL_Handler, Default_Handler

    .weak       DMAC_Handler,
    .thumb_set  DMAC_Handler, Default_Handler

    .weak       USB_Handler,
    .thumb_set  USB_Handler, Default_Handler

    .weak       EVSYS_Handler,
    .thumb_set  EVSYS_Handler, Default_Handler

    .weak       SERCOM0_Handler,
    .thumb_set  SERCOM0_Handler, Default_Handler

    .weak       SERCOM1_Handler,
    .thumb_set  SERCOM1_Handler, Default_Handler

    .weak       SERCOM2_Handler,
    .thumb_set  SERCOM2_Handler, Default_Handler

    .weak       SERCOM3_Handler,
    .thumb_set  SERCOM3_Handler, Default_Handler

    .weak       SERCOM4_Handler,
    .thumb_set  SERCOM4_Handler, Default_Handler

    .weak       SERCOM5_Handler,
    .thumb_set  SERCOM5_Handler, Default_Handler

    .weak       TCC0_Handler,
    .thumb_set  TCC0_Handler, Default_Handler

    .weak       TCC1_Handler,
    .thumb_set  TCC1_Handler, Default_Handler

    .weak       TCC2_Handler,
    .thumb_set  TCC2_Handler, Default_Handler

    .weak       TC3_Handler,
    .thumb_set  TC3_Handler, Default_Handler

    .weak       TC4_Handler,
    .thumb_set  TC4_Handler, Default_Handler

    .weak       TC5_Handler,
    .thumb_set  TC5_Handler, Default_Handler

    .weak       TC6_Handler,
    .thumb_set  TC6_Handler, Default_Handler

    .weak       TC7_Handler,
    .thumb_set  TC7_Handler, Default_Handler

    .weak       ADC_Handler,
    .thumb_set  ADC_Handler, Default_Handler

    .weak       AC_Handler,
    .thumb_set  AC_Handler, Default_Handler

    .weak       DAC_Handler,
    .thumb_set  DAC_Handler, Default_Handler

    .weak       PTC_Handler,
    .thumb_set  PTC_Handler, Default_Handler

    .weak       I2S_Handler,
    .thumb_set  I2S_Handler, Default_Handler

    .weak       AC1_Handler,
    .thumb_set  AC1_Handler, Default_Handler

    .weak       TCC3_Handler,
    .thumb_set  TCC3_Handler, Default_Handler

