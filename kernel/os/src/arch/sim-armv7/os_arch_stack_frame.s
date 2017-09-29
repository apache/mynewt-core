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

#define sigsetjmp   __sigsetjmp

    .text
    .p2align 4, 0x90    /* align on 16-byte boundary and fill with NOPs */

    .globl os_arch_frame_init
    /*
     * void os_arch_frame_init(struct stack_frame *sf)
     */
os_arch_frame_init:
    mov	    r1, sp
    mov	    sp, r0		/* stack for the task starts from sf */
    sub     sp, sp, #24	/* stack must be aligned by 8 */
    str	    r1, [sp, #12]
    str     lr, [sp, #16]	/* Store LR there */
    str     r0, [sp, #4]	/* Store sf pointer to stack */
    add     r0, r0, #8		/* $r0 = sf->sf_jb */
    mov     r1, #0
    bl      __sigsetjmp
    cmp     r0, #0		/* If $r0 == 0 then return */
    beq     end
    mov	    r1, r0
    ldr     r0, [sp, #4]
    and     sp, sp, #0xfffffff8
    bl      os_arch_task_start
end:
    ldr	    r1, [sp, #16]	/* return $sp for the callee */
    ldr     sp, [sp, #12]
    mov	    pc, r1
