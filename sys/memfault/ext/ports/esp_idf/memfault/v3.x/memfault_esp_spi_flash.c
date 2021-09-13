//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/esp_port/spi_flash.h"

#include "esp_spi_flash.h"

int memfault_esp_spi_flash_coredump_begin(void) {
  // re-configure flash driver to be call-able from an Interrupt context
  spi_flash_guard_set(&g_flash_guard_no_os_ops);
  return 0;
}

int memfault_esp_spi_flash_erase_range(size_t start_address, size_t size) {
  return spi_flash_erase_range(start_address, size);
}

int memfault_esp_spi_flash_write(size_t dest_addr, const void *src, size_t size) {
  return spi_flash_write(dest_addr, src, size);
}

int memfault_esp_spi_flash_read(size_t src_addr, void *dest, size_t size) {
  return spi_flash_read(src_addr, dest, size);
}
