//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Reference implementation of platform dependency functions to use a sector of internal flash
//! for coredump capture
//!
//! To use, update your linker script (.ld file) to expose information about the location to use.
//! For example, using the last sector of the STM32F429I (2MB dual banked flash) would look
//! something like this:
//!
//! MEMORY
//! {
//!    /* ... other regions ... */
//!    COREDUMP_STORAGE_FLASH (rx) : ORIGIN = 0x81E0000, LENGTH = 128K
//! }
//! __MemfaultCoreStorageStart = ORIGIN(COREDUMP_STORAGE_FLASH);
//! __MemfaultCoreStorageEnd = ORIGIN(COREDUMP_STORAGE_FLASH) + LENGTH(COREDUMP_STORAGE_FLASH);
//! __MemfaultCoreStorageSectorNumber = 23;

#include "memfault/panics/coredump.h"

#include <string.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/core.h"
#include "memfault/panics/platform/coredump.h"

#include "stm32f4xx_hal.h"

#ifndef MEMFAULT_COREDUMP_STORAGE_START_ADDR
extern uint32_t __MemfaultCoreStorageStart[];
#define MEMFAULT_COREDUMP_STORAGE_START_ADDR ((uint32_t)__MemfaultCoreStorageStart)
#endif

#ifndef MEMFAULT_COREDUMP_STORAGE_END_ADDR
extern uint32_t __MemfaultCoreStorageEnd[];
#define MEMFAULT_COREDUMP_STORAGE_END_ADDR ((uint32_t)__MemfaultCoreStorageEnd)
#endif

#ifndef MEMFAULT_COREDUMP_STORAGE_FLASH_SECTOR_NUMBER
//! The sector number to write to. This ID can be found in the "Embedded Flash memory"
//! section of the reference manual for the STM32 family
extern uint32_t __MemfaultCoreStorageSectorNumber[];
#define MEMFAULT_COREDUMP_STORAGE_FLASH_SECTOR_NUMBER ((uint32_t)__MemfaultCoreStorageSectorNumber)
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
  const uint32_t data = 0x0;
  memfault_platform_coredump_storage_write(0, &data, sizeof(data));
}

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  const size_t size =
      MEMFAULT_COREDUMP_STORAGE_END_ADDR - MEMFAULT_COREDUMP_STORAGE_START_ADDR;

  *info  = (sMfltCoredumpStorageInfo) {
    .size = size,
    .sector_size = size,
  };
}

bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                              size_t data_len) {
  if (!prv_op_within_flash_bounds(offset, data_len)) {
    return false;
  }

  const uint32_t start_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR + offset;
  HAL_FLASH_Unlock();
  {
    const uint8_t *datap = data;
    for (size_t i = 0; i < data_len; i++) {
      const uint32_t res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, start_addr + i, datap[i]);
      if (res != HAL_OK) {
        prv_coredump_writer_assert_and_reboot(res);
      }
    }
  }
  HAL_FLASH_Lock();
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

  FLASH_EraseInitTypeDef s_erase_cfg = {
    .TypeErase = FLASH_TYPEERASE_SECTORS,
    // Only needs to be provided for Mass Erase
    .Banks = 0,
    .Sector = MEMFAULT_COREDUMP_STORAGE_FLASH_SECTOR_NUMBER,
    .NbSectors = 1,

    // See "Program/erase parallelism" in TRM. By using the lowest parallelism
    // the driver will work over the entire voltage range supported by the MCU
    // (1.8 - 3.6V). Exact time ranges for sector erases can be found in the
    // "Flash memory programming" section of the device datasheet
    .VoltageRange = FLASH_VOLTAGE_RANGE_1
  };
  uint32_t SectorError = 0;
  HAL_FLASH_Unlock();
  {
    int res = HAL_FLASHEx_Erase(&s_erase_cfg, &SectorError);
    if (res != HAL_OK) {
      prv_coredump_writer_assert_and_reboot(res);
    }
  }
  HAL_FLASH_Lock();

  return true;
}
