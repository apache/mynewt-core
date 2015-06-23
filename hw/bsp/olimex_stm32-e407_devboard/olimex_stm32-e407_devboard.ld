/*
 * Copyright SilverSpring Networks 2004 - 2011
 * All rights reserved.
 *
 * Linker script template
 *
 * $Id: armelf.tpl 33163 2012-06-12 23:48:26Z cdo $
 */

/* 
 * Variables that start with BSP_ are filled in via a sed command in 
 * the make stage.  Check `pwd`/Makefile.inc for the actual definitions
 * of these values.
 *
 *
 * This linker script exposes the following variables to the initialization
 * code on startup:
 *
 * _text =      Beginning VMA address of the .text section
 * _etext =     End VMA address of the .text section
 * _data =      Beginning VMA address of the .data section
 * _edata =     End VMA address of the .data section
 * _fc =        Beginning VMA address of the .fc section
 * _efc =       End VMA address of the .fc section
 * _int_sram =  Beginning VMA address of the .int_sram section
 * _eint_sram = End VMA address of the .int_sram section
 * _bss =       Beginning VMA address of the .bss section
 * _ebss =      End VMA address of the .bss section
 *
 * _fc_load =   Beginning LMA address of the .fc section
 *
 *  The following data is in internal SRAM
 * __stack_start = Beginning address of the stack
 * __stack_end = End address of the stack
 *
 * The following addresses are defined for 16MB PSRAM and must be redefined in
 * boot.s after the PSRAM size check is performed
 * __heap_start = Beginning address of the heap
 * __heap_end = End address of the heap
 * __cri_start = Beginning address of the cri number block
 * __cri_end = End address of the cri number block
 * 
 * These variables are used and referenced in init_sects.c, make sure to keep
 * init_sects.c up to date when changing any of these variables.
 */
ENTRY(Reset_Handler)

SECTIONS
{
    INTSRAM_SIZE = 0x00010000;
    INTRAM_BEG = 0x10000000;
    INTRAM_END = INTRAM_BEG + INTSRAM_SIZE;

    PSRAM_BEG = 0x20000000;

    TEXT_LINK_ADDR = 0x20000000;
    TEXT_LOAD_ADDR = 0x20000000; 

    .text TEXT_LINK_ADDR : AT(TEXT_LOAD_ADDR) {
        _text = .;
        KEEP(*(.isr_vector))
        KEEP(*startup_STM32F40x.o (.text))
        KEEP(*(.boot_text))
        KEEP(*boot.o (.text))
        *(.text .text.*)
        *(.text_nofc)
        *(.glue_7t)
        *(.glue_7)
        *(.rodata .rodata.*)
        *(.rodata.str1.*)
        . = ALIGN(32);
        _etext = .;
    }

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    }
    __exidx_end = .;

    _etext = .;
    __etext = .;
    
    .data _etext : AT(_etext) {
        _data = .;
        __data_start__ = .;
        *(.boot_data)
        *(.got.plt) *(.got)
        *(.data .data.*)
        . = ALIGN(32);
        _edata = .;
        __data_end__ = .;
    }

    .fc _edata : AT(_etext + SIZEOF(.data)) {
        _fc = .;
        *(.fc)
        *(.os_crit_text)
        . = ALIGN(32);
        _efc = .;
    }

    .int_sram INTRAM_BEG (NOLOAD) : {
        _int_sram = .;
        *(.int_sram)
        . = ALIGN(32);
        _eint_sram = .;
    }

    /* .stack_dummy section doesn't contains any symbols. It is only
     * used for linker to calculate size of stack sections, and assign
     * values to stack symbols later */
    .stack_dummy (COPY):
    {
        *(.stack*)
    }

    .bss _efc (NOLOAD) : {
        __bss_start__ = .;
        _bss = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(32);
        _ebss = .;
        __bss_end__ = .;
    }

    __end__ = .;
    end = __end__;
 
    _end = .;
  
    _fc_load = _etext + SIZEOF(.data);

    PSRAM_LEN = 0x20000;
    PSRAM_END = PSRAM_BEG + PSRAM_LEN;

    __intram_start = INTRAM_BEG;
    __intram_end   = INTRAM_END;

    /* Set stack top to end of RAM, and stack limit move down by
     * size of stack_dummy section */
    __StackTop = INTRAM_END;
    __StackLimit = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);

    __intsram_chk = 0;

    /* Debugging information */

    /* Stabs debugging sections.  */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }
    /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /* SGI/MIPS DWARF 2 extensions */
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
}

ASSERT(__intsram_chk == 0, "Internal SRAM overflow");


