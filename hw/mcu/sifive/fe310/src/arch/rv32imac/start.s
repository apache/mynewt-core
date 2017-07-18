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

#include <env/encoding.h>

    .section .init
    .globl _reset_handler
    .type _reset_handler,@function

_reset_handler:
    la gp, _gp
    la sp, _sp

    /* Disable Local/Timer/External interrupts */
    csrc mstatus, MSTATUS_MIE
    csrr t0, mie
    li t1, ~(MIP_MSIP | MIP_MTIP | MIP_MEIP)
    and t0, t0, t1
    csrw mie, t0

    /* Load data section from flash */
    la a0, _data_lma
    la a1, _data
    la a2, _edata
    bgeu a1, a2, 2f
1:
    lw t0, (a0)
    sw t0, (a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b
2:

    /* Clear bss section */
    la a0, __bss_start
    la a1, _end
    bgeu a0, a1, 2f
1:
    sw zero, (a0)
    addi a0, a0, 4
    bltu a0, a1, 1b
2:

    /* Init heap */
    la a0, _end
    la a1, _heap_end
    call _sbrkInit

    /* Call global constructors */
    la a0, __libc_fini_array
    call atexit
    call __libc_init_array

    call _init
    call _start
    call _fini
    tail exit
