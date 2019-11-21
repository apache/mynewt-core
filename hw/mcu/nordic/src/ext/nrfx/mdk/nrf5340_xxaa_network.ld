/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x1000000, LENGTH = 0x40000
  RAM (rwx) : ORIGIN = 0x21000000, LENGTH = 0x10000
}


INCLUDE "nrf_common.ld"
