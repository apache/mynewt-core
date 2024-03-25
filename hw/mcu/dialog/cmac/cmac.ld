/*
 * Version: Sourcery G++ 4.5-1
 * Support: https://support.codesourcery.com/GNUToolchain/
 *
 * Copyright (c) 2007, 2008, 2009, 2010 CodeSourcery, Inc.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions.  No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */
OUTPUT_FORMAT ("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")

ENTRY(Reset_Handler)

SECTIONS
{
    /* Size of binary image, i.e. ROM + initialized data */
    img_size = LOADADDR(.data) + SIZEOF(.data);
    /* Size of RAM area required for CMAC */
    ram_size = LOADADDR(.data) + end_of_ram - ADDR(.data);
    /* Offset of data section in binary image */
    data_offset = LOADADDR(.data);
    /* Offset of shared section in binary image */
    shared_offset = LOADADDR(.data) + ADDR(.shm) - ADDR(.data);
    /* Base address of shared section on CMAC side (to translate pointers) */
    shared_addr = ADDR(.shm);

    .text :
    {
        __text = .;
        __intvect_start__ = .;
        __isr_vector_start = .;
        KEEP(*(.isr_vector))
        __isr_vector_end = .;

        /* Put image info just after intvect so loader can locate it easily */
        KEEP(*(.shm_magic))
        LONG(img_size)
        LONG(ram_size)
        LONG(data_offset)
        LONG(shared_offset)
        LONG(shared_addr)
        KEEP(*(.shm_vect))

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

INCLUDE "link_tables.ld.h"
        *(.rodata*)

        *(.eh_frame*)
        . = ALIGN(4);
    } > ROM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } > ROM

    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        . = ALIGN(4);
    } > ROM

    .data : ALIGN(1K)
    {
        . = ALIGN(4);
        __vector_tbl_reloc__ = .;
        . = . + (__isr_vector_end - __isr_vector_start);

        . = ALIGN(4);
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
        _edata = ABSOLUTE(.);
    } > RAM AT > ROM

    .bssnz (NOLOAD) :
    {
        . = ALIGN(4);
        __bssnz_start__ = .;
        *(.bss.core.nz*)
        . = ALIGN(4);
        __bssnz_end__ = .;
    } > RAM

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    .heap_dummy (NOLOAD) :
    {
        . = ALIGN(8);
        *(.heap*)
    } > RAM

    .stack_dummy (NOLOAD) :
    {
        . = ALIGN(8);
        *(.stack*)
    } > RAM

    .shm (NOLOAD) : ALIGN(1K)
    {
        __shm_start__ = .;
        *(.shm_data)
        __shm_end__ = .;
    } > RAM

    . = ALIGN(1K);
    end_of_ram = .;
    _eram = ABSOLUTE(.);

    .ram :
    {
        BYTE(0xAA)
        FILL(0xAA)
        . = . + (_eram - _edata - 1);
    } AT > ROM

    __HeapBase = ADDR(.heap_dummy);
    __StackTop = ADDR(.shm);

    /* Move stack limit down by size of stack_dummy section */
    __StackLimit = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);

    /* Top of heap is the bottom of the stack */
    __HeapLimit = __StackLimit;

    /* Check if data + heap + stack exceeds RAM limit */
    ASSERT(__HeapBase <= __HeapLimit, "region RAM overflowed with stack")
}
