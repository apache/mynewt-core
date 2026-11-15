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

#ifndef __MCU_PIC32_ISR_H__
#define __MCU_PIC32_ISR_H__

#include <stddef.h>
#include <stdint.h>
#include <os/link_tables.h>
#include <syscfg/syscfg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Interrupt configuration, one element for each PIC32_ISR_NO_FPU/WITH_FPU() */
struct pic32_isr_cfg {
    void (*handler)(void);
    uint8_t vector;
    uint8_t ipl;
};

LINK_TABLE(struct pic32_isr_cfg, pic32_isr_table)

/**
 * Sets priority of all interrupts declared with PIC32_ISR_NO_FPU() and
 * PIC32_ISR_WITH_FPU(). Called from SystemInit().
 */
void pic32_isr_init(void);

#define PIC32_ISR_STR_(x) #x
#define PIC32_ISR_STR(x) PIC32_ISR_STR_(x)

#if MYNEWT_VAL(MCU_ISR_DISPATCHER)

/*
 * Vector entry: a0 = handler, a1 = IPL, jump to common dispatcher
 * (src/arch/pic32/pic32_isr_dispatch.S). Linker sets vector offset when
 * __vector_dispatch_<n> is defined.
 */
#define PIC32_ISR_STUB(_vector, _ipl, _fun, _disp)                               \
    __asm__(".pushsection .vector_" PIC32_ISR_STR(_vector) ",\"ax\",@progbits\n" \
            ".set push\n"                                                        \
            ".set nomips16\n"                                                    \
            ".set nomicromips\n"                                                 \
            ".set noreorder\n"                                                   \
            ".align 2\n"                                                         \
            ".globl __vector_dispatch_" PIC32_ISR_STR(_vector) "\n"              \
            ".type __vector_dispatch_" PIC32_ISR_STR(_vector) ", @function\n"    \
            "__vector_dispatch_" PIC32_ISR_STR(_vector) ":\n"                    \
            "    lui     $4, %hi(" #_fun ")\n"                                   \
            "    addiu   $4, $4, %lo(" #_fun ")\n"                               \
            "    j       " #_disp "\n"                                           \
            "    li      $5, " PIC32_ISR_STR(_ipl) "\n"                          \
            ".size __vector_dispatch_" PIC32_ISR_STR(_vector) ", "               \
            ".-__vector_dispatch_" PIC32_ISR_STR(_vector) "\n"                   \
            ".set pop\n"                                                         \
            ".popsection\n");

#define PIC32_ISR_VECTOR_NO_FPU(_vector, _ipl, _fun)                         \
    PIC32_ISR_STUB(_vector, _ipl, _fun, pic32_isr_no_fpu)

#if defined(__mips_hard_float)
#define PIC32_ISR_VECTOR_WITH_FPU(_vector, _ipl, _fun)                       \
    PIC32_ISR_STUB(_vector, _ipl, _fun, pic32_isr_with_fpu)
#else
/* Soft float, handler does not use FPU */
#define PIC32_ISR_VECTOR_WITH_FPU(_vector, _ipl, _fun)                       \
    PIC32_ISR_STUB(_vector, _ipl, _fun, pic32_isr_no_fpu)
#endif

#else

#define PIC32_ISR_ATTR_(_ipl) interrupt(IPL##_ipl##SRS)
#define PIC32_ISR_ATTR(_ipl) PIC32_ISR_ATTR_(_ipl)

/* Compiler generated interrupt function that calls the handler */
#define PIC32_ISR_VECTOR_NO_FPU(_vector, _ipl, _fun)                         \
    void __attribute__((PIC32_ISR_ATTR(_ipl), vector(_vector), no_fpu))      \
    pic32_vector_##_vector(void)                                             \
    {                                                                        \
        _fun();                                                              \
    }

/*
 * Compiler saves FPU registers in the prologue while FPU is still disabled,
 * it relies on Coprocessor Unusable exception handler (HARDFLOAT) to enable
 * FPU.
 */
#if defined(__mips_hard_float) && !MYNEWT_VAL(HARDFLOAT)
#define PIC32_ISR_FPU_OK 0
#else
#define PIC32_ISR_FPU_OK 1
#endif

#define PIC32_ISR_VECTOR_WITH_FPU(_vector, _ipl, _fun)                       \
    _Static_assert(PIC32_ISR_FPU_OK, "PIC32_ISR_WITH_FPU() requires "        \
                   "HARDFLOAT: 1 or MCU_ISR_DISPATCHER: 1");                 \
    void __attribute__((PIC32_ISR_ATTR(_ipl), vector(_vector)))              \
    pic32_vector_##_vector(void)                                             \
    {                                                                        \
        _fun();                                                              \
    }

#endif

#define PIC32_ISR_DECLARE(_vector, _ipl, _fun, _entry)                       \
    _Static_assert((_ipl) >= 2 && (_ipl) <= 7,                               \
                   "PIC32 interrupt priority level must be 2..7");           \
    _entry(_vector, _ipl, _fun)                                              \
    LINK_TABLE_ELEMENT(pic32_isr_table, _fun##_cfg) = {                      \
        .handler = _fun,                                                     \
        .vector = (_vector),                                                 \
        .ipl = (_ipl),                                                       \
    }

/**
 * Connects interrupt handler that does not use FPU to interrupt vector.
 *
 * Handler is a normal function (it can be static) defined before the macro.
 * The interrupt priority (IPCx register) is set from the same value by
 * pic32_isr_init(), so the code does not have to set it.
 * Handler runs with FPU disabled, it must not use floating point.
 *
 * Priority level must be 2..7, these levels use their own shadow register
 * set so registers of the interrupted code are not saved.
 * When MCU_ISR_DISPATCHER is enabled, interrupts that preempt a task run on
 * the main stack (_stack) instead of the task stack.
 *
 * @param _vector - interrupt vector number (i.e. _CHANGE_NOTICE_A_VECTOR)
 * @param _ipl    - interrupt priority level 2..7, number or MYNEWT_VAL()
 * @param _fun    - handler function void fun(void)
 */
#define PIC32_ISR_NO_FPU(_vector, _ipl, _fun)                                \
    PIC32_ISR_DECLARE(_vector, _ipl, _fun, PIC32_ISR_VECTOR_NO_FPU)

/**
 * Connects interrupt handler that may use floating point to interrupt vector.
 *
 * Same as PIC32_ISR_NO_FPU() but FPU is enabled for the handler and FPU
 * registers that it may change are saved on entry (216 bytes of stack more
 * in FR64 mode).
 * With MCU_ISR_DISPATCHER: 0 it requires HARDFLOAT: 1.
 *
 * @param _vector - interrupt vector number (i.e. _CHANGE_NOTICE_A_VECTOR)
 * @param _ipl    - interrupt priority level 2..7, number or MYNEWT_VAL()
 * @param _fun    - handler function void fun(void)
 */
#define PIC32_ISR_WITH_FPU(_vector, _ipl, _fun)                              \
    PIC32_ISR_DECLARE(_vector, _ipl, _fun, PIC32_ISR_VECTOR_WITH_FPU)

#ifdef __cplusplus
}
#endif

#endif /* __MCU_PIC32_ISR_H__ */
