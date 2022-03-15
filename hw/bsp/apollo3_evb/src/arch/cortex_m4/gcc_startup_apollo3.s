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
    .arch armv7e-m

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
    .long   NMI_Handler                 /* NMI Handler */
    .long   HardFault_Handler           /* Hard Fault Handler */
    .long   MemoryManagement_Handler    /* Memory Fault */
    .long   BusFault_Handler            /* Bus Fault */
    .long   UsageFault_Handler          /* Usage Fault */
    .long   SecureFault_Handler         /* Secure Fault */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   SVC_Handler                 /* SVCall Handler */
    .long   DebugMon_Handler            /* Debug Monitor */
    .long   0                           /* Reserved */
    .long   PendSV_Handler              /* PendSV Handler */
    .long   SysTick_Handler             /* SysTick Handler */

  /* External Interrupts */
    .long   BROWNOUT_IRQHandler
    .long   WATCHDOG_IRQHandler
    .long   RTC_IRQHandler
    .long   VCOMP_IRQHandler
    .long   IOSLAVE_IOS_IRQHandler
    .long   IOSLAVE_ACC_IRQHandler
    .long   IOMASTER0_IRQHandler
    .long   IOMASTER1_IRQHandler
    .long   IOMASTER2_IRQHandler
    .long   IOMASTER3_IRQHandler
    .long   IOMASTER4_IRQHandler
    .long   IOMASTER5_IRQHandler
    .long   BLE_IRQHandler
    .long   GPIO_IRQHandler
    .long   CTIMER_IRQHandler
    .long   UART0_IRQHandler
    .long   UART1_IRQHandler
    .long   SCARD_IRQHandler
    .long   ADC_IRQHandler
    .long   PDM_IRQHandler
    .long   MSPI_IRQHandler
    .long   SOFTWARE0_IRQHandler
    .long   STIMER_IRQHandler
    .long   STIMER_CMPR0_IRQHandler
    .long   STIMER_CMPR1_IRQHandler
    .long   STIMER_CMPR2_IRQHandler
    .long   STIMER_CMPR3_IRQHandler
    .long   STIMER_CMPR4_IRQHandler
    .long   STIMER_CMPR5_IRQHandler
    .long   STIMER_CMPR6_IRQHandler
    .long   STIMER_CMPR7_IRQHandler
    .long   CLKGEN_IRQHandler
__ble_patch:
/* The patch table should pad the vector table size to a total of 64 entries
 * (16 core + 48 periph) such that code begins at offset 0x100.
 */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */

    .size    __isr_vector, . - __isr_vector

/* Reset Handler */

    .text
    .thumb
    .thumb_func
    .align 1
    .globl    Reset_Handler
    .type    Reset_Handler, %function
Reset_Handler:
    .fnstart

    /* Clear BSS */
    mov     r0, #0
    ldr     r2, =__bss_start__
    ldr     r3, =__bss_end__
.bss_zero_loop:
    cmp     r2, r3
    itt     lt
    strlt   r0, [r2], #4
    blt    .bss_zero_loop

/*     Loop to copy data from read only memory to RAM. The ranges
 *      of copy from/to are specified by following symbols evaluated in
 *      linker script.
 *      __etext: End of code section, i.e., begin of data sections to copy from.
 *      __data_start__/__data_end__: RAM address range that data should be
 *      copied to. Both must be aligned to 4 bytes boundary.  */

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

	ldr		r0, =__StackTop
	msr		psp, r0
	msr		msp, r0

    LDR     R0, =__HeapBase
    LDR     R1, =__HeapLimit
    BL      _sbrkInit

    LDR     R0, =SystemInit
    BLX     R0

    BL      hal_system_init

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


    .weak   HardFault_Handler
    .type   HardFault_Handler, %function
HardFault_Handler:
    B       .
    .size   HardFault_Handler, . - HardFault_Handler


    .weak   MemoryManagement_Handler
    .type   MemoryManagement_Handler, %function
MemoryManagement_Handler:
    B       .
    .size   MemoryManagement_Handler, . - MemoryManagement_Handler


    .weak   BusFault_Handler
    .type   BusFault_Handler, %function
BusFault_Handler:
    B       .
    .size   BusFault_Handler, . - BusFault_Handler


    .weak   UsageFault_Handler
    .type   UsageFault_Handler, %function
UsageFault_Handler:
    B       .
    .size   UsageFault_Handler, . - UsageFault_Handler


    .weak   SecureFault_Handler
    .type   SecureFault_Handler, %function
SecureFault_Handler:
    B       .
    .size   SecureFault_Handler, . - SecureFault_Handler


    .weak   SVC_Handler
    .type   SVC_Handler, %function
SVC_Handler:
    B       .
    .size   SVC_Handler, . - SVC_Handler


    .weak   DebugMon_Handler
    .type   DebugMon_Handler, %function
DebugMon_Handler:
    B       .
    .size   DebugMon_Handler, . - DebugMon_Handler


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


/* IRQ Handlers */

    .globl  Default_Handler
    .type   Default_Handler, %function
Default_Handler:
    B       .
    .size   Default_Handler, . - Default_Handler

    .macro  IRQ handler
    .weak   \handler
    .set    \handler, Default_Handler
    .endm

    IRQ BROWNOUT_IRQHandler
    IRQ WATCHDOG_IRQHandler
    IRQ RTC_IRQHandler
    IRQ VCOMP_IRQHandler
    IRQ IOSLAVE_IOS_IRQHandler
    IRQ IOSLAVE_ACC_IRQHandler
    IRQ IOMASTER0_IRQHandler
    IRQ IOMASTER1_IRQHandler
    IRQ IOMASTER2_IRQHandler
    IRQ IOMASTER3_IRQHandler
    IRQ IOMASTER4_IRQHandler
    IRQ IOMASTER5_IRQHandler
    IRQ BLE_IRQHandler
    IRQ GPIO_IRQHandler
    IRQ CTIMER_IRQHandler
    IRQ UART0_IRQHandler
    IRQ UART1_IRQHandler
    IRQ SCARD_IRQHandler
    IRQ ADC_IRQHandler
    IRQ PDM_IRQHandler
    IRQ MSPI_IRQHandler
    IRQ SOFTWARE0_IRQHandler
    IRQ STIMER_IRQHandler
    IRQ STIMER_CMPR0_IRQHandler
    IRQ STIMER_CMPR1_IRQHandler
    IRQ STIMER_CMPR2_IRQHandler
    IRQ STIMER_CMPR3_IRQHandler
    IRQ STIMER_CMPR4_IRQHandler
    IRQ STIMER_CMPR5_IRQHandler
    IRQ STIMER_CMPR6_IRQHandler
    IRQ STIMER_CMPR7_IRQHandler
    IRQ CLKGEN_IRQHandler

  .end
