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

#include <xc.h>
#include <cp0defs.h>
#include <syscfg/syscfg.h>

#if MYNEWT_VAL(MCU_ISR_DISPATCHER)

#if __PIC32_SRS_SET_COUNT != 8
#error "MCU_ISR_DISPATCHER requires 8 shadow register sets"
#endif

/* Same condition as in crt0.S that enables DSP ASE */
#if defined(INIT_DSPR2) || defined(__PIC32_HAS_DSPR2)
#define ISR_SAVE_DSP    1
#else
#define ISR_SAVE_DSP    0
#endif

/* o32 argument area for the handler */
#define ISR_ARGS        16
#define ISR_LO          (ISR_ARGS + 0)
#define ISR_HI          (ISR_ARGS + 4)
#if ISR_SAVE_DSP
#define ISR_LO1         (ISR_ARGS + 8)
#define ISR_HI1         (ISR_ARGS + 12)
#define ISR_LO2         (ISR_ARGS + 16)
#define ISR_HI2         (ISR_ARGS + 20)
#define ISR_LO3         (ISR_ARGS + 24)
#define ISR_HI3         (ISR_ARGS + 28)
#define ISR_DSPCTL      (ISR_ARGS + 32)
#define ISR_FRAME       (ISR_ARGS + 40)
#else
#define ISR_FRAME       (ISR_ARGS + 8)
#endif

#if defined(__mips_hard_float)
#define ISR_HAS_FPU     1
/*
 * Only registers that handler may change (caller saved) are stored,
 * same condition as in crt0.S that enables FR64 mode.
 */
#if defined(INIT_FPU64) || defined(__PIC32_HAS_FPU64)
/* Status.FR = 1: $f0..$f19 and odd $f21..$f31, $f20..$f30 even are callee saved */
#define ISR_FP_REGS     26
#else
/* Status.FR = 0: pairs $f0..$f19, $f20..$f31 are callee saved */
#define ISR_FP_REGS     10
#endif
#define ISR_FP(n)       (ISR_FRAME + (n) * 8)
#define ISR_FCSR        ISR_FP(ISR_FP_REGS)
#define ISR_FPU_FRAME   (ISR_FCSR + 8)
#else
#define ISR_HAS_FPU     0
#endif

    .set    nomips16
    .set    nomicromips
    .set    noreorder
    .set    noat

/*
 * Common part of interrupts declared with PIC32_ISR_NO_FPU() and
 * PIC32_ISR_WITH_FPU().
 *
 * Entered from vector stub with:
 *   a0 - handler
 *   a1 - interrupt priority level (2..7)
 *
 * PRISS assigns separate shadow register set to each priority level 2..7,
 * so registers of the interrupted code don't need to be saved and same
 * level can't preempt itself. Only HI/LO, DSP accumulators and FPU are
 * shared. s0..s2 (preserved by the handler) keep EPC, Status and SRSCtl.
 */

/* Save CP0 state and select stack, allocates frame_size bytes on stack */
.macro isr_enter frame_size
    mfc0    s0, _CP0_EPC
    mfc0    s1, _CP0_STATUS
    mfc0    s2, _CP0_SRSCTL

    /*
     * Select stack:
     * - interrupted other interrupt (previous shadow set is not 0):
     *   continue on its stack
     * - interrupted code in set 0 before first task was started:
     *   continue on the current (main) stack
     * - interrupted task (or IPL1 interrupt running on task stack):
     *   switch to the main stack, it is not used once tasks are running,
     *   so tasks don't need to have room for interrupts.
     *   g_current_task is set in context switch with interrupts disabled,
     *   after that main stack is not used any more.
     */
    ext     t0, s2, _CP0_SRSCTL_PSS_POSITION, _CP0_SRSCTL_PSS_LENGTH
    bnez    t0, 1f
    rdpgpr  sp, sp
    lui     t0, %hi(g_current_task)
    lw      t0, %lo(g_current_task)(t0)
    beqz    t0, 1f
    nop
    lui     sp, %hi(_stack)
    addiu   sp, sp, %lo(_stack)
1:
    addiu   sp, sp, -\frame_size

    /* New Status: IPL = a1, EXL = ERL = UM = 0, FPU disabled */
    move    t1, s1
    ins     t1, a1, _CP0_STATUS_IPL_POSITION, _CP0_STATUS_IPL_LENGTH
    ins     t1, zero, _CP0_STATUS_EXL_POSITION, 4
    ins     t1, zero, _CP0_STATUS_CU1_POSITION, _CP0_STATUS_CU1_LENGTH
.endm

/* Save HI/LO and DSP state */
.macro isr_save_acc
    mflo    t0
    mfhi    t1
    sw      t0, ISR_LO(sp)
    sw      t1, ISR_HI(sp)
#if ISR_SAVE_DSP
    mflo    t0, $ac1
    mfhi    t1, $ac1
    sw      t0, ISR_LO1(sp)
    sw      t1, ISR_HI1(sp)
    mflo    t0, $ac2
    mfhi    t1, $ac2
    sw      t0, ISR_LO2(sp)
    sw      t1, ISR_HI2(sp)
    mflo    t0, $ac3
    mfhi    t1, $ac3
    sw      t0, ISR_LO3(sp)
    sw      t1, ISR_HI3(sp)
    rddsp   t0, 0x3f
    sw      t0, ISR_DSPCTL(sp)
#endif
.endm

/* Restore HI/LO and DSP state */
.macro isr_restore_acc
    lw      t0, ISR_LO(sp)
    lw      t1, ISR_HI(sp)
    mtlo    t0
    mthi    t1
#if ISR_SAVE_DSP
    lw      t0, ISR_LO1(sp)
    lw      t1, ISR_HI1(sp)
    mtlo    t0, $ac1
    mthi    t1, $ac1
    lw      t0, ISR_LO2(sp)
    lw      t1, ISR_HI2(sp)
    mtlo    t0, $ac2
    mthi    t1, $ac2
    lw      t0, ISR_LO3(sp)
    lw      t1, ISR_HI3(sp)
    mtlo    t0, $ac3
    mthi    t1, $ac3
    lw      t0, ISR_DSPCTL(sp)
    wrdsp   t0, 0x3f
#endif
.endm

/*
 * Return from interrupt.
 * sp of this shadow set does not need to be restored, it is selected
 * again on next entry.
 * Interrupts are disabled before CP0 registers are restored, Status gets
 * back EXL = 1 and CU1 of the interrupted code.
 */
.macro isr_exit
    di
    ehb
    mtc0    s0, _CP0_EPC
    mtc0    s2, _CP0_SRSCTL
    mtc0    s1, _CP0_STATUS
    eret
.endm

/* Handler that does not use FPU, FPU stays disabled */
    .section .text.pic32_isr_no_fpu,"ax",@progbits
    .align  2
    .globl  pic32_isr_no_fpu
    .type   pic32_isr_no_fpu, @function
    .ent    pic32_isr_no_fpu
pic32_isr_no_fpu:
    isr_enter ISR_FRAME
    /* Allow interrupts with higher priority */
    mtc0    t1, _CP0_STATUS
    isr_save_acc

    jalr    a0
    nop

    isr_restore_acc
    isr_exit
    .end    pic32_isr_no_fpu
    .size   pic32_isr_no_fpu, .-pic32_isr_no_fpu

#if ISR_HAS_FPU
/*
 * Handler that may use FPU.
 * FPU is enabled and registers that handler may change are saved on
 * interrupt stack. FPU registers may belong to any task (lazy FPU context
 * switch) or to other interrupt, they are saved regardless of the owner,
 * so no Coprocessor Unusable exception is needed.
 */
    .section .text.pic32_isr_with_fpu,"ax",@progbits
    .align  2
    .globl  pic32_isr_with_fpu
    .type   pic32_isr_with_fpu, @function
    .ent    pic32_isr_with_fpu
pic32_isr_with_fpu:
    isr_enter ISR_FPU_FRAME
    /* Enable FPU, allow interrupts with higher priority */
    li      t2, _CP0_STATUS_CU1_MASK
    or      t1, t1, t2
    mtc0    t1, _CP0_STATUS
    isr_save_acc
    /* Status.CU1 must be set before FPU is accessed */
    ehb

    sdc1    $f0, ISR_FP(0)(sp)
    sdc1    $f2, ISR_FP(1)(sp)
    sdc1    $f4, ISR_FP(2)(sp)
    sdc1    $f6, ISR_FP(3)(sp)
    sdc1    $f8, ISR_FP(4)(sp)
    sdc1    $f10, ISR_FP(5)(sp)
    sdc1    $f12, ISR_FP(6)(sp)
    sdc1    $f14, ISR_FP(7)(sp)
    sdc1    $f16, ISR_FP(8)(sp)
    sdc1    $f18, ISR_FP(9)(sp)
#if ISR_FP_REGS == 26
    sdc1    $f1, ISR_FP(10)(sp)
    sdc1    $f3, ISR_FP(11)(sp)
    sdc1    $f5, ISR_FP(12)(sp)
    sdc1    $f7, ISR_FP(13)(sp)
    sdc1    $f9, ISR_FP(14)(sp)
    sdc1    $f11, ISR_FP(15)(sp)
    sdc1    $f13, ISR_FP(16)(sp)
    sdc1    $f15, ISR_FP(17)(sp)
    sdc1    $f17, ISR_FP(18)(sp)
    sdc1    $f19, ISR_FP(19)(sp)
    sdc1    $f21, ISR_FP(20)(sp)
    sdc1    $f23, ISR_FP(21)(sp)
    sdc1    $f25, ISR_FP(22)(sp)
    sdc1    $f27, ISR_FP(23)(sp)
    sdc1    $f29, ISR_FP(24)(sp)
    sdc1    $f31, ISR_FP(25)(sp)
#endif
    cfc1    t0, $31
    sw      t0, ISR_FCSR(sp)

    jalr    a0
    nop

    lw      t0, ISR_FCSR(sp)
    ctc1    t0, $31
    ldc1    $f0, ISR_FP(0)(sp)
    ldc1    $f2, ISR_FP(1)(sp)
    ldc1    $f4, ISR_FP(2)(sp)
    ldc1    $f6, ISR_FP(3)(sp)
    ldc1    $f8, ISR_FP(4)(sp)
    ldc1    $f10, ISR_FP(5)(sp)
    ldc1    $f12, ISR_FP(6)(sp)
    ldc1    $f14, ISR_FP(7)(sp)
    ldc1    $f16, ISR_FP(8)(sp)
    ldc1    $f18, ISR_FP(9)(sp)
#if ISR_FP_REGS == 26
    ldc1    $f1, ISR_FP(10)(sp)
    ldc1    $f3, ISR_FP(11)(sp)
    ldc1    $f5, ISR_FP(12)(sp)
    ldc1    $f7, ISR_FP(13)(sp)
    ldc1    $f9, ISR_FP(14)(sp)
    ldc1    $f11, ISR_FP(15)(sp)
    ldc1    $f13, ISR_FP(16)(sp)
    ldc1    $f15, ISR_FP(17)(sp)
    ldc1    $f17, ISR_FP(18)(sp)
    ldc1    $f19, ISR_FP(19)(sp)
    ldc1    $f21, ISR_FP(20)(sp)
    ldc1    $f23, ISR_FP(21)(sp)
    ldc1    $f25, ISR_FP(22)(sp)
    ldc1    $f27, ISR_FP(23)(sp)
    ldc1    $f29, ISR_FP(24)(sp)
    ldc1    $f31, ISR_FP(25)(sp)
#endif

    isr_restore_acc
    isr_exit
    .end    pic32_isr_with_fpu
    .size   pic32_isr_with_fpu, .-pic32_isr_with_fpu
#endif

#endif
