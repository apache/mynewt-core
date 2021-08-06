
/*
 * Part of the MEMORY section that defines application memory size.
 * This file is included by MCU linker script.
 * this file does not have whole MEMORY section to make it clear
 * what are differences between MCUs (Flash size and RAM), config
 * sections are same.
 */

  /* Program leave space for image header ~192KB */
  kseg0_program_mem     (rx)  : ORIGIN = 0x9D000020, LENGTH = 0x2FFE0
  /* Cachable RAM 512KB */
  kseg0_data_mem       (w!x)  : ORIGIN = 0x80000000, LENGTH = 0x80000
