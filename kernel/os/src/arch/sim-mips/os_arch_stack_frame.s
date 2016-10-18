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

#define sigsetjmp   __sigsetjmp

    .text
    .p2align 4, 0x90    /* align on 16-byte boundary and fill with NOPs */

    .globl os_arch_frame_init
    .globl _os_arch_frame_init
    /*
     * void os_arch_frame_init(struct stack_frame *sf)
     */

os_arch_frame_init:
    /* ABI stack frame */
    addi    $sp, $sp, -24           /* 8 bytes for register save, 16 for args */
    sw      $ra, 0x20($sp)          /* push ra to stack */
    sw      $s0, 0x16($sp)          /* push s0 to the stack */

    /* save and update sp to sf */
    move    $s0, $a0                /* move sf to s0 */
    sw      $sp, 0x0($s0)           /* sf->mainsp = stack pointer */
    /* fairly sure sf will be 8 byte alligned anyway, but no harm in and vs move */
    and     $sp, $s0, 0xfffffff1    /* stack pointer = sf 8 byte aligned */

    /* call setjmp */
    addi    $a0, $s0, 0x08          /* populate the arguments for sigsetjmp */
    move    $a1, $zero
    jal     sigsetjmp               /* sigsetjmp(sf->sf_jb, 0) */
    nop

    /* branch if starting task */
    bne     $v0, $zero, os_arch_frame_init_start
    nop

    /* back to main stack and return */
    lw      $sp, 0x0($s0)           /* back to main stack */
    lw      $ra, 0x20($sp)          /* pop ra from stack */
    lw      $s0, 0x16($sp)          /* pop s0 from the stack */
    addi    $sp, $sp, 24
    jr      $ra                     /* return */
    nop

os_arch_frame_init_start:
    move    $a0, $s0                /* populate arguments for task */
    move    $a1, $v0
    j       os_arch_task_start      /* jump to task, never to return */
    nop
