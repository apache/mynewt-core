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

/* Entry Point */
ENTRY(Reset_Handler)

/* Define output sections */
SECTIONS
{
    /* The startup code goes first into internal flash */
    .interrupts :
    {
        . = ALIGN(4);
        __isr_vector = .;
        __isr_vector_start = .;
        KEEP(*(.isr_vector))      /* Startup code */
        __isr_vector_end = .;
    } > FLASH

    /* Keep first in RAM, no need to clear it */
    .vector_relocation :
    {
        . = ALIGN(4);
        __vector_tbl_reloc__ = .;
        . = . + (__isr_vector_end - __isr_vector_start);
        . = ALIGN(4);
    } > RAM

    /* This section will be zeroed by RTT package init */
    .rtt (NOLOAD):
    {
        . = ALIGN(4);
        *(.rtt)
        . = ALIGN(4);
    } > RAM

    /* The program code and other data goes into internal image location */
    .text :
    {
        . = ALIGN(4);
        *(.text)                 /* .text sections (code) */
        *(.text*)                /* .text* sections (code) */

        KEEP(*(.init))
        KEEP(*(.fini))

        /* preinit data */
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);

        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);

        /* .ctors */
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)

        /* .dtors */
        KEEP(*crtbegin.o(.dtors))
        KEEP(*crtbegin?.o(.dtors))
        KEEP(*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors))
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))

        *(.rodata)               /* .rodata sections (constants, strings, etc.) */
        *(.rodata*)              /* .rodata* sections (constants, strings, etc.) */
        *(.glue_7)               /* glue arm to thumb code */
        *(.glue_7t)              /* glue thumb to arm code */
        *(.eh_frame)
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .copy.table :
    {
        . = ALIGN(4);
        __copy_table_start__ = .;

        /* Copy data section to RAM */
        LONG (LOADADDR(.text_ram))                          /* From */
        LONG (__text_ram_start__)                           /* To   */
        LONG (SIZEOF(.text_ram))                            /* Size */

        /* Copy coredata section to RAM */
        LONG (LOADADDR(.coredata))                          /* From */
        LONG (__coredata_start__)                           /* To   */
        LONG (SIZEOF(.coredata))                            /* Size */

        /* Copy data section to RAM */
        LONG (LOADADDR(.data))                              /* From */
        LONG (__data_start__)                               /* To   */
        LONG (SIZEOF(.data))                                /* Size */

        __copy_table_end__ = .;
    } > FLASH

    __etext = .;    /* define a global symbol at end of code */
    __text_ram_addr = LOADADDR(.text_ram);

    .text_ram :
    {
        __text_ram_start__ = .;
        *(.text_ram*)
        *(.ramfunc*)             /* for functions in ram */
        . = ALIGN(4);
        __text_ram_end__ = .;
    } > RAM AT > FLASH
    __text_ram_image__ = LOADADDR(.text_ram);

    .coredata :
    {
        __coredata_start__ = .;
        *(.data.core)
        . = ALIGN(4);
        __coredata_end__ = .;
    } > RAM AT > FLASH
    __coredata_image__ = LOADADDR(.coredata);
    __ecoredata = __etext + SIZEOF(.coredata);

    .data :
    {
        __data_start__ = .;
        *(vtable)
        *(.data*)

        KEEP(*(.jcr*))
        . = ALIGN(4);
        /* All data end */
        __data_end__ = .;

    } > RAM AT > FLASH
    __data_image__ = LOADADDR(.data);

    .bssnz :
    {
        . = ALIGN(4);
        __bssnz_start__ = .;
        *(.bss.core.nz*)
        . = ALIGN(4);
        __bssnz_end__ = .;
    } > RAM

    .corebss (NOLOAD):
    {
        . = ALIGN(4);
        __corebss_start__ = .;
        *(.bss.core)
        . = ALIGN(4);
        __corebss_end__ = .;
        *(.corebss*)
        *(.bss.core.nz)
        . = ALIGN(4);
        __ecorebss = .;
    } > RAM

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

    _ram_start = ORIGIN(RAM);

    /* Set stack top to end of RAM, and stack limit move down by
     * size of stack_dummy section */
    __StackTop = ORIGIN(RAM) + LENGTH(RAM);
    _estack = __StackTop;
    __StackLimit = __StackTop - STACK_SIZE;
    PROVIDE(__stack = __StackTop);

    /* Top of head is the bottom of the stack */
    __HeapLimit = __StackLimit;

    /* Check if data + heap + stack exceeds RAM limit */
    ASSERT(__HeapBase <= __HeapLimit, "region RAM overflowed with stack")
}

