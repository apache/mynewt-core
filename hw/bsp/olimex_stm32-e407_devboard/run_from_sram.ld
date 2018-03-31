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

/* Linker script for STM32F407 when running code from SRAM */

/* Linker script to configure memory regions. */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
  CCM (rwx) : ORIGIN = 0x10000000, LENGTH = 64K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x20000
}

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

SECTIONS
{
    .text :
    {
        __vector_tbl_reloc__ = .;
        KEEP(*(.isr_vector))
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
    } > RAM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > RAM

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > RAM

    __exidx_end = .;

    . = ALIGN(8);
    __etext = .;

    .coredata : AT (__etext)
    {
        __coredata_start__ = .;
        *(.data.core)
        . = ALIGN(8);
        __coredata_end__ = .;
    } > CCM

    __ecoredata = __etext + SIZEOF(.coredata);

    /* This section is here so that the start of .data has the same VMA and LMA */
    .ram_coredata (NOLOAD):
    {
        . = . + SIZEOF(.coredata);
    } > RAM

    .data : AT (__ecoredata)
    {
        __data_start__ = .;
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
        __data_end__ = .;

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
    } > CCM

    .bss :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    . = ALIGN(8);
    __HeapBase = .;
    __HeapLimit = ORIGIN(RAM) + LENGTH(RAM);

    /* .stack_dummy section doesn't contains any symbols. It is only
     * used for linker to calculate size of stack sections, and assign
     * values to stack symbols later */
    .stack_dummy (COPY):
    {
        *(.stack*)
    } > CCM

    /* Set stack top to end of CCM; stack limit is bottom of stack */
    __StackTop = ORIGIN(CCM) + LENGTH(CCM);
    __StackLimit = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);

    /* Check for CCM overflow */
    ASSERT(__StackLimit >= __ecorebss, "CCM overflow!")
}

