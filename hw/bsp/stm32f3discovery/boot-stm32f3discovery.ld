/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
ENTRY(Reset_Handler)

/*
 * Memory map
 */
MEMORY {
	FLASH (rx):	ORIGIN = 0x08000000, LENGTH = 32K
	CCRAM (rw):	ORIGIN = 0x10000000, LENGTH = 8K
	SRAM  (rw):	ORIGIN = 0x20000000, LENGTH = 40K
}

/*
 * Define output sections
 */
SECTIONS {
	.text : {
		_text = .;
		__isr_vector = .;
		KEEP(*(.isr_vector*))
		__eisr_vector = .;
		*(.text* .gnu.linkonce.t.*)
		_etext = .;
		. = ALIGN(4);
		*(.rodata*)
	} > FLASH
	.sram_isrs : {
		__vector_tbl_reloc__ = .;
		. = . + (__eisr_vector - __isr_vector);
	} > SRAM
	.data : {
		_sdata = .;
		_sidata = LOADADDR(.data);
		*(.data*)
		_edata = .;
	} > SRAM AT > FLASH
	.bss : {
		_sbss = .;
		__bss_start__ = .;
		*(.bss*)
		*(COMMON)
		__bss_end__ = .;
		_ebss = .;
		. = ALIGN(4);
	} > SRAM
	.heap : {
		_heap = .;
	} > SRAM
	_eheap = ORIGIN(SRAM) + LENGTH(SRAM);
	_estack = ORIGIN(CCRAM) + LENGTH(CCRAM);
}
