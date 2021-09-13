//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/esp_port/spi_flash.h"

#include "esp_flash.h"
#include "esp_spi_flash.h"
#include "esp_flash_internal.h"

#ifdef CONFIG_SPI_FLASH_USE_LEGACY_IMPL
#error "The v4.x esp-idf port expects CONFIG_SPI_FLASH_USE_LEGACY_IMPL=n"
#endif

int memfault_esp_spi_flash_coredump_begin(void) {
  // re-configure flash driver to be call-able from an Interrupt context

  spi_flash_guard_set(&g_flash_guard_no_os_ops);
  return esp_flash_app_disable_protect(true);
}

int memfault_esp_spi_flash_erase_range(size_t start_address, size_t size) {
  return esp_flash_erase_region(esp_flash_default_chip, start_address, size);
}

int memfault_esp_spi_flash_write(size_t dest_addr, const void *src, size_t size) {
  return esp_flash_write(esp_flash_default_chip, src, dest_addr, size);
}

int memfault_esp_spi_flash_read(size_t src_addr, void *dest, size_t size) {
  return esp_flash_read(esp_flash_default_chip, dest, src_addr, size);
}
