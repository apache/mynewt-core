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

 #include "syscfg/syscfg.h"

    .syntax unified
    .arch   armv6-m

    .section .stack, "aw"
    .align  3
#ifdef __STACK_SIZE
    .equ    Stack_Size, __STACK_SIZE
#else
    .equ    Stack_Size, 0xC00
#endif
    .globl  __StackTop
    .globl  __StackLimit
__StackLimit:
    .space  Stack_Size
    .size   __StackLimit, . - __StackLimit
__StackTop:
    .size   __StackTop, . - __StackTop

    .section .heap, "aw"
    .align  3
#ifdef __HEAP_SIZE
    .equ    Heap_Size, __HEAP_SIZE
#else
    .equ    Heap_Size, 0x100
#endif
    .globl  __HeapBase
    .globl  __HeapLimit
__HeapBase:
    .if     Heap_Size
    .space  Heap_Size
    .endif
    .size   __HeapBase, . - __HeapBase
__HeapLimit:
    .size   __HeapLimit, . - __HeapLimit

    .section .isr_vector
    .align 2
    .globl  __isr_vector
__isr_vector:
    .long   __StackTop
    .long   Reset_Handler
    /* Cortex-M0+ interrupts */
    .long   NMI_Handler
    .long   HardFault_Handler
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   SVC_Handler
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   PendSV_Handler
    .long   0                       /* Reserved */
    /* CMAC interrupts */
    .long   FIELD_IRQHandler
    .long   CALLBACK_IRQHandler
    .long   FRAME_IRQHandler
    .long   DIAG_IRQHandler
    .long   HW_GEN_IRQHandler
    .long   SW_MAC_IRQHandler
    .long   LL_TIMER2PRMTV_IRQHandler
    .long   LL_TIMER2LLC_IRQHandler
    .long   CRYPTO_IRQHandler
    .long   SW_LLC_1_IRQHandler
    .long   SW_LLC_2_IRQHandler
    .long   SW_LLC_3_IRQHandler
    .long   SYS2CMAC_IRQHandler
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .size   __isr_vector, . - __isr_vector

    .equ    CM_CTRL_REG,                0x40000000
    .equ    MCPU_STATE_RETAINED,        8
    .equ    MCPU_SWD_EN,                4

    .equ    P0_SET_DATA_REG,            0x50020a08
    .equ    P0_RESET_DATA_REG,          0x50020a10
    .equ    P0_00_MODE_REG,             0x50020a18
    .equ    P0_SET_PAD_LATCH_REG,       0x50000074

    .text
    .thumb
    .thumb_func
    .align 2
    .globl Reset_Handler
    .type  Reset_Handler, %function
Reset_Handler:

#if MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP) >= 0
    ldr     r0, =P0_SET_PAD_LATCH_REG
    ldr     r1, =(1 << MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP))
    str     r1, [r0, #0]

    ldr     r0, =(P0_00_MODE_REG + MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP) * 4)
    ldr     r1, =0x0300
    str     r1, [r0, #0]

    ldr     r4, =(1 << MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP))
    ldr     r5, =P0_SET_DATA_REG
    ldr     r6, =P0_RESET_DATA_REG
    str     r4, [r5, #0]
    str     r4, [r6, #0]
    str     r4, [r5, #0]
#endif

    ldr     r0, =CM_CTRL_REG
    ldr     r1, [r0, #0]
#if MYNEWT_VAL(CMAC_DEBUG_SWD_ENABLE)
    movs    r2, MCPU_SWD_EN
    orrs    r1, r1, r2
    str     r1, [r0, #0]
#endif
    movs    r2, MCPU_STATE_RETAINED
    tst     r1, r2
    bne     cmac_sleep_do_wakeup

#if MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP) >= 0
    str     r4, [r6, #0]
    str     r4, [r5, #0]
#endif

/* Clear BSS */
    movs    r0, 0
    ldr     r1, =__bss_start__
    ldr     r2, =__bss_end__
.loop_bss_clear:
    cmp     r1, r2
    bge     .loop_bss_clear_done
    str     r0, [r1]
    adds    r1, r1, #4
    b       .loop_bss_clear
.loop_bss_clear_done:

    ldr     r0, =__HeapBase
    ldr     r1, =__HeapLimit
    bl      _sbrkInit

    bl      SystemInit
    bl      hal_system_init
    bl      _start

    .pool
    .size   Reset_Handler, . - Reset_Handler

/* Default handlers for some interrupts */
    .macro  IRQ handler
    .weak   \handler
    .set    \handler, os_default_irq_asm
    .endm

    /* Cortex-M0+ interrupts */
    IRQ  NMI_Handler
    IRQ  HardFault_Handler
    /* CMAC interrupts */
    IRQ  DIAG_IRQHandler
    IRQ  HW_GEN_IRQHandler
    IRQ  LL_TIMER2PRMTV_IRQHandler
    IRQ  SW_LLC_1_IRQHandler
    IRQ  SW_LLC_2_IRQHandler
    IRQ  SW_LLC_3_IRQHandler

.end
