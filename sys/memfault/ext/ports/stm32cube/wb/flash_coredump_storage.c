//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Reference implementation of platform dependency functions to use a sector of internal flash
//! for coredump capture
//!
//! STM32WB55xx/STM32WB35xx Flash topology
//!  - Single Bank, Up to 1MB
//!  – Page Size: 4kB
//!  - double-word operations only (64 bits plus 8 ECC bits)
//!
//! Note: Wireless Coprocessor Binary is programmed to the top of internal flash so
//! be sure to place coredump region before that. More details can be found in "Release_Notes.html"
//! within STM32CubeWB SDK: https://github.com/STMicroelectronics/STM32CubeWB/tree/v1.10.1/Projects/STM32WB_Copro_Wireless_Binaries
//!
//! To use this port, update your linker script (.ld file) to reserve a region for
//! coredump storage such as the COREDUMP_STORAGE_FLASH region below.
//!
//! MEMORY
//! {
//!    FLASH (rx)                  : ORIGIN = 0x08000000, LENGTH = 768K
//!    COREDUMP_STORAGE_FLASH (rx) : ORIGIN = 0x080C0000, LENGTH = 128K
//!    BLE_FLASH (rx) : ORIGIN = 0x080E0000, LENGTH = 128K
//! }
//! __MemfaultCoreStorageStart = ORIGIN(COREDUMP_STORAGE_FLASH);
//! __MemfaultCoreStorageEnd = ORIGIN(COREDUMP_STORAGE_FLASH) + LENGTH(COREDUMP_STORAGE_FLASH);

#include "memfault/panics/coredump.h"
#include "memfault/ports/buffered_coredump_storage.h"
#include "memfault/ports/stm32cube/wb/flash.h"

#include <string.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/core.h"
#include "memfault/panics/platform/coredump.h"
#include "memfault/core/debug_log.h"

#include "stm32wbxx_hal.h"

#ifndef MEMFAULT_COREDUMP_STORAGE_START_ADDR
extern uint32_t __MemfaultCoreStorageStart[];
#define MEMFAULT_COREDUMP_STORAGE_START_ADDR ((uint32_t)__MemfaultCoreStorageStart)
#endif

#ifndef MEMFAULT_COREDUMP_STORAGE_END_ADDR
extern uint32_t __MemfaultCoreStorageEnd[];
#define MEMFAULT_COREDUMP_STORAGE_END_ADDR ((uint32_t)__MemfaultCoreStorageEnd)
#endif


bool memfault_stm32cubewb_flash_clear_ecc_errors(
    uint32_t start_addr, uint32_t end_addr, uint32_t *corrupted_address) {
  const bool eccd_error = __HAL_FLASH_GET_FLAG(FLASH_FLAG_ECCD);
  if (!eccd_error) {
    if (corrupted_address != NULL) {
      *corrupted_address = 0; // no error found
    }
    return 0;
  }

  const uint32_t eccr = FLASH->ECCR;
  uint32_t corrupted_flash_address =
      FLASH_BASE + ((eccr & FLASH_ECCR_ADDR_ECC) >> FLASH_ECCR_ADDR_ECC_Pos);


  if (corrupted_address != NULL) {
      *corrupted_address = corrupted_flash_address;
  }

  if ((corrupted_flash_address < start_addr) ||
      (corrupted_flash_address >= end_addr)) {
    // There is a ECC error but it is in a range we do not want to zero out
    return -1;
  }

  // When a multi bit ECCD error is detected, it can be cleared by programming the corrupted
  // address to 0x0.

  uint32_t res;
  HAL_FLASH_Unlock();
  {
      uint64_t clear_error = 0;
      res = HAL_FLASH_Program(
          FLASH_TYPEPROGRAM_DOUBLEWORD, corrupted_flash_address, clear_error);
  }
  HAL_FLASH_Lock();
  return res == HAL_OK ? 0 : res;
}

void memfault_platform_fault_handler(const sMfltRegState *regs, eMemfaultRebootReason reason) {
  memfault_stm32cubewb_flash_clear_ecc_errors(
      MEMFAULT_COREDUMP_STORAGE_START_ADDR, MEMFAULT_COREDUMP_STORAGE_END_ADDR, NULL);
}


// Error writing to flash - should never happen & likely detects a configuration error.
// Call the reboot handler which will halt the device if a debugger is attached and then reboot.
MEMFAULT_NO_OPT
static void prv_coredump_writer_assert_and_reboot(int error_code) {
  memfault_platform_halt_if_debugging();
  memfault_platform_reboot();
}

static bool prv_op_within_storage_bounds(uint32_t offset, size_t data_len) {
  sMfltCoredumpStorageInfo info = { 0 };
  memfault_platform_coredump_storage_get_info(&info);
  return (offset + data_len) <= info.size;
}

void memfault_platform_coredump_storage_clear(void) {
  HAL_FLASH_Unlock();
  {
    const uint64_t clear_val = 0x0;
    const uint32_t res = HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_DOUBLEWORD, MEMFAULT_COREDUMP_STORAGE_START_ADDR, clear_val);

    if (res != HAL_OK) {
      MEMFAULT_LOG_ERROR("Could not clear coredump storage, 0x%" PRIx32, res);
    }
  }
  HAL_FLASH_Lock();
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
  const uint32_t start_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR;
  const uint32_t addr = start_addr + blk->write_offset;

  HAL_FLASH_Unlock();
  {
    uint64_t data;
    for (size_t i = 0; i < MEMFAULT_COREDUMP_STORAGE_WRITE_SIZE; i += sizeof(data)) {
      memcpy(&data, &blk->data[i], sizeof(data));

      const uint32_t res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, data);

      if (res != HAL_OK) {
        prv_coredump_writer_assert_and_reboot(res);
      }
    }
  }
  HAL_FLASH_Lock();

  *blk = (sCoredumpWorkingBuffer){ 0 };
  return true;
}

bool memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                             size_t read_len) {
  if (!prv_op_within_storage_bounds(offset, read_len)) {
    return false;
  }

  // The internal flash is memory mapped so we can just use memcpy!
  const uint32_t start_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR;
  memcpy(data, (void *)(start_addr + offset), read_len);
  return true;
}

static bool prv_erase_pages(uint32_t Page, uint32_t NbPages) {
  FLASH_EraseInitTypeDef s_erase_cfg = {
    .TypeErase = FLASH_TYPEERASE_PAGES,
    .Page = Page,
    .NbPages = NbPages,
  };
  uint32_t SectorError = 0;
  HAL_FLASH_Unlock();
  {
    const int res = HAL_FLASHEx_Erase(&s_erase_cfg, &SectorError);
    if (res != HAL_OK) {
      prv_coredump_writer_assert_and_reboot(res);
    }
  }
  HAL_FLASH_Lock();
  return true;
}

bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  if (!prv_op_within_storage_bounds(offset, erase_size)) {
    return false;
  }

  const size_t flash_end_addr = (FLASH_END_ADDR + 1);

  // check that address is in the range of flash
  const uint32_t erase_begin_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR + offset;
  const uint32_t end_addr = erase_begin_addr + erase_size;
  if (erase_begin_addr < FLASH_BASE || erase_begin_addr >= flash_end_addr) {
    return false;
  }

  // make sure region is aligned along page boundaries and
  // is whole page units in size
  if (((erase_begin_addr + offset) % FLASH_PAGE_SIZE) != 0 ||
      (erase_size % FLASH_PAGE_SIZE) != 0) {
    return false;
  }

  const size_t page_start = (erase_begin_addr - FLASH_BASE) / FLASH_PAGE_SIZE;
  const size_t num_pages = (end_addr - erase_begin_addr) / FLASH_PAGE_SIZE;

  if (!prv_erase_pages(page_start,  num_pages)) {
    return false;
  }

  return true;
}
