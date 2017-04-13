/*
 * Copyright 2016, Imagination Technologies Limited and/or its
 *                 affiliated group companies.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include <mips/regdef.h>
#include <mips/asm.h>
#include <mips/cpu.h>
#include <mips/hal.h>

#define OS_STACK_ALIGNMENT  (8)

#define CTX_ALIGNED_SIZE ((((CTX_SIZE - 1) / OS_STACK_ALIGNMENT) + 1) * \
    OS_STACK_ALIGNMENT)

# note that k0 is not saved when using these macros.
# The cause register is saved but not resored.

# saves to location k0

.macro _gpctx_save
    REG_S   $1, CTX_REG(1) (k0)
    REG_S   $2, CTX_REG(2) (k0)
    REG_S   $3, CTX_REG(3) (k0)
    REG_S   $4, CTX_REG(4) (k0)
    REG_S   $5, CTX_REG(5) (k0)
    REG_S   $6, CTX_REG(6) (k0)
    REG_S   $7, CTX_REG(7) (k0)
    REG_S   $8, CTX_REG(8) (k0)
    REG_S   $9, CTX_REG(9) (k0)
    REG_S   $10, CTX_REG(10) (k0)
    REG_S   $11, CTX_REG(11) (k0)
    REG_S   $12, CTX_REG(12) (k0)
    REG_S   $13, CTX_REG(13) (k0)
    REG_S   $14, CTX_REG(14) (k0)
    REG_S   $15, CTX_REG(15) (k0)
    REG_S   $16, CTX_REG(16) (k0)
    REG_S   $17, CTX_REG(17)(k0)
    REG_S   $18, CTX_REG(18)(k0)
    REG_S   $19, CTX_REG(19)(k0)
    REG_S   $20, CTX_REG(20)(k0)
    REG_S   $21, CTX_REG(21)(k0)
    REG_S   $22, CTX_REG(22)(k0)
    REG_S   $23, CTX_REG(23)(k0)
    REG_S   $24, CTX_REG(24)(k0)
    REG_S   $25, CTX_REG(25)(k0)
    # k0 not saved
    REG_S   $27, CTX_REG(27)(k0)
    REG_S   $28, CTX_REG(28)(k0)
    REG_S   $29, CTX_REG(29)(k0)
    REG_S   $30, CTX_REG(30)(k0)
    REG_S   $31, CTX_REG(31)(k0)
    PTR_S   $0, CTX_LINK(k0)        # Clear the link field

#if (__mips_isa_rev < 6)
	mfhi	$9
	mflo	$10
	REG_S	$9, CTX_HI0(k0)
	REG_S	$10, CTX_LO0(k0)
#endif

    # cp0
    PTR_MFC0 $9, C0_EPC
    REG_S	$9, CTX_EPC(k0)
    PTR_MFC0 $9, C0_BADVADDR
	REG_S	$9, CTX_BADVADDR(k0)
    mfc0	$9, C0_SR
	sw	    $9, CTX_STATUS(k0)
    mfc0    $9, C0_CR
    sw      $9, CTX_CAUSE(k0)

    move	$11, $0
	move	$12, $0
	mfc0	$9, C0_CONFIG3
	ext	    $10, $9, CFG3_BP_SHIFT, 1
	beqz	$10, 1f
	mfc0	$11, C0_BADPINSTR
1:
	ext	    $9, $9, CFG3_BI_SHIFT, 1
	beqz	$9, 1f
	mfc0    $12, C0_BADINSTR
1:
	sw	    $11, CTX_BADPINSTR(k0)
	sw	    $12, CTX_BADINSTR(k0)
.endm

# restores from location a0

.macro _gpctx_load
#if (__mips_isa_rev < 6)
	REG_L	$9, CTX_HI0(a0)
	REG_L	$10, CTX_LO0(a0)
	mthi	$9
	mtlo	$10
#endif

    REG_L   $1, CTX_REG(1)(a0)
    REG_L   $2, CTX_REG(2)(a0)
    REG_L   $3, CTX_REG(3)(a0)
    # a0 is loaded last
    REG_L   $5, CTX_REG(5)(a0)
    REG_L   $6, CTX_REG(6)(a0)
    REG_L   $7, CTX_REG(7)(a0)
    REG_L   $8, CTX_REG(8)(a0)
    REG_L   $9, CTX_REG(9)(a0)
    REG_L   $10, CTX_REG(10)(a0)
    REG_L   $11, CTX_REG(11)(a0)
    REG_L   $12, CTX_REG(12)(a0)
    REG_L   $13, CTX_REG(13)(a0)
    REG_L   $14, CTX_REG(14)(a0)
    REG_L   $15, CTX_REG(15)(a0)
    REG_L   $16, CTX_REG(16)(a0)
    REG_L   $17, CTX_REG(17)(a0)
    REG_L   $18, CTX_REG(18)(a0)
    REG_L   $19, CTX_REG(19)(a0)
    REG_L   $20, CTX_REG(20)(a0)
    REG_L   $21, CTX_REG(21)(a0)
    REG_L   $22, CTX_REG(22)(a0)
    REG_L   $23, CTX_REG(23)(a0)
    REG_L   $24, CTX_REG(24)(a0)
    REG_L   $25, CTX_REG(25)(a0)
    # restore k0 after interrupts have been disabled
    REG_L   $27, CTX_REG(27)(a0)
    REG_L   $28, CTX_REG(28)(a0)
    # restore sp after interrupts have been disabled
    REG_L   $30, CTX_REG(30)(a0)
    REG_L   $31, CTX_REG(31)(a0)

    di

    REG_L   k0, CTX_EPC(a0)
    PTR_MTC0 k0, C0_EPC
    lw      k0, CTX_STATUS(a0)
    # STATUS here will have EXL set
    mtc0    k0, C0_SR
    ehb

    # restore k0 and sp as these may be overwritten by nested interrupts
    REG_L   $26, CTX_REG(26)(a0)
    REG_L   $29, CTX_REG(29)(a0)
    # restore a0 last
    REG_L   $4, CTX_REG(4)(a0)
.endm

.text
.global get_global_pointer
.ent get_global_pointer
get_global_pointer:
    .set noat
    move    v0, gp
    jr      ra
.end

.text
.global _mips_isr_sw0
.ent _mips_isr_sw0
_mips_isr_sw0:
    # context switch
    .set noat
    # save k0
    REG_S   $26, (CTX_REG(25) - CTX_ALIGNED_SIZE)(sp)
    addi    k0, sp, -CTX_ALIGNED_SIZE
    _gpctx_save                     # save the context
    .set at
    lw      t0, g_current_task      # get current task
    beqz    t0, 1f
    sw      k0, 0(t0)               # update stored sp
1:
    lw      t1, g_os_run_list       # get new task
    sw      t1, g_current_task      # g_current_task = g_os_run_list
    mfc0    k0, C0_CR
    andi    k0, k0, 0xfeff          # clear interrupt in cause register
    lui     k1, 0x0080              # make sure IV is set
    or      k0, k0, k1
    mtc0    k0, C0_CR
    lw      a0, 0(t1)
    .set noat
    _gpctx_load                     # updates actual sp
    eret
.end
