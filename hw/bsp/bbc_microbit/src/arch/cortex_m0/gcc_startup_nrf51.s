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
    .equ    Stack_Size, 432
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

    .section .isr_vector
    .align 2
    .globl __isr_vector
__isr_vector:
    .long    __StackTop                 /* Top of Stack */
    .long   Reset_Handler               /* Reset Handler */
    .long   _NMI_Handler                /* NMI Handler */
    .long   _HardFault_Handler          /* Hard Fault Handler */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   _SVC_Handler                /* SVCall Handler */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   _PendSV_Handler             /* PendSV Handler */
    .long   _SysTick_Handler            /* SysTick Handler */

  /* External Interrupts */
    .long   _POWER_CLOCK_IRQHandler
    .long   _RADIO_IRQHandler
    .long   _UART0_IRQHandler
    .long   _SPI0_TWI0_IRQHandler
    .long   _SPI1_TWI1_IRQHandler
    .long   0                         /*Reserved */
    .long   _GPIOTE_IRQHandler
    .long   _ADC_IRQHandler
    .long   _TIMER0_IRQHandler
    .long   _TIMER1_IRQHandler
    .long   _TIMER2_IRQHandler
    .long   _RTC0_IRQHandler
    .long   _TEMP_IRQHandler
    .long   _RNG_IRQHandler
    .long   _ECB_IRQHandler
    .long   _CCM_AAR_IRQHandler
    .long   _WDT_IRQHandler
    .long   _RTC1_IRQHandler
    .long   _QDEC_IRQHandler
    .long   _LPCOMP_IRQHandler
    .long   _SWI0_IRQHandler
    .long   _SWI1_IRQHandler
    .long   _SWI2_IRQHandler
    .long   _SWI3_IRQHandler
    .long   _SWI4_IRQHandler
    .long   _SWI5_IRQHandler
    .long   0                         /*Reserved */
    .long   0                         /*Reserved */
    .long   0                         /*Reserved */
    .long   0                         /*Reserved */
    .long   0                         /*Reserved */
    .long   0                         /*Reserved */

    .size    __isr_vector, . - __isr_vector

/* Reset Handler */

    .equ    NRF_POWER_RAMON_ADDRESS,             0x40000524
    .equ    NRF_POWER_RAMONB_ADDRESS,            0x40000554
    .equ    NRF_POWER_RAMONx_RAMxON_ONMODE_Msk,  0x3

    .text
    .thumb
    .thumb_func
    .align 1
    .globl    Reset_Handler
    .type    Reset_Handler, %function
Reset_Handler:
    .fnstart

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
    LDR     R0, =__HeapBase
    LDR     R1, =__HeapLimit
    BL      _sbrkInit

    LDR     R0, =SystemInit
    BLX     R0
    LDR     R0, =_start
    BX      R0

    .pool
    .cantunwind
    .fnend
    .size   Reset_Handler,.-Reset_Handler

    .section ".text"


/* Dummy Exception Handlers (infinite loops which can be modified) */

    .weak   NMI_Handler
    .type   NMI_Handler, %function
NMI_Handler:
    B       .
    .size   NMI_Handler, . - NMI_Handler

    .weak   SVC_Handler
    .type   SVC_Handler, %function
SVC_Handler:
    B       .
    .size   SVC_Handler, . - SVC_Handler


    .weak   PendSV_Handler
    .type   PendSV_Handler, %function
PendSV_Handler:
    B       .
    .size   PendSV_Handler, . - PendSV_Handler


    .weak   SysTick_Handler
    .type   SysTick_Handler, %function
SysTick_Handler:
    B       .
    .size   SysTick_Handler, . - SysTick_Handler

/* Default handler. This uses the vector in the relocated vector table */
    .globl  Default_Handler
    .type   Default_Handler, %function
Default_Handler:
    LDR     R2, =__vector_tbl_reloc__
    MRS     R0, PSR
    MOVS    R1, #0x3F
    ANDS    R0, R1
    LSLS    R0, R0, #2
    LDR     R0, [R0, R2]
    BX      R0
    .size   Default_Handler, . - Default_Handler

/*
 * All of the following IRQ Handlers will point to the default handler unless
 * they are defined elsewhere.
 */
    .macro  IRQ handler
    .weak   \handler
    .set    \handler, Default_Handler
    .endm

    IRQ  _NMI_Handler
    IRQ  _HardFault_Handler
    IRQ  _SVC_Handler
    IRQ  _PendSV_Handler
    IRQ  _SysTick_Handler
    IRQ  _POWER_CLOCK_IRQHandler
    IRQ  _RADIO_IRQHandler
    IRQ  _UART0_IRQHandler
    IRQ  _SPI0_TWI0_IRQHandler
    IRQ  _SPI1_TWI1_IRQHandler
    IRQ  _GPIOTE_IRQHandler
    IRQ  _ADC_IRQHandler
    IRQ  _TIMER0_IRQHandler
    IRQ  _TIMER1_IRQHandler
    IRQ  _TIMER2_IRQHandler
    IRQ  _RTC0_IRQHandler
    IRQ  _TEMP_IRQHandler
    IRQ  _RNG_IRQHandler
    IRQ  _ECB_IRQHandler
    IRQ  _CCM_AAR_IRQHandler
    IRQ  _WDT_IRQHandler
    IRQ  _RTC1_IRQHandler
    IRQ  _QDEC_IRQHandler
    IRQ  _LPCOMP_IRQHandler
    IRQ  _SWI0_IRQHandler
    IRQ  _SWI1_IRQHandler
    IRQ  _SWI2_IRQHandler
    IRQ  _SWI3_IRQHandler
    IRQ  _SWI4_IRQHandler
    IRQ  _SWI5_IRQHandler

  .end
