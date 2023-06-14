/* stack: dynamic */
/* heap: dynamic */

ENTRY(Reset_Handler)

SECTIONS
{
  .imghdr (NOLOAD):
  {
      . = . + _imghdr_size;
  } > FLASH

  __text = .;

  /* text: executable code */
  /* located in _flash_ */
  .text :
  {
    . = ALIGN(4);
    __isr_vector_start = .;
    KEEP(*(.isr_vector))
    KEEP(*(.ble_patch))
    __isr_vector_end = .;
    *(.text)
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

    /* .rodata */
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)

    PROVIDE(mynewt_main = main);
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

  .vector_relocation :
  {
      . = ALIGN(4);
      __vector_tbl_reloc__ = .;
      . = . + (__isr_vector_end - __isr_vector_start);
      . = ALIGN(4);
  } > RAM

  /* data: uniquely initialized symbols */
  /* loaded into flash region, copied to sram region at startup */
  /* VMA appears in sram region, LMA is in flash region for initialization */
  /* _init_data used by startup to locate flash region copy of data */
  .data :
  {
    __data_start__ = .;
    *(vtable)
    *(.data)
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
    __data_end__ = .;
  } > RAM AT>FLASH
  _init_data = LOADADDR(.data); /* used by startup to initialize data */

  /* bss: zero-initialized symbols */
  /* don't require flash memory to remember their value */
  .bss :
  {
    . = ALIGN(4);
    __bss_start__ = .;
    *(.bss)
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
