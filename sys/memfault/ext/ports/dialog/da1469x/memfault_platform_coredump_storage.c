//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Reference implementation of platform dependency functions to use space
//! on the external SPI flash connected to the DA1469xx for coredump capture
//!
//! By default, coredumps are saved in the log partition of NVMS Storage but
//! the location can be overriden by configuring the macros below.

#include "memfault/config.h"

#if MEMFAULT_PLATFORM_COREDUMP_STORAGE_USE_FLASH

#include "memfault/panics/platform/coredump.h"

#include "qspi_automode.h"
#include "bsp_memory_defaults.h"

// We default to the NVMS_LOG_PART if the user doesn't specify a partition.
#ifndef MEMFAULT_COREDUMP_STORAGE_START_ADDR
#define MEMFAULT_COREDUMP_STORAGE_START_ADDR NVMS_LOG_PART_START
#endif

#ifndef MEMFAULT_PLATFORM_COREDUMP_STORAGE_MAX_SIZE_BYTES
#define MEMFAULT_PLATFORM_COREDUMP_STORAGE_MAX_SIZE_BYTES NVMS_LOG_PART_SIZE
#endif

#if (MEMFAULT_COREDUMP_STORAGE_START_ADDR == NVMS_LOG_PART_START) && \
  (MEMFAULT_PLATFORM_COREDUMP_STORAGE_MAX_SIZE_BYTES > NVMS_LOG_PART_SIZE)
#error "MEMFAULT_PLATFORM_COREDUMP_STORAGE_MAX_SIZE_BYTES exceeds size of NVMS_LOG_PART"
#endif

#if ((MEMFAULT_COREDUMP_STORAGE_START_ADDR % FLASH_SECTOR_SIZE) != 0)
#error "MEMFAULT_COREDUMP_STORAGE_START_ADDR should be aligned by the sector size"
#endif

#if ((MEMFAULT_PLATFORM_COREDUMP_STORAGE_MAX_SIZE_BYTES % FLASH_SECTOR_SIZE) != 0)
#error "MEMFAULT_PLATFORM_COREDUMP_STORAGE_MAX_SIZE_BYTES should be aligned by the sector size"
#endif

//! Note: Backgrounded flash ops rely on FreeRTOS constructs being available and therefore can not
//! be used while saving a coredump from a fault handler.  To save coredumps and use background
//! flash ops, sdk/bsp/memory/src/qspi_automode.c in the DA1469x SDK will need to be patched
#if dg_configDISABLE_BACKGROUND_FLASH_OPS == 0
#error "dg_configDISABLE_BACKGROUND_FLASH_OPS must be set to 1 in custom_config_*.h"
#endif

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  *info = (sMfltCoredumpStorageInfo) {
    .size = MEMFAULT_PLATFORM_COREDUMP_STORAGE_MAX_SIZE_BYTES,
    .sector_size = FLASH_SECTOR_SIZE,
  };
}

static bool prv_op_within_flash_bounds(uint32_t offset, size_t data_len) {
  sMfltCoredumpStorageInfo info = { 0 };
  memfault_platform_coredump_storage_get_info(&info);
  return (offset + data_len) <= info.size;
}

bool memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                             size_t read_len) {
  if (!prv_op_within_flash_bounds(offset, read_len)) {
    return false;
  }

  const uint32_t address = MEMFAULT_COREDUMP_STORAGE_START_ADDR + offset;
  uint32_t bytes_read = qspi_automode_read(address, (uint8_t *)data, read_len);

  return (bytes_read == read_len);
}

bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  if (!prv_op_within_flash_bounds(offset, erase_size)) {
    return false;
  }

  const size_t sector_size = FLASH_SECTOR_SIZE;

  if ((offset % sector_size) != 0) {
    return false;
  }

  for (size_t sector = offset; sector < erase_size; sector += sector_size) {
    const uint32_t address = MEMFAULT_COREDUMP_STORAGE_START_ADDR + sector;
    qspi_automode_erase_flash_sector(address);
  }

  return true;
}

bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                              size_t data_len) {
  if (!prv_op_within_flash_bounds(offset, data_len)) {
    return false;
  }

  uint32_t address = MEMFAULT_COREDUMP_STORAGE_START_ADDR + offset;
  uint32_t bytes_written =  0 ;

  while (bytes_written < data_len) {
    bytes_written += qspi_automode_write_flash_page(address + bytes_written,
                                                    &((const uint8_t *)data)[bytes_written],
                                                    data_len - bytes_written);
  }


  return true;
}

// Note: This function is called while the system is running once the coredump has been read. We
// clear the first word in this scenario to avoid blocking the system for a long time on an erase.
void memfault_platform_coredump_storage_clear(void) {
  uint32_t clear_word = 0x0;
  memfault_platform_coredump_storage_write(0, &clear_word, sizeof(clear_word));
}

#endif /* MEMFAULT_PLATFORM_COREDUMP_STORAGE_USE_FLASH */
