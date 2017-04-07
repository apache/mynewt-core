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

#include <xc.h>

#define CTX_SIZE (35*4)
#define CTX_EPC (31*4)
#define CTX_BADVADDR (32*4)
#define CTX_STATUS (33*4)
#define CTX_CAUSE (34*4)

# note that k0 is not saved when using these macros.
# The cause register is saved but not resored.

# saves to location k0
.macro _gpctx_save
    sw      $1, 0(k0)
    sw      $2, 4(k0)
    sw      $3, 8(k0)
    sw      $4, 12(k0)
    sw      $5, 16(k0)
    sw      $6, 20(k0)
    sw      $7, 24(k0)
    sw      $8, 28(k0)
    sw      $9, 32(k0)
    sw      $10, 36(k0)
    sw      $11, 40(k0)
    sw      $12, 44(k0)
    sw      $13, 48(k0)
    sw      $14, 52(k0)
    sw      $15, 56(k0)
    sw      $16, 60(k0)
    sw      $17, 64(k0)
    sw      $18, 68(k0)
    sw      $19, 72(k0)
    sw      $20, 76(k0)
    sw      $21, 80(k0)
    sw      $22, 84(k0)
    sw      $23, 88(k0)
    sw      $24, 92(k0)
    sw      $25, 96(k0)
    # k0 not saved
    sw      $27, 104(k0)
    sw      $28, 108(k0)
    sw      $29, 112(k0)
    sw      $30, 116(k0)
    sw      $31, 120(k0)

    # cp0
    sw	    ra, CTX_EPC(k0)
    mfc0    $9, _CP0_BADVADDR /* PTR_MFC0? */
	sw	    $9, CTX_BADVADDR(k0)
    mfc0	$9, _CP0_STATUS
	sw	    $9, CTX_STATUS(k0)
    mfc0    $9, _CP0_CAUSE
    sw      $9, CTX_CAUSE(k0)

.endm

# restores from location a0

.macro _gpctx_load
    lw     $1, 0(a0)
    lw     $2, 4(a0)
    lw     $3, 8(a0)
    # a0 is loaded last
    lw     $5, 16(a0)
    lw     $6, 20(a0)
    lw     $7, 24(a0)
    lw     $8, 28(a0)
    lw     $9, 32(a0)
    lw     $10, 36(a0)
    lw     $11, 40(a0)
    lw     $12, 44(a0)
    lw     $13, 48(a0)
    lw     $14, 52(a0)
    lw     $15, 56(a0)
    lw     $16, 60(a0)
    lw     $17, 64(a0)
    lw     $18, 68(a0)
    lw     $19, 72(a0)
    lw     $20, 76(a0)
    lw     $21, 80(a0)
    lw     $22, 84(a0)
    lw     $23, 88(a0)
    lw     $24, 92(a0)
    lw     $25, 96(a0)
    # restore k0 after interrupts have been disabled
    lw     $27, 104(a0)
    lw     $28, 108(a0)
    # restore sp after interrupts have been disabled
    lw     $30, 116(a0)
    lw     $31, 120(a0)

    di
    lw     k0, CTX_EPC(a0)
    mtc0   k0, _CP0_EPC
    lw     k0, CTX_STATUS(a0)
    # STATUS here will have EXL set
    mtc0   k0, _CP0_STATUS
    ehb

    # restore k0 and sp as these may be overwritten by nested interrupts
    lw     $26, 100(a0)
    lw     $29, 112(a0)
    # restore a0 last
    lw     $4, 12(a0)
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
    # context switch
    .set noat
    rdpgpr  sp, sp
    # save k0 as its not saved in ctx_switch, hardcode values for now
    sw   $26, ((25 * 4) - ((((CTX_SIZE - 1) / 8) + 1) * 8))(sp)
    addi    k0, sp, -((((CTX_SIZE - 1) / 8) + 1) * 8)
    _gpctx_save                     # save the context
    .set at
    lw      t0, g_current_task      # get current task
    beqz    t0, 1f
    sw      k0, 0(t0)               # update stored sp
1:
    lw      t1, g_os_run_list       # get new task
    sw      t1, g_current_task      # g_current_task = g_os_run_list

    li      k0, _IFS0_CS0IF_MASK
    sw      k0, IFS0CLR

    lw      a0, 0(t1)
    .set noat
    _gpctx_load                     # updates actual sp
    wrpgpr  sp, sp

    eret
.end
