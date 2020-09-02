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

    .section .bss
    .align  3
    .globl  cmac_sleep_state
    .type   cmac_sleep_state, %object
cmac_sleep_state:
.saved_primask:
    .space 4    /* PRIMASK */
.saved_msp:
    .space 4    /* MSP */
.saved_psp:
    .space 4    /* PSP */
.saved_control:
    .space 4    /* CONTROL */
.saved_regs1:
    .space 16   /* R4-R7 */
.saved_regs2:
    .space 24   /* R8-R12, LR */
.saved_nvic:
    .space 4    /* ISER */
    .space 16   /* IPR[0..31] */
.saved_scb:
    .space 12   /* SCR, SHPR2, SHPR3 */

    .size   cmac_sleep_state, . - cmac_sleep_state

    .equ CM_CTRL_REG,               0x40000000
    .equ MCPU_STATE_RETAINED,       0x08
    .equ NVIC_BASE,                 0xE000E100
    .equ NVIC_IPR_OFFSET,           0x300
    .equ SCB_BASE,                  0xE000ED00
    .equ SCB_SCR_OFFSET,            0x010
    .equ SCB_SHPR2_OFFSET,          0x01C
    .equ SCB_SHPR3_OFFSET,          0x020

    .section .text
    .thumb
    .thumb_func
    .align  2
    .globl  cmac_sleep_do_sleep
    .type   cmac_sleep_do_sleep, %function
cmac_sleep_do_sleep:
    ldr     r3, =cmac_sleep_state

/* Disable interrupts and save original PRIMASK */
    mrs     r0, PRIMASK
    cpsid   i
    stmia   r3!, {r0}

/* Save MSP, PSP, CONTROL and general purpose registers */
    mrs     r0, MSP
    mrs     r1, PSP
    mrs     r2, CONTROL
    stmia   r3!, {r0-r2,r4-r7}
    mov     r1, r8
    mov     r2, r9
    mov     r4, r10
    mov     r5, r11
    mov     r6, r12
    mov     r7, lr
    stmia   r3!, {r1-r2,r4-r7}

/* Save NVIC state (ISER and IPR[0..15]) */
    ldr     r0, =NVIC_BASE
    ldr     r4, [r0]
    stmia   r3!, {r4}
    ldr     r0, =(NVIC_BASE + NVIC_IPR_OFFSET)
    ldmia   r0!, {r4-r7}
    stmia   r3!, {r4-r7}

/* Save SCB state (SCR, SHPR2 and SHPR3) */
    ldr     r0, =SCB_BASE
    ldr     r4, [r0, SCB_SCR_OFFSET]
    ldr     r5, [r0, SCB_SHPR2_OFFSET]
    ldr     r6, [r0, SCB_SHPR3_OFFSET]
    stmia   r3!, {r4-r6}

/* Set CM_CTRL_REG[MCPU_STATE_RETAINED] */
    ldr     r1, =CM_CTRL_REG
    ldr     r2, [r1, #0]
    movs    r3, MCPU_STATE_RETAINED
    orrs    r2, r2, r3
    str     r2, [r1, #0]

#if MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP) >= 0
    ldr     r0, =0x50020a10
    ldr     r1, =(1 << MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP))
    str     r1, [r0, #0]
#endif

/* Set SCB->SCR[SLEEPDEEP] so we can enter deep sleep */
    ldr     r0, =(SCB_BASE + SCB_SCR_OFFSET)
    ldr     r1, [r0, #0]
    mov     r2, r1
    movs    r3, #4          /* SLEEPDEEP */
    orrs    r2, r2, r3
    str     r2, [r0, #0]

/* Sleep! */
    dsb
    wfi

/*
 * If deep sleep was executed we'll restart in reset handler, otherwise we just
 * restore registers and continue by returning false to caller to indicate we
 * did not sleep.
 */
    str     r1, [r0, #0]
#if MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP) >= 0
    ldr     r0, =0x50020a08
    ldr     r1, =(1 << MYNEWT_VAL(MCU_DEBUG_GPIO_DEEP_SLEEP))
    str     r1, [r0, #0]
#endif
    movs    r0, #0

    b       cmac_sleep_do_restore

    .section .text
    .thumb
    .thumb_func
    .align  2
    .globl  cmac_sleep_do_wakeup
    .type   cmac_sleep_do_wakeup, %function
cmac_sleep_do_wakeup:
/* Disable interrupts, we'll restore proper PRIMASK at the end */
    cpsid   i

 /*
  * Temporarily restore saved MSP as temporary stack pointer to allow proper
  * stacking in case of an exception.
  */
    ldr     r3, =.saved_msp
    ldr     r3, [r3, #0]
    mov     sp, r3

 /* Restore NVIC state */
    ldr     r3, =.saved_nvic
    ldr     r0, =NVIC_BASE
    ldmia   r3!, {r4}
    str     r4, [r0, #0]
    ldr     r0, =(NVIC_BASE + NVIC_IPR_OFFSET)
    ldmia   r3!, {r4-r7}
    stmia   r0!, {r4-r7}

 /* Restore SCB state */
    ldmia   r3!, {r4-r6}
    ldr     r0, =SCB_BASE
    str     r4, [r0, SCB_SCR_OFFSET]
    str     r5, [r0, SCB_SHPR2_OFFSET]
    str     r6, [r0, SCB_SHPR3_OFFSET]

/* Restore MSP, PSP and CONTROL */
    ldr     r3, =.saved_msp
    ldmia   r3!, {r0-r2}
    msr     MSP, r0
    msr     PSP, r1
    msr     CONTROL, r2

/* Finish restore, return true to caller to indicate we slept */
    movs    r0, #1
    ldr     r3, =cmac_sleep_do_restore
    bx      r3

    .section .text
    .thumb
    .thumb_func
    .align  2
    .globl  cmac_sleep_do_restore
    .type   cmac_sleep_do_restore, %function
cmac_sleep_do_restore:
    ldr     r3, =.saved_regs2
    ldmia   r3!, {r1-r2,r4-r7}
    mov     r8, r1
    mov     r9, r2
    mov     r10, r4
    mov     r11, r5
    mov     r12, r6
    mov     lr, r7
    ldr     r3, =.saved_regs1
    ldmia   r3!, {r4-r7}

    ldr     r3, =.saved_primask
    ldr     r3, [r3, #0]
    msr     PRIMASK, r3

    bx      lr
