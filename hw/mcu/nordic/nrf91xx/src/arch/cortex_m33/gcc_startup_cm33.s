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
    .arch armv8-m.main

    .section .stack
#if defined(__STACK_SIZE)
    .align 3
    .equ    Stack_Size, __STACK_SIZE
#else
    .align 3
    .equ    Stack_Size, 432
#endif
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

    .section .isr_vector, "ax"
    .align 2
    .globl __isr_vector
__isr_vector:
    .long    __StackTop            /* Top of Stack */
    .long   Reset_Handler               /* Reset Handler */
    .long   NMI_Handler                 /* NMI Handler */
    .long   HardFault_Handler           /* Hard Fault Handler */
    .long   MemoryManagement_Handler
    .long   BusFault_Handler
    .long   UsageFault_Handler
    .long   SecureFault_Handler
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   SVC_Handler                 /* SVCall Handler */
    .long   DebugMon_Handler
    .long   0                           /* Reserved */
    .long   PendSV_Handler              /* PendSV Handler */
    .long   SysTick_Handler             /* SysTick Handler */

  /* External Interrupts */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   SPU_IRQHandler
    .long   0
    .long   CLOCK_POWER_IRQHandler
    .long   0                           /* Reserved */
    .long   0                           /* Reserved */
    .long   UARTE0_SPIM0_SPIS0_TWIM0_TWIS0_IRQHandler
    .long   UARTE1_SPIM1_SPIS1_TWIM1_TWIS1_IRQHandler
    .long   UARTE2_SPIM2_SPIS2_TWIM2_TWIS2_IRQHandler
    .long   UARTE3_SPIM3_SPIS3_TWIM3_TWIS3_IRQHandler
    .long   0
    .long   GPIOTE0_IRQHandler
    .long   SAADC_IRQHandler
    .long   TIMER0_IRQHandler
    .long   TIMER1_IRQHandler
    .long   TIMER2_IRQHandler
    .long   0
    .long   0
    .long   RTC0_IRQHandler
    .long   RTC1_IRQHandler
    .long   0
    .long   0
    .long   WDT_IRQHandler
    .long   0
    .long   0
    .long   EGU0_IRQHandler
    .long   EGU1_IRQHandler
    .long   EGU2_IRQHandler
    .long   EGU3_IRQHandler
    .long   EGU4_IRQHandler
    .long   EGU5_IRQHandler
    .long   PWM0_IRQHandler
    .long   PWM1_IRQHandler
    .long   PWM2_IRQHandler
    .long   PWM3_IRQHandler
    .long   0
    .long   PDM_IRQHandler
    .long   0
    .long   I2S_IRQHandler
    .long   0
    .long   IPC_IRQHandler
    .long   0                         /* Reserved */
    .long   FPU_IRQHandler
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   GPIOTE1_IRQHandler
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   KMU_IRQHandler
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   CRYPTOCELL_IRQHandler
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */
    .long   0                         /* Reserved */

    .size    __isr_vector, . - __isr_vector

/* Reset Handler */

    .text
    .thumb
    .thumb_func
    .align 1
    .globl Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:

    /* Clear BSS */
    ldr     r2, =__bss_start__
    ldr     r3, =__bss_end__
    mov     r0, 0
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

    subs    r3, r3, r2
    ble     .LC0

.LC1:
    subs   r3, r3, #4
    ldr    r0, [r1,r3]
    str    r0, [r2,r3]
    bgt    .LC1

.LC0:

    LDR     R0, =__HeapBase
    LDR     R1, =__HeapLimit
    BL      _sbrkInit

    LDR     R0, =SystemInit
    BLX     R0

    BL      hal_system_init

    LDR     R0, =_start
    BX      R0

    .pool
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
    b       .
    .size   SecureFault_Handler, . - SecureFault_Handler


    .weak   SVC_Handler
    .type   SVC_Handler, %function
SVC_Handler:
    B       .
    .size   SVC_Handler, . - SVC_Handler


    .weak   DebugMon_Handler
    .type   DebugMon_Handler, %function
DebugMon_Handler:
    b       .
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

    IRQ  SPU_IRQHandler
    IRQ  CLOCK_POWER_IRQHandler
    IRQ  UARTE0_SPIM0_SPIS0_TWIM0_TWIS0_IRQHandler
    IRQ  UARTE1_SPIM1_SPIS1_TWIM1_TWIS1_IRQHandler
    IRQ  UARTE2_SPIM2_SPIS2_TWIM2_TWIS2_IRQHandler
    IRQ  UARTE3_SPIM3_SPIS3_TWIM3_TWIS3_IRQHandler
    IRQ  GPIOTE0_IRQHandler
    IRQ  SAADC_IRQHandler
    IRQ  TIMER0_IRQHandler
    IRQ  TIMER1_IRQHandler
    IRQ  TIMER2_IRQHandler
    IRQ  RTC0_IRQHandler
    IRQ  RTC1_IRQHandler
    IRQ  WDT_IRQHandler
    IRQ  EGU0_IRQHandler
    IRQ  EGU1_IRQHandler
    IRQ  EGU2_IRQHandler
    IRQ  EGU3_IRQHandler
    IRQ  EGU4_IRQHandler
    IRQ  EGU5_IRQHandler
    IRQ  PWM0_IRQHandler
    IRQ  PWM1_IRQHandler
    IRQ  PWM2_IRQHandler
    IRQ  PWM3_IRQHandler
    IRQ  PDM_IRQHandler
    IRQ  I2S_IRQHandler
    IRQ  IPC_IRQHandler
    IRQ  FPU_IRQHandler
    IRQ  GPIOTE1_IRQHandler
    IRQ  KMU_IRQHandler
    IRQ  CRYPTOCELL_IRQHandler

   .end
