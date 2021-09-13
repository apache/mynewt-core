//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Reference implementation of platform dependency functions to use sectors of internal flash
//! on the EFM/EFR Memory System Controller.
//!
//! To use, update your linker script (.ld file) to expose information about the location to use.
//! For example, using the last 64K of the EFM32PG12BxxxF1024 (1MB flash) would look
//! something like this:
//!
//! MEMORY
//! {
//!    /* ... other regions ... */
//!    COREDUMP_STORAGE_FLASH (rx) : ORIGIN = 0xF0000, LENGTH = 64K
//! }
//! __MemfaultCoreStorageStart = ORIGIN(COREDUMP_STORAGE_FLASH);
//! __MemfaultCoreStorageEnd = ORIGIN(COREDUMP_STORAGE_FLASH) + LENGTH(COREDUMP_STORAGE_FLASH);

#include "memfault/config.h"

#if MEMFAULT_PLATFORM_COREDUMP_STORAGE_USE_FLASH

#include "memfault/panics/coredump.h"
#include "memfault/ports/buffered_coredump_storage.h"

#include <string.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/core.h"
#include "memfault/panics/platform/coredump.h"

#include "em_device.h"
#include "em_msc.h"

#ifndef MEMFAULT_COREDUMP_STORAGE_START_ADDR
extern uint32_t __MemfaultCoreStorageStart[];
#define MEMFAULT_COREDUMP_STORAGE_START_ADDR ((uint32_t)__MemfaultCoreStorageStart)
#endif

#ifndef MEMFAULT_COREDUMP_STORAGE_END_ADDR
extern uint32_t __MemfaultCoreStorageEnd[];
#define MEMFAULT_COREDUMP_STORAGE_END_ADDR ((uint32_t)__MemfaultCoreStorageEnd)
#endif

// Error writing to flash - should never happen & likely detects a configuration error
// Call the reboot handler which will halt the device if a debugger is attached and then reboot
MEMFAULT_NO_OPT
static void prv_coredump_writer_assert_and_reboot(int error_code) {
  memfault_platform_halt_if_debugging();
  memfault_platform_reboot();
}

static bool prv_op_within_flash_bounds(uint32_t offset, size_t data_len) {
  sMfltCoredumpStorageInfo info = { 0 };
  memfault_platform_coredump_storage_get_info(&info);
  return (offset + data_len) <= info.size;
}

void memfault_platform_coredump_storage_clear(void) {
  uint32_t *addr = (void *)MEMFAULT_COREDUMP_STORAGE_START_ADDR;
  uint32_t zeros = 0;

  const MSC_Status_TypeDef rv = MSC_WriteWord(addr, &zeros, sizeof(zeros));
  if ((rv != mscReturnOk) || ((*addr) != 0)) {
    MEMFAULT_LOG_ERROR("Failed to clear coredump storage, rv=%d", (int)rv);
  }
}

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  const size_t size =
      MEMFAULT_COREDUMP_STORAGE_END_ADDR - MEMFAULT_COREDUMP_STORAGE_START_ADDR;

  *info  = (sMfltCoredumpStorageInfo) {
    .size = size,
    .sector_size = FLASH_PAGE_SIZE,
  };
}

bool memfault_platform_coredump_storage_buffered_write(sCoredumpWorkingBuffer *blk) {
  const uint32_t addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR + blk->write_offset;

  const MSC_Status_TypeDef rv = MSC_WriteWord((void *)addr, blk->data, sizeof(blk->data));
  if (rv != mscReturnOk) {
    prv_coredump_writer_assert_and_reboot(rv);
  }

  return true;
}

bool memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                             size_t read_len) {
  if (!prv_op_within_flash_bounds(offset, read_len)) {
    return false;
  }

  // The internal flash is memory mapped so we can just use memcpy!
  const uint32_t start_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR;
  memcpy(data, (void *)(start_addr + offset), read_len);
  return true;
}

bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  if (!prv_op_within_flash_bounds(offset, erase_size)) {
    return false;
  }

  if (((offset % FLASH_PAGE_SIZE) != 0) || (erase_size % FLASH_PAGE_SIZE) != 0) {
    // configuration error
    prv_coredump_writer_assert_and_reboot(offset);
  }

  const uint32_t start_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR + offset;

  for (size_t sector_offset = 0; sector_offset < erase_size; sector_offset += FLASH_PAGE_SIZE) {
    const MSC_Status_TypeDef rv = MSC_ErasePage((void *)(start_addr + sector_offset));
    if (rv != mscReturnOk) {
      prv_coredump_writer_assert_and_reboot(rv);
    }
  }

  return true;
}

#endif /* MEMFAULT_PLATFORM_COREDUMP_STORAGE_USE_FLASH */
