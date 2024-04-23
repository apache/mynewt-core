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

#include <syscfg/syscfg.h>
#include <sysflash/sysflash.h>
#include <mcu_config.ld.h>
#include <bsp_config.ld.h>
#include <target_config.ld.h>
#include <mynewt_config.ld.h>

/* Entry Point */
ENTRY(RESET_HANDLER)
EXTERN(g_pfnVectors)

/* Specify the memory areas */
MEMORY
{
#ifdef FLASH_AREA_BOOTLOADER_OFFSET
    BOOT (rx!w) : ORIGIN = FLASH_AREA_BOOTLOADER_OFFSET, LENGTH = FLASH_AREA_BOOTLOADER_SIZE
#endif
#ifdef FLASH_AREA_IMAGE_0_OFFSET
    SLOT0 (rx!w) : ORIGIN = FLASH_AREA_IMAGE_0_OFFSET, LENGTH = FLASH_AREA_IMAGE_0_SIZE
#endif
    /*
     * If STACK_REGION is defined it means that MCU stack is place in other region then RAM
     * int that case RAM region size is exactly RAM_SIZE
     * If STACK_REGION is NOT defined STACK_RAM region is place at the end of RAM
     * and RAM region size is shortened.
     */
#ifdef STACK_REGION
    RAM (rwx) : ORIGIN = RAM_START, LENGTH = RAM_SIZE
#else
    RAM (rwx) : ORIGIN = RAM_START, LENGTH = (RAM_SIZE - STACK_SIZE)
    STACK_RAM (rw) : ORIGIN = RAM_START + RAM_SIZE - STACK_SIZE, LENGTH = STACK_SIZE
#endif
#include <memory_regions.ld.h>
}

/* Define output sections */
SECTIONS
{
    /*
     * Image header is added by newt tool during execution newt image-create.
     * This section allows to have complete elf with predefined image header that
     * will satisfy mcuboot and yet elf file can be written to device
     * with debugger or IDE that supports elf writing.
     * This section is not provided by bootloader (or for small-flash devices that
     * don't have enough space for boot loader.
     * newt create-image will remove this section when it creates .bin file.
     */
    .image_header :
    {
        KEEP(*(.image_header))      /* Image header */
    } > MYNEWT_CODE

    /* The startup code goes first into internal flash */
    .interrupts :
    {
        . = ALIGN(4);
        __isr_vector = .;
        __isr_vector_start = .;
        KEEP(*(.isr_vector))      /* Startup code */
        __isr_vector_end = .;
    } > MYNEWT_CODE

    /* Keep first in RAM, no need to clear it */
    .vector_relocation :
    {
        . = ALIGN(4);
        __vector_tbl_reloc__ = .;
        . = . + (__isr_vector_end - __isr_vector_start);
        . = ALIGN(4);
    } > VECTOR_RELOCATION_RAM

    /* This section will be zeroed by RTT package init */
    .rtt (NOLOAD):
    {
        . = ALIGN(4);
        *(.rtt)
        . = ALIGN(4);
    } > RTT_RAM

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

#include <link_tables.ld.h>

        *(.rodata)               /* .rodata sections (constants, strings, etc.) */
        *(.rodata*)              /* .rodata* sections (constants, strings, etc.) */
        *(.glue_7)               /* glue arm to thumb code */
        *(.glue_7t)              /* glue thumb to arm code */
        *(.eh_frame)

        /* Just in case some old code still has main instead of mynewt_main */
        PROVIDE(mynewt_main = main);
    } > MYNEWT_CODE

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > MYNEWT_CODE

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > MYNEWT_CODE

    .copy.table :
    {
        . = ALIGN(4);
        __copy_table_start__ = .;

        LONG (LOADADDR(.interrupts))                        /* From */
        LONG (SIZEOF(.interrupts))                          /* Size */
        LONG (__vector_tbl_reloc__)                         /* To   */

#ifdef TEXT_RAM
        /* Copy data section to RAM */
        LONG (LOADADDR(.text_ram))                          /* From */
        LONG (SIZEOF(.text_ram))                            /* Size */
        LONG (__text_ram_start__)                           /* To   */
#endif

#ifdef COREDATA_RAM
        /* Copy coredata section to RAM */
        LONG (LOADADDR(.coredata))                          /* From */
        LONG (SIZEOF(.coredata))                            /* Size */
        LONG (__coredata_start__)                           /* To   */
#endif

        /* Copy data section to RAM */
        LONG (LOADADDR(.data))                              /* From */
        LONG (SIZEOF(.data))                                /* Size */
        LONG (__data_start__)                               /* To   */

        __copy_table_end__ = .;
    } > MYNEWT_CODE

    .zero.table :
    {
        . = ALIGN(4);
        __zero_table_start__ = .;

#ifdef COREBSS_RAM
        /* Zero core bss section */
        LONG (__corebss_start__)                            /* From */
        LONG (SIZEOF(.corebss))                             /* Size */
#endif

        /* Zero bss section */
        LONG (__bss_start__)                                /* From */
        LONG (SIZEOF(.bss))                                 /* Size */

        __zero_table_end__ = .;
    } > MYNEWT_CODE

    __etext = .;    /* define a global symbol at end of code */
    __text_ram_addr = LOADADDR(.text_ram);

#ifdef TEXT_RAM
    .text_ram :
    {
        . = ALIGN(4);
        __text_ram_start__ = .;
        *(.text_ram*)
        *(.ramfunc*)             /* for functions in ram */
        . = ALIGN(4);
        __text_ram_end__ = .;
    } > TEXT_RAM AT > MYNEWT_CODE
    __text_ram_image__ = LOADADDR(.text_ram);
#endif

#ifdef COREDATA_RAM
    .coredata :
    {
        __coredata_start__ = .;
        *(.data.core)
        . = ALIGN(4);
        __coredata_end__ = .;
    } > COREDATA_RAM AT > MYNEWT_CODE
    __coredata_image__ = LOADADDR(.coredata);
    __ecoredata = __etext + SIZEOF(.coredata);
#endif

    .data :
    {
        __data_start__ = .;
        _sdata = .;
        *(vtable)
        *(.data*)

        KEEP(*(.jcr*))
        . = ALIGN(4);
        /* All data end */
        __data_end__ = .;
        _edata = .;

    } > DATA_RAM AT > MYNEWT_CODE
    __data_image__ = LOADADDR(.data);
    _sidata = LOADADDR(.data);

    .bssnz (NOLOAD) :
    {
        . = ALIGN(4);
        __bssnz_start__ = .;
        *(.bss.core.nz*)
        . = ALIGN(4);
        __bssnz_end__ = .;
    } > BSSNZ_RAM

#ifdef COREBSS_RAM
    .corebss (NOLOAD):
    {
        . = ALIGN(4);
        __corebss_start__ = .;
        *(.bss.core)
        *(.corebss*)
        . = ALIGN(4);
        __corebss_end__ = .;
        . = ALIGN(4);
        __ecorebss = .;
    } > COREBSS_RAM
#endif

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = .;
    } > BSS_RAM

    /* Heap starts after BSS */
    . = ALIGN(8);
    __HeapBase = .;
    /* Top of head is the bottom of the stack */
    __HeapLimit = ORIGIN(RAM) + LENGTH(RAM);

    /* Check if data + heap + stack exceeds RAM limit */
    ASSERT(__HeapBase <= __HeapLimit, "No space for MCU heap")

    _ram_start = ORIGIN(RAM);

    /* Set stack top to end of RAM, and stack limit move down by
     * size of stack_dummy section */
    __StackTop = ORIGIN(STACK_RAM) + LENGTH(STACK_RAM);
    __StackLimit = ORIGIN(STACK_RAM);
    _estack = __StackTop;
    PROVIDE(__stack = __StackTop);
}

