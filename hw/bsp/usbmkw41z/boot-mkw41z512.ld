/*
** ###################################################################
**     Processor:           MKW41Z
**     Compiler:            GNU C Compiler
**
**     Abstract:
**         Linker file for the GNU C Compiler
**
**     Copyright (c) 2015 Freescale Semiconductor, Inc.
**     All rights reserved.
**
**     Redistribution and use in source and binary forms, with or without modification,
**     are permitted provided that the following conditions are met:
**
**     o Redistributions of source code must retain the above copyright notice, this list
**       of conditions and the following disclaimer.
**
**     o Redistributions in binary form must reproduce the above copyright notice, this
**       list of conditions and the following disclaimer in the documentation and/or
**       other materials provided with the distribution.
**
**     o Neither the name of Freescale Semiconductor, Inc. nor the names of its
**       contributors may be used to endorse or promote products derived from this
**       software without specific prior written permission.
**
**     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
**     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
**     WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
**     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
**     ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
**     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
**     ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
**     (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
**     SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**     http:                 www.freescale.com
**     mail:                 support@freescale.com
**
** ###################################################################
*/
OUTPUT_FORMAT ("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")

/* Entry Point */
ENTRY(Reset_Handler)

/*
 * NOTE: there are 16 bytes of flash configuration at address 0x400.
 * The bootloader starts at 0 and is 24KB (0x6000). Thus, we need to subtract
 * 0x410 from the total bootloader length and that is how we get 0x5BF0.
 */
MEMORY
{
  VECTOR_TBL (rx) : ORIGIN = 0x00000000, LENGTH = 0x100
  FCF (rx) : ORIGIN = 0x00000400, LENGTH = 0x10
  FLASH (rx) : ORIGIN = 0x00000410, LENGTH = 0x5BF0
  RAM (rwx) : ORIGIN = 0x1FFF8000, LENGTH = 0x20000
}

/* Define output sections */
SECTIONS
{
    .interrupts :
    {
        __isr_vector_start = .;
        KEEP(*(.isr_vector))
        __isr_vector_end = .;
    } > VECTOR_TBL

    .flash_config :
    {
        . = ALIGN(4);
        KEEP(*(.FlashConfig))    /* Flash Configuration Field (FCF) */
        . = ALIGN(4);
    } > FCF

    .text :
    {
        *(.text*)

        KEEP(*(.init))
        KEEP(*(.fini))

        /* .ctors */
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)

        /* .dtors */
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)

        *(.rodata*)

        *(.eh_frame*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } > FLASH

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        . = ALIGN(4);
    } > FLASH
    __exidx_end = .;

    __etext = .;

    /* Keep first in RAM, as well as in bootloader */
    .vector_relocation :
    {
        . = ALIGN(4);
        __vector_tbl_reloc__ = .;
        . = . + (__isr_vector_end - __isr_vector_start);
        . = ALIGN(4);
    } > RAM

    .data :
    {
        __data_start__ = .;
        *(vtable)
        *(.data*)

        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__preinit_array_start = .);
        *(.preinit_array)
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN (__init_array_start = .);
        *(SORT(.init_array.*))
        *(.init_array)
        PROVIDE_HIDDEN (__init_array_end = .);


        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN (__fini_array_start = .);
        *(SORT(.fini_array.*))
        *(.fini_array)
        PROVIDE_HIDDEN (__fini_array_end = .);

        *(.jcr)
        . = ALIGN(4);
        /* All data end */
        __data_end__ = .;
    } > RAM AT > FLASH

    .bss :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    /* Heap starts after BSS */
    . = ALIGN(8);
    __HeapBase = .;

    /* .stack_dummy section doesn't contains any symbols. It is only
     * used for linker to calculate size of stack sections, and assign
     * values to stack symbols later */
    .stack_dummy (COPY):
    {
        *(.stack*)
    } > RAM

    _ram_start = ORIGIN(RAM);

    /* Set stack top to end of RAM, and stack limit move down by
     * size of stack_dummy section */
    __StackTop = ORIGIN(RAM) + LENGTH(RAM);
    __StackLimit = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);

    /* Top of head is the bottom of the stack */
    __HeapLimit = __StackLimit;

    /* Check if data + heap + stack exceeds RAM limit */
    ASSERT(__HeapBase <= __HeapLimit, "region RAM overflowed with stack")
}

