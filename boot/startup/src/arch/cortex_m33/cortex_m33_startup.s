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

    .syntax unified
    .arch   armv7-m

    .text
    .thumb
    .thumb_func
    .align 2
    .globl Reset_Handler
    .type  Reset_Handler, %function
Reset_Handler:
    mov     r0, #0
    msr     control, r0

    ldr     r0, =__StackLimit
    msr     msplim, r0

    ldr	r4, =__copy_table_start__
    ldr	r5, =__copy_table_end__

.L_for_each_copy_region:
    cmp     r4, r5
    bge     .L_copy_table_done
    ldmia   r4!, {r1, r2, r3}

.L_copy_loop:
    subs    r2, #4
    ittt    gt
    ldrgt   r0, [r1, r2]
    strgt   r0, [r3, r2]
    bgt	.L_copy_loop

    b   .L_for_each_copy_region

.L_copy_table_done:

    ldr	r4, =__zero_table_start__
    ldr	r5, =__zero_table_end__

.L_for_each_zero_region:
    cmp     r4, r5
    bge     .L_zero_table_done
    ldmia   r4!, {r1, r2}
    mov     r0, #0

.L_zero_loop:
    subs    r2, #4
    itt     gt
    strgt   r0, [r1, r2]
    bgt     .L_zero_loop

    b       .L_for_each_zero_region
.L_zero_table_done:

    ldr     r0, =__HeapBase
    ldr     r1, =__HeapLimit
    bl      _sbrkInit
    bl      SystemInit
    bl      hal_system_init
    bl      _start

    .size   Reset_Handler, . - Reset_Handler
