/*
Copyright (c) 2015, Nordic Semiconductor ASA
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of Nordic Semiconductor ASA nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
NOTE: Template files (including this one) are application specific and therefore
expected to be copied into the application project folder prior to its use!
*/

    .syntax unified
    .arch armv6-m

    .section .stack
    .align 3
    .equ    Stack_Size, 384
    .globl    __StackTop
    .globl    __StackLimit
__StackLimit:
    .space    Stack_Size
    .size __StackLimit, . - __StackLimit
__StackTop:
    .size __StackTop, . - __StackTop

    .section .heap
    .align 3
#ifdef __HEAP_SIZE
    .equ    Heap_Size, __HEAP_SIZE
#else
    .equ    Heap_Size, 0
#endif
    .globl    __HeapBase
    .globl    __HeapLimit
__HeapBase:
    .if    Heap_Size
    .space    Heap_Size
    .endif
    .size __HeapBase, . - __HeapBase
__HeapLimit:
    .size __HeapLimit, . - __HeapLimit

    .section .isr_vector_split
    .align 2
    .globl __isr_vector_split
__isr_vector_split:
    .long    __StackTop                 /* Top of Stack */
    .long   Reset_Handler_split         /* Reset Handler */

    .size    __isr_vector_split, . - __isr_vector_split

/* Reset Handler */

    .equ    NRF_POWER_RAMON_ADDRESS,             0x40000524
    .equ    NRF_POWER_RAMONB_ADDRESS,            0x40000554
    .equ    NRF_POWER_RAMONx_RAMxON_ONMODE_Msk,  0x3

    .text
    .thumb
    .thumb_func
    .align 1
    .globl    Reset_Handler_split
    .type    Reset_Handler_split, %function
Reset_Handler_split:
    .fnstart

/* Clear CPU state before proceeding */
    SUBS    r0, r0
    MSR     CONTROL, r0
    MSR     PRIMASK, r0

/* Make sure ALL RAM banks are powered on */
    MOVS    R1, #NRF_POWER_RAMONx_RAMxON_ONMODE_Msk

    LDR     R0, =NRF_POWER_RAMON_ADDRESS
    LDR     R2, [R0]
    ORRS    R2, R1
    STR     R2, [R0]

    LDR     R0, =NRF_POWER_RAMONB_ADDRESS
    LDR     R2, [R0]
    ORRS    R2, R1
    STR     R2, [R0]

    /* Clear BSS */
    subs    r0, r0
    ldr     r2, =__bss_start__
    ldr     r3, =__bss_end__
.bss_zero_loop:
    cmp     r2, r3
    bhs     .data_copy_loop
    stmia   r2!, {r0}
    b    .bss_zero_loop

/*     Loop to copy data from read only memory to RAM. The ranges
 *      of copy from/to are specified by following symbols evaluated in
 *      linker script.
 *      __etext: End of code section, i.e., begin of data sections to copy from.
 *      __data_start__/__data_end__: RAM address range that data should be
 *      copied to. Both must be aligned to 4 bytes boundary.  */
.data_copy_loop:
    ldr    r1, =__etext
    ldr    r2, =__data_start__
    ldr    r3, =__data_end__

    subs    r3, r2
    ble     .LC0

.LC1:
    subs    r3, 4
    ldr    r0, [r1,r3]
    str    r0, [r2,r3]
    bgt    .LC1
.LC0:
    ldr    r1, =__etext_loader
    ldr    r2, =__data_start___loader
    ldr    r3, =__data_end___loader

    subs    r3, r2
    ble     .LC2

.LC3:
    subs    r3, 4
    ldr    r0, [r1,r3]
    str    r0, [r2,r3]
    bgt    .LC3
.LC2:

    subs    r0, r0
    ldr    r2, =__bss_start___loader
    ldr    r3, =__bss_end___loader

    subs    r3, r2
    ble     .LC4

.LC5:
    subs    r3, 4
    str    r0, [r2,r3]
    bgt    .LC5
.LC4:
    LDR     R0, =__HeapBase
    LDR     R1, =__HeapLimit
    BL      _sbrkInit

    LDR     R0, =SystemInit
    BLX     R0

    BL      hal_system_init

    LDR     R0, =_start_split
    BX      R0

    .pool
    .cantunwind
    .fnend
    .size   Reset_Handler_split,.-Reset_Handler_split

    .section ".text"

  .end
