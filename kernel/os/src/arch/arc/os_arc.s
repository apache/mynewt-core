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

#include "inc/arc/arc.h"
#include "inc/arc/arc_asm_common.h"

    .file "os_arc.s"

/*---------------------------------------------------------------
 * Normal interrupt entry code.
 *-------------------------------------------------------------*/
    .global exc_entry_int
    .align 4
exc_entry_int:
    /* disable interrupts */
    clri

#if ARC_FEATURE_FIRQ == 1
#if ARC_FEATURE_RGF_NUM_BANKS > 1
   /*  check whether it is P0 interrupt */
    lr      r0, [AUX_IRQ_ACT]
    btst    r0, 0
    bnz     exc_entry_firq
#else
    PUSH    r10
    lr      r10, [AUX_IRQ_ACT]
    btst    r10, 0
    POP     r10
    bnz     exc_entry_firq
#endif
#endif
    /* Pushes r12, gp, fp, ilink, r30 and reserves one location */
    INTERRUPT_PROLOGUE

    /* Temporarily save current stack pointer */
    mov     blink, sp

    /*
     * Increment nested exception count. If within a nested exception the
     * stack pointer is already set. If first exception, we use the exception
     * stack going forward. The exception stack is a firmware created
     * construct.
     */
    ld      r3, [exc_nest_count]
    add     r2, r3, 1
    st      r2, [exc_nest_count]
    cmp     r3, 0
    bne     irq_handler_1
    mov     sp, _e_stack

irq_handler_1:
    /* Save old stack pointer */
    PUSH    blink

    /* Get address of interrupt in r2 */
    lr      r0, [AUX_IRQ_CAUSE]
    sr      r0, [AUX_IRQ_SELECT]
    mov     r1, exc_int_handler_table
    ld.as   r2, [r1, r0]

    /* handle software triggered interrupt */
    lr      r3, [AUX_IRQ_HINT]
    cmp     r3, r0
    bne.d   irq_hint_handled
    xor     r3, r3, r3
    sr      r3, [AUX_IRQ_HINT]
irq_hint_handled:
    /*
     * Enable interrupts to allow higher priority interrupts to preempt.
     * Handle current interrupt
     */
    seti
    jl      [r2]

    /* Disable interrupts */
    clri

    /* Restore saved stack pointer */
    POP     blink
    mov     sp, blink

    /*
     * Decrement nested exception count. If nested, simply return.
     * If not, check for context switch
     */
    mov     r1, exc_nest_count
    ld      r0, [r1]
    sub     r0, r0, 1
    cmp     r0, 0
    bne.d   ret_int
    st      r0, [r1]

    /* Check if we should be running a different task */
    mov     r3, g_os_run_list
    ld      r2, [r3]
    mov     r3, g_current_task
    ld      r1, [r3]
    cmp     r1, r2
    beq     ret_int

    /* Save remaining registers on current task stack */
    SAVE_CALLEE_REGS

    /*
     * Current task is in r1 and task to run in r2. We need to:
     *  -> Save current stack pointer in cureren task.
     *  -> Set current task to new task to be run.
     *  -> Get stack pointer of new task.
     */
    st      sp, [r1]
    st      r2, [r3]
    ld      sp, [r2]

    /* Restore registers of task that are not done by hw */
    RESTORE_CALLEE_REGS

    /* Return from the interrupt */
ret_int:
    INTERRUPT_EPILOGUE
    rtie


/*---------------------------------------------------------------
 * Normal interrupt entry code.
 *-------------------------------------------------------------*/
    .global exc_entry_firq
    .align 4
exc_entry_firq:
    /* disable interrupt */
    clri

    SAVE_FIQ_EXC_REGS

    /* Increment nested exceptopn count */
    ld      r0, [exc_nest_count]
    add     r0, r0, 1
    st      r0, [exc_nest_count]

    /* r2 = _kernel_exc_tbl + irqno *4 */
    lr      r0, [AUX_IRQ_CAUSE]
    mov     r1, exc_int_handler_table
    ld.as   r2, [r1, r0]

    /* for the case of software triggered interrupt */
    lr      r3, [AUX_IRQ_HINT]
    cmp     r3, r0
    bne.d   firq_hint_handled
    xor     r3, r3, r3
    sr      r3, [AUX_IRQ_HINT]
firq_hint_handled:
    /* jump to interrupt handler */
    jl      [r2]

    /* no interrupts are allowed from here */
firq_return:

    /* exc_nest_count -1 */
    ld      r0, [exc_nest_count]
    sub     r0, r0, 1
    st      r0, [exc_nest_count]

    RESTORE_FIQ_EXC_REGS
    rtie

/*---------------------------------------------------------------
 * Exception entry code.
 *-------------------------------------------------------------*/
    .global exc_entry_cpu
    .align 4
exc_entry_cpu:

    EXCEPTION_PROLOGUE

    mov     blink, sp

    /*
     * Increment nested exception count. If within a nested exception the
     * stack pointer is already set. If first exception, we use the exception
     * stack going forward. The exception stack is a firmware created
     * construct.
     */
    ld      r0, [exc_nest_count]
    add     r1, r0, 1
    st      r1, [exc_nest_count]
    cmp     r0, 0
    bne     exc_handler_1

    mov     sp, _e_stack
exc_handler_1:
    /* Save old stack pointer */
    PUSH    blink

    /* Call exception handler */
    lr      r0, [AUX_ECR]
    lsr     r0, r0, 16
    mov     r1, exc_int_handler_table
    ld.as   r2, [r1, r0]
    jl      [r2]

ret_exc:
    POP     blink
    mov     sp, blink
    mov     r1, exc_nest_count
    ld      r0, [r1]
    sub     r0, r0, 1
    cmp     r0, 0
    bne.d   ret_exc_r_1
    st      r0, [r1]

    /* Check if we should be running a different task */
    mov     r3, g_os_run_list
    ld      r2, [r3]
    mov     r3, g_current_task
    ld      r1, [r3]
    cmp     r1, r2
    beq     ret_exc_r_1

    /* Save remaining registers on current task stack */
    SAVE_CALLEE_REGS

    /*
     * Current task is in r1 and task to run in r2. We need to:
     *  -> Save current stack pointer in current task.
     *  -> Set current task to new task to be run.
     *  -> Get stack pointer of new task.
     */
    st      sp, [r1]
    st      r2, [r3]
    ld      sp, [r2]

    /* Restore registers of task that are not done by hw */
    RESTORE_CALLEE_REGS

ret_exc_r_1:
    EXCEPTION_EPILOGUE
    rtie

