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
    .arch   armv7-m

    .section sleep_state, "aw"
    .align  3
    .globl  da1469x_m33_sleep_state
    .type   da1469x_m33_sleep_state, %object
da1469x_m33_sleep_state:
.saved_primask:
    .space 4    /* PRIMASK */
.saved_msp:
    .space 4    /* MSP */
.saved_psp:
    .space 4    /* PSP */
.saved_control:
    .space 4    /* CONTROL */
.saved_regs:
    .space 40   /* R4-R12, LR */
.saved_nvic:
    .space 8    /* ISER[0..1] */
    .space 40   /* IPR[0..39] */
.saved_scb:
    .space 28   /* SCR, CCR, SHPR[0..11], CPACR */
#if MYNEWT_VAL(HARDFLOAT)
.saved_fpu:
    .space 8    /* FPCCR, FPDSCR */
#endif

    .size   da1469x_m33_sleep_state, . - da1469x_m33_sleep_state

    .equ CLK_AMBA_REG,              0x50000000
    .equ RESET_STAT_REG,            0x500000BC
    .equ NVIC_BASE,                 0xE000E100
    .equ NVIC_IPR_OFFSET,           0x300
    .equ SCB_BASE,                  0xE000ED00
    .equ SCB_SCR_OFFSET,            0x010
    .equ SCB_SHCSR_OFFSET,          0x024
    .equ SCB_CPACR_OFFSET,          0x088
    .equ FPU_BASE,                  0xE000EF30
    .equ FPU_FPCCR_OFFSET,          0x004
    .equ FPU_FPDSCR_OFFSET,         0x00C
    .equ QSPIC_BASE,                0x38000000
    .equ QSPIC_CTRLBUS_OFFSET,      0x000
    .equ QSPIC_CTRLMOD_OFFSET,      0x004
    .equ QSPIC_WRITEDATA_OFFSET,    0x018

    .equ SCB_CPACR_MASK,            0x00F00000  /* CP10 and CP11 */
    .equ SCB_SHCSR_MASK,            0x000F0000  /* xxxFAULTENA */
    .equ FPU_FPCCR_MASK,            0xF0000000  /* ASPEN, LSPEN, LSPENS, CLRONRET */

    .section .text_ram
    .thumb
    .thumb_func
    .align  2
    .globl  da1469x_m33_sleep
    .type   da1469x_m33_sleep, %function
da1469x_m33_sleep:
    ldr     r3, =da1469x_m33_sleep_state

/* Disable interrupts and save original PRIMASK */
    mrs     r0, PRIMASK
    cpsid   i
    stmia   r3!, {r0}

/* Save MSP, PSP, CONTROL and general purpose registers */
    mrs     r0, MSP
    mrs     r1, PSP
    mrs     r2, CONTROL
    stmia   r3!, {r0-r2,r4-r12, lr}

/* Save NVIC state (ISER[0..1] and IPR[0..39]) */
    ldr     r0, =NVIC_BASE
    add     r1, r0, NVIC_IPR_OFFSET
    ldmia   r0!, {r4-r5}
    stmia   r3!, {r4-r5}
    ldmia   r1!, {r2, r4-r12}
    stmia   r3!, {r2, r4-r12}

/* Save SCB state (SCR, CCR, SHPR and SHCSR) */
    ldr     r0, =(SCB_BASE + SCB_SCR_OFFSET)
    ldmia   r0!, {r4-r9}
    and     r9, r9, #(SCB_SHCSR_MASK)
    ldr     r10, [r1, #(SCB_CPACR_OFFSET - SCB_SCR_OFFSET)]
    and     r10, r10, #(SCB_CPACR_MASK)
    stmia   r3!, {r4-r10}

#if MYNEWT_VAL(HARDFLOAT)
/* Save FPU state (FPCCR and FPDSCR) */
    ldr     r0, =FPU_BASE
    ldr     r4, [r0, #(FPU_FPCCR_OFFSET)]
    and     r4, r4, #(FPU_FPCCR_MASK)
    ldr     r5, [r0, #(FPU_FPDSCR_OFFSET)]
    stmia   r3!, {r4-r5}
#endif

/* Clear RESET_STAT_REG so wakeup handler can detect wakeup from deep sleep */
    ldr     r0, =RESET_STAT_REG
    movs    r3, #0
    str     r3, [r0, #0]

/* Disable QSPI controller clock */
    ldr     r0, =CLK_AMBA_REG
    ldr     r1, [r0, #0]
    bic     r1, r1, #0x1000
    str     r1, [r0, #0]

/* Set SCB->SCR[SLEEPDEEP] so we can enter deep sleep */
    ldr     r0, =(SCB_BASE + SCB_SCR_OFFSET)
    ldr     r1, [r0, #0]
    mov     r2, r1
    orr     r2, r1, #4      /* SLEEPDEEP */
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
    mov     r0, #0

/* Enable QSPI controller clock */
    ldr     r2, =CLK_AMBA_REG
    ldr     r1, [r2, #0]
    orr     r1, r1, #0x1000
    str     r1, [r2, #0]

    b       da1469x_m33_restore

    .section .text_ram
    .thumb
    .thumb_func
    .align  2
    .globl  da1469x_m33_wakeup
    .type   da1469x_m33_wakeup, %function
da1469x_m33_wakeup:
/* Disable interrupts, we'll restore proper PRIMASK at the end */
    cpsid   i

 /*
  * Temporarily restore saved MSP as temporary stack pointer to allow proper
  * stacking in case of an exception.
  */
    ldr     r3, =.saved_msp
    ldr     sp, [r3, #0]

 /* Restore NVIC state */
    ldr     r3, =.saved_nvic
    ldr     r0, =NVIC_BASE
    add     r1, r0, NVIC_IPR_OFFSET
    ldmia   r3!, {r4-r5}
    stmia   r0!, {r4-r5}
    ldmia   r3!, {r2, r4-r12}
    stmia   r1!, {r2, r4-r12}

 /* Restore SCB state */
    ldmia   r3!, {r4-r10}
    ldr     r0, =(SCB_BASE + SCB_SCR_OFFSET)
    ldr     r1, [r0, #(SCB_CPACR_OFFSET - SCB_SCR_OFFSET)]
    orr     r10, r10, r1
    str     r10, [r0, #(SCB_CPACR_OFFSET - SCB_SCR_OFFSET)]
    ldr     r1, [r0, #(SCB_SHCSR_OFFSET - SCB_SCR_OFFSET)]
    orr     r9, r9, r1
    stmia   r0!, {r4-r9}

#if MYNEWT_VAL(HARDFLOAT)
/* Restore FPU state */
    ldmia   r3!, {r4-r5}
    ldr     r0, =FPU_BASE
    ldr     r1, [r0, #(FPU_FPCCR_OFFSET)]
    orr     r4, r4, r1
    str     r4, [r0, #(FPU_FPCCR_OFFSET)]
    str     r5, [r0, #(FPU_FPDSCR_OFFSET)]
#endif

/* Restore MSP, PSP and CONTROL */
    ldr     r3, =.saved_msp
    ldmia   r3!, {r0-r2}
    msr     MSP, r0
    msr     PSP, r1
    msr     CONTROL, r2

/* Enable QSPI controller clock */
    ldr     r0, =CLK_AMBA_REG
    ldr     r1, [r0, #0]
    orr     r1, r1, #0x1000
    str     r1, [r0, #0]

/*
 * Flash is likely in QPI mode but QSPIC assumes it is in single mode so we
 * should make sure both are in sync by forcing exit from QPI mode and then
 * re-enter it. We need to do this before we start executing code from flash
 * again.
 */
    ldr     r0, =QSPIC_BASE
    ldr     r1, [r0, #(QSPIC_CTRLMOD_OFFSET)]
    bic     r1, r1, #1      /* Clear automode */
    orr     r1, r1, #0x3c   /* Set IO2/IO3 DAT and OEN */
    str     r1, [r0, #(QSPIC_CTRLMOD_OFFSET)]
    mov     r1, #1          /* Set single */
    str     r1, [r0, #(QSPIC_CTRLBUS_OFFSET)]
    mov     r1, #8          /* Enable CS */
    str     r1, [r0, #(QSPIC_CTRLBUS_OFFSET)]
    mov     r1, #0xff       /* Exit QPI mode */
    strb    r1, [r0, #(QSPIC_WRITEDATA_OFFSET)]
    mov     r1, #16         /* Disable CS */
    str     r1, [r0, #(QSPIC_CTRLBUS_OFFSET)]
    mov     r1, #4          /* Set quad */
    str     r1, [r0, #(QSPIC_CTRLBUS_OFFSET)]
    ldr     r1, [r0, #(QSPIC_CTRLMOD_OFFSET)]
    bic     r1, r1, #0xc    /* Clear IO2/IO3 OEN */
    orr     r1, r1, #1      /* Enable automode */
    str     r1, [r0, #(QSPIC_CTRLMOD_OFFSET)]

/* Finish restore, return true to caller to indicate we slept */
    mov     r0, #1
    ldr     r3, =da1469x_m33_restore
    bx      r3

    .section .text_ram
    .thumb
    .thumb_func
    .align  2
    .globl  da1469x_m33_restore
    .type   da1469x_m33_restore, %function
da1469x_m33_restore:
    ldr     r3, =.saved_regs
    ldmia   r3!, {r4-r12, lr}

    ldr     r3, =.saved_primask
    ldr     r3, [r3, #0]
    msr     PRIMASK, r3

    bx      lr
