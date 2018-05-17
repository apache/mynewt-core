/*----------------------------------------------------------------------------
 * Copyright (c) 1999-2009 KEIL, 2009-2013 ARM Germany GmbH
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  - Neither the name of ARM  nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *---------------------------------------------------------------------------*/
#include <syscfg/syscfg.h>
#include "os/os_trace_api.h"

        .file   "HAL_CM0.S"
        .syntax unified

/*----------------------------------------------------------------------------
 *      Functions
 *---------------------------------------------------------------------------*/

        .thumb

        .section ".text"
        .align  2

/*--------------------------- os_set_env ------------------------------------*/
#       void os_set_env (void);
#
#   Called to switch to privileged or unprivileged thread mode. The variable
#   'os_flags' contains the CONTROL bit LSB to set the device into the
#   proper mode. We also use PSP so we set bit 1 (the SPSEL bit) to 1.
#

        .thumb_func
        .type   os_set_env, %function
        .global os_set_env
os_set_env:
        .fnstart
        .cantunwind

        MSR     PSP,R0
        LDR     R0,=os_flags
        LDRB    R0,[R0]
        ADDS    R0, R0, #2
        MSR     CONTROL,R0
        ISB
        BX      LR

        .fnend
        .size   os_set_env, .-os_set_env
/*--------------------------- os_set_env ------------------------------------*/


/*--------------------------- os_arch_init_task_stack------------------------*/
#       void os_arch_init_task_stack(os_stack_t *sf);
# NOTE: This function only stores R4 through R11 on stack. The reason we do 
# this is that the application may have stored some values in some of the
# registers and we want to preserve those values (position independent code
# is a good example). The other registers are handled in the C startup code.
        .thumb_func
        .type   os_arch_init_task_stack, %function
        .global os_arch_init_task_stack
os_arch_init_task_stack:
        .fnstart

        MOV     R3,R0
        STMIA   R0!,{R4-R7}
        MOV     R4,R8
        MOV     R5,R9
        MOV     R6,R10
        MOV     R7,R11
        STMIA   R0!,{R4-R7}
        LDM     R3!,{R4-R7}
        BX      LR

        .fnend
        .size   os_arch_init_task_stack, .-os_arch_init_task_stack
/*--------------------------- os_set_env ------------------------------------*/


/*-------------------------- SVC_Handler ------------------------------------*/
#       void SVC_Handler (void);
        .thumb_func
        .type   SVC_Handler, %function
        .global SVC_Handler
SVC_Handler:
        .fnstart
        .cantunwind

        PUSH    {R4,LR}
#if MYNEWT_VAL(OS_SYSVIEW)
        BL      os_trace_isr_enter
#endif

        MRS     R0,PSP                  /* Read PSP */
        LDR     R1,[R0,#24]             /* Read Saved PC from Stack */
        SUBS    R1,R1,#2                /* Point to SVC Instruction */
        LDRB    R1,[R1]                 /* Load SVC Number */
        CMP     R1,#0
        BNE     SVC_User                /* User SVC Number > 0 */
        LDR     R1,[R0,#16]             /* Read saved R12 from Stack */
        MOV     R12, R1                 /* Store in R12 */
        LDMIA   R0,{R0-R3}              /* Read R0-R3 from stack */
        BLX     R12                     /* Call SVC Function */
        MRS     R3,PSP                  /* Read PSP */
        STMIA   R3!,{R0-R2}             /* Store return values */
#if MYNEWT_VAL(OS_SYSVIEW)
        BL      os_trace_isr_exit
#endif
        POP     {R4,PC}                 /* RETI */

        /*------------------- User SVC ------------------------------*/
SVC_User:
        LDR     R2,=SVC_Count
        LDR     R2,[R2]
        CMP     R1,R2
        BHI     SVC_Done                /* Overflow */
        LDR     R4,=SVC_Table-4
        LSLS    R1,R1,#2
        LDR     R4,[R4,R1]              /* Load SVC Function Address */
        MOV     LR,R4
        LDMIA   R0,{R0-R3,R4}           /* Read R0-R3,R12 from stack */
        MOV     R12,R4
        BLX     LR                      /* Call SVC Function */
        MRS     R4,PSP                  /* Read PSP */
        STMIA   R4!,{R0-R3}             /* Function return values */
SVC_Done:
#if MYNEWT_VAL(OS_SYSVIEW)
        BL      os_trace_isr_exit
#endif
        POP     {R4,PC}                 /* RETI */
        .fnend
        .size   SVC_Handler, .-SVC_Handler
/*-------------------------- PendSV_Handler ---------------------------------*/
#       void PendSV_Handler (void);
        .thumb_func
        .type   PendSV_Handler, %function
        .global PendSV_Handler
PendSV_Handler:
        .fnstart
        .cantunwind

        LDR     R3,=g_os_run_list   /* Get highest priority task ready to run */
        LDR     R2,[R3]             /* Store in R2 */
        LDR     R3,=g_current_task  /* Get current task */
        LDR     R1,[R3]             /* Current task in R1 */
        CMP     R1,R2
        BNE     context_switch
        BX      LR                  /* RETI, no task switch */
context_switch:
        MRS     R0,PSP              /* Read PSP */
        SUBS    R0,R0,#32
        STMIA   R0!,{R4-R7}         /* Save Old context */
        MOV     R4,R8
        MOV     R5,R9
        MOV     R6,R10
        MOV     R7,R11
        STMIA   R0!,{R4-R7}         /* Save Old context */
        SUBS    R0,R0,#32

        STR     R0,[R1,#0]          /* Update stack pointer in current task */
        STR     R2,[R3]             /* g_current_task = highest ready */

        LDR     R0,[R2,#0]          /* get stack pointer of task we will start */
        ADDS    R0,R0, #16
        LDMIA   R0!,{R4-R7}         /* Restore New Context */
        MOV     R8,R4
        MOV     R9,R5
        MOV     R10,R6
        MOV     R11,R7
        MSR     PSP,R0              /* Write PSP */
        SUBS    R0,R0,#32
        LDMIA   R0!,{R4-R7}         /* Restore New Context */

#if MYNEWT_VAL(OS_SYSVIEW)
        PUSH    {R4,LR}
        MOV     R0, R2
        BL      os_trace_task_start_exec
        POP     {R4,PC}
#else
        BX      LR                  /* Return to Thread Mode */
#endif

        .fnend
        .size   PendSV_Handler, .-PendSV_Handler



/*-------------------------- SysTick_Handler --------------------------------*/

#       void SysTick_Handler (void);
        .thumb_func
        .type   SysTick_Handler, %function
        .global SysTick_Handler
SysTick_Handler:
        .fnstart
        .cantunwind

        PUSH    {R4,LR}                 /* Save EXC_RETURN */
#if MYNEWT_VAL(OS_SYSVIEW)
        BL      os_trace_isr_enter
#endif
        BL      timer_handler
#if MYNEWT_VAL(OS_SYSVIEW)
        BL      os_trace_isr_exit
#endif
        POP     {R4,PC}                 /* Restore EXC_RETURN */

        .fnend
        .size   SysTick_Handler, .-SysTick_Handler

/* divide-by-zero */
        .thumb_func
	.type __aeabi_idiv0, %function
	.global __aeabi_idiv0
	.global __aeabi_ldiv0
__aeabi_idiv0:
__aeabi_ldiv0:
        .fnstart
        .cantunwind
	push {r0,r1,r5}
	ldr r0, =file_name
	ldr r1, =__LINE__
	ldr r5, =__assert_func
	bx r5
        .fnend
        .size   __aeabi_idiv0, .-__aeabi_idiv0

/*-------------------------- Defalt IRQ --------------------------------*/
        .thumb_func
        .type   os_default_irq_asm, %function
        .global os_default_irq_asm
os_default_irq_asm:
        .fnstart
        .cantunwind

#if MYNEWT_VAL(OS_SYSVIEW)
        PUSH    {R4,LR}
        BL      os_trace_isr_enter
#endif

        /*
         * LR = 0xfffffff9 if we were using MSP as SP
         * LR = 0xfffffffd if we were using PSP as SP
         */
        MOV     R0,LR
        MOVS    R1,#4
        TST     R0,R1
        MRS     R12,MSP
        BEQ     using_msp_as_sp
        MRS     R12,PSP
using_msp_as_sp:
        /*
         * Push the "trap frame" structure onto stack by moving R8-R11 into 
         * R0-R3 and then pushing R0-R3. LR gets pushed as well with LR 
         * residing at higher memory, R8 at the lower memory address.
         */
        MOV     R0,R8
        MOV     R1,R9
        MOV     R2,R10
        MOV     R3,R11
#if MYNEWT_VAL(OS_SYSVIEW)
        PUSH    {R0-R3}
#else
        PUSH    {R0-R3, LR}
#endif
        /* Now push r3 - r7. R3 is the lowest memory address. */
        MOV     R3,R12
        PUSH    {R3-R7}
        MOV     R0, SP
        BL      os_default_irq
        POP     {R3-R7}
        POP     {R0-R3}
        MOV     R8,R0
        MOV     R9,R1
        MOV     R10,R2
        MOV     R11,R3
#if MYNEWT_VAL(OS_SYSVIEW)
        BL      os_trace_isr_exit
        POP     {R4,PC}
#else
        POP     {PC}                 /* Restore EXC_RETURN */
#endif

        .fnend
        .size   os_default_irq_asm, .-os_default_irq_asm

        /*
         * Prevent libgcc unwind stuff from getting pulled in.
         */
        .section ".data"
        .global __aeabi_unwind_cpp_pr0
__aeabi_unwind_cpp_pr0:
	.section ".rodata"
file_name:
	.asciz __FILE__
        .end

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
