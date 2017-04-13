/**
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

#include "mcu/mcu.h"
#include "os/os_arch_defs.h"
#include "syscfg/syscfg.h"

#define OS_STACK_ALIGNMENT  (8)

#define CTX_ALIGNED_SIZE ((((CTX_SIZE - 1) / OS_STACK_ALIGNMENT) + 1) * \
    OS_STACK_ALIGNMENT)
#define CTX_OFFS(r) (((r) * 4) - CTX_ALIGNED_SIZE)

#if (__mips_isa_rev < 6)
    #define CTX_SIZE (36 * 4)
    #define CTX_LO CTX_OFFS(34)
    #define CTX_HI CTX_OFFS(35)
#else
    #define CTX_SIZE (34 * 4)
#endif

# relative to the stack pointer
#define CTX_REG(r) CTX_OFFS((r) - 1)
#define CTX_EPC CTX_OFFS(30)
#define CTX_BADVADDR CTX_OFFS(31)
#define CTX_STATUS CTX_OFFS(32)
#define CTX_CAUSE CTX_OFFS(33)

.macro _gpctx_save
    .set push
    .set noat
    sw      $1, CTX_REG(1)(sp)
    .set at
    sw      v0, CTX_REG(2)(sp)
    sw      v1, CTX_REG(3)(sp)
    sw      a0, CTX_REG(4)(sp)
    sw      a1, CTX_REG(5)(sp)
    sw      a2, CTX_REG(6)(sp)
    sw      a3, CTX_REG(7)(sp)
    sw      t0, CTX_REG(8)(sp)
    sw      t1, CTX_REG(9)(sp)
    sw      t2, CTX_REG(10)(sp)
    sw      t3, CTX_REG(11)(sp)
    sw      t4, CTX_REG(12)(sp)
    sw      t5, CTX_REG(13)(sp)
    sw      t6, CTX_REG(14)(sp)
    sw      t7, CTX_REG(15)(sp)
    sw      s0, CTX_REG(16)(sp)
    sw      s1, CTX_REG(17)(sp)
    sw      s2, CTX_REG(18)(sp)
    sw      s3, CTX_REG(19)(sp)
    sw      s4, CTX_REG(20)(sp)
    sw      s5, CTX_REG(21)(sp)
    sw      s6, CTX_REG(22)(sp)
    sw      s7, CTX_REG(23)(sp)
    sw      t8, CTX_REG(24)(sp)
    sw      t9, CTX_REG(25)(sp)
    sw      k0, CTX_REG(26)(sp)
    sw      k1, CTX_REG(27)(sp)
    sw      gp, CTX_REG(28)(sp)
    # don't bother saving sp
    sw      fp, CTX_REG(29)(sp)
    sw      ra, CTX_REG(30)(sp)

#if (__mips_isa_rev < 6)
	mfhi   k0
    sw	   k0, CTX_HI(sp)
	mflo   k0
	sw     k0, CTX_LO(sp)
#endif

    # cp0
    mfc0    k0, _CP0_EPC
    sw	    k0, CTX_EPC(sp)
    mfc0    k0, _CP0_BADVADDR
	sw	    k0, CTX_BADVADDR(sp)
    mfc0	k0, _CP0_STATUS
	sw	    k0, CTX_STATUS(sp)
    mfc0    k0, _CP0_CAUSE
    sw      k0, CTX_CAUSE(sp)
    .set pop
.endm

.macro _gpctx_load
    .set push
    .set noat
    lw     $1, CTX_REG(1)(sp)
    lw     v0, CTX_REG(2)(sp)
    lw     v1, CTX_REG(3)(sp)
    lw     a0, CTX_REG(4)(sp)
    lw     a1, CTX_REG(5)(sp)
    lw     a2, CTX_REG(6)(sp)
    lw     a3, CTX_REG(7)(sp)
    lw     t0, CTX_REG(8)(sp)
    lw     t1, CTX_REG(9)(sp)
    lw     t2, CTX_REG(10)(sp)
    lw     t3, CTX_REG(11)(sp)
    lw     t4, CTX_REG(12)(sp)
    lw     t5, CTX_REG(13)(sp)
    lw     t6, CTX_REG(14)(sp)
    lw     t7, CTX_REG(15)(sp)
    lw     s0, CTX_REG(16)(sp)
    lw     s1, CTX_REG(17)(sp)
    lw     s2, CTX_REG(18)(sp)
    lw     s3, CTX_REG(19)(sp)
    lw     s4, CTX_REG(20)(sp)
    lw     s5, CTX_REG(21)(sp)
    lw     s6, CTX_REG(22)(sp)
    lw     s7, CTX_REG(23)(sp)
    lw     t8, CTX_REG(24)(sp)
    lw     t9, CTX_REG(25)(sp)
    # restore k0 last
    lw     k1, CTX_REG(27)(sp)
    lw     gp, CTX_REG(28)(sp)
    # sp already restored
    lw     fp, CTX_REG(29)(sp)
    lw     ra, CTX_REG(30)(sp)

    di

#if (__mips_isa_rev < 6)
    lw	   k0, CTX_HI(sp)
    mthi   k0
    lw	   k0, CTX_LO(sp)
    mtlo   k0
#endif

    # cp0
    lw     k0, CTX_EPC(sp)
    mtc0   k0, _CP0_EPC
    lw     k0, CTX_STATUS(sp)
    # STATUS here will have EXL set
    mtc0   k0, _CP0_STATUS
    ehb
    # restore k0
    lw     k0, CTX_REG(26)(sp)
    .set pop
.endm

.text
.global get_global_pointer
.ent get_global_pointer
get_global_pointer:
    .set noat
    move   v0, gp
    jr     ra
.end

.text
.global isr_sw0
.ent isr_sw0
isr_sw0:
    .set noat
    rdpgpr  sp, sp

    # context switch
    _gpctx_save                     # save the context
    .set at
    lw      k0, g_current_task      # get current task
    beqz    k0, 1f                  # if there is a current task
    sw      sp, 0(k0)               # update stored sp
1:
    li      k0, _IFS0_CS0IF_MASK    # clear sw interrupt
    sw      k0, IFS0CLR

    lw      k0, g_os_run_list       # get new task
    sw      k0, g_current_task      # g_current_task = g_os_run_list

    lw      sp, 0(k0)               # restore sp
    .set noat
    _gpctx_load
    wrpgpr  sp, sp
    eret
.end
