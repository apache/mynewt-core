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

/* Linker script to place sections and symbol values. Should be used together
 * with other linker script that defines memory regions FLASH and RAM.
 * It references following symbols, which must be defined in code:
 *   Reset_Handler : Entry of reset handler
 *
 * It defines following symbols, which code can use without definition:
 *   __exidx_start
 *   __exidx_end
 *   __etext
 *   __data_start__
 *   __preinit_array_start
 *   __preinit_array_end
 *   __init_array_start
 *   __init_array_end
 *   __fini_array_start
 *   __fini_array_end
 *   __data_end__
 *   __bss_start__
 *   __bss_end__
 *   __end__
 *   end
 *   __HeapBase
 *   __HeapLimit
 *   __StackLimit
 *   __StackTop
 *   __stack
 *   __coredata_start__
 *   __coredata_end__
 *   __corebss_start__
 *   __corebss_end__
 *   __ecoredata
 *   __ecorebss
 */
ENTRY(Reset_Handler)

_estack = 0x20040000;

SECTIONS
{
    /* Reserve space at the start of the image for the header. */
    .imghdr (NOLOAD):
    {
        . = . + _imghdr_size;
    } > FLASH

    .text :
    {
        __isr_vector_start = .;
        KEEP(*(.isr_vector))
        __isr_vector_end = .;
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

        KEEP(*(.eh_frame*))
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH

    __exidx_end = .;

    __etext = .;

    .vector_relocation :
    {
        . = ALIGN(4);
        __vector_tbl_reloc__ = .;
        . = . + (__isr_vector_end - __isr_vector_start);
        . = ALIGN(4);
    } > RAM

    .coredata :
    {
        __coredata_start__ = .;
        *(.data.core)
        . = ALIGN(4);
        __coredata_end__ = .;
    } > DTCM AT > FLASH

    __ecoredata = __etext + SIZEOF(.coredata);

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        __data_start__ = _sdata;
        *(vtable)
        *(.data*)

        /* preinit data */
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);


        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);

        KEEP(*(.jcr*))
        . = ALIGN(4);
        /* All data end */
        _edata = .;
        __data_end__ = _edata;

    } > RAM AT > FLASH

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
    } > DTCM

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    __HeapBase = .;
    __HeapLimit = ORIGIN(RAM) + LENGTH(RAM);

    _ram_start = ORIGIN(RAM);
    _dtcmram_start = ORIGIN(DTCM);

    /* .stack_dummy section doesn't contains any symbols. It is only
     * used for linker to calculate size of stack sections, and assign
     * values to stack symbols later */
    .stack_dummy (COPY):
    {
        *(.stack*)
    } > DTCM

    /* Set stack top to end of DTCM; stack limit is bottom of stack */
    __StackTop = ORIGIN(DTCM) + LENGTH(DTCM);
    __StackLimit = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);

    /* Check for DTCM overflow */
    ASSERT(__StackLimit >= __ecorebss, "DTCM overflow!")
}

