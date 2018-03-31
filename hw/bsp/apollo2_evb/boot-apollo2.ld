/* Linker script for Nordic Semiconductor nRF5 devices
 *
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
MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x4000

  /* NOTE: The last 8 bytes of SRAM are intentionally excluded.  The Apollo2's
   * in-ROM boot loader crashes when the initial stack pointer is at the end of
   * RAM.  Reducing the size of RAM slightly forces the initial MSP down by 8
   * bytes.
   */
  RAM (rwx) : ORIGIN = 0x10000000, LENGTH = 0x3fff8
}

/* The bootloader does not contain an image header */
_imghdr_size = 0x0;
