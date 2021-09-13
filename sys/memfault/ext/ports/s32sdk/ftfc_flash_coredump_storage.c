//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Reference implementation of platform dependency functions to use sectors of internal flash
//! on the S32K1xx family for coredump capture. The implementation makes use of the
//! Flash Memory Module (FTFC) peripheral.
//!
//! To use, update your linker script (.ld file) to expose information about the location to use.
//!
//! For example, using 8kB (4 sectors) of the S32K144 FlexNVM would
//! look something like this:
//!
//! MEMORY
//! {
//!    /* ... other regions ... */
//!    m_flexnvm  (RW)  : ORIGIN = 0x10000000, LENGTH = 8K
//! }
//! __MemfaultCoreStorageStart = ORIGIN(m_flexnvm);
//! __MemfaultCoreStorageEnd = ORIGIN(m_flexnvm) + LENGTH(m_flexnvm);
//!
//! Notes:
//!  - S32K1xx has program flash (PF) and data flash (DF). We recommend using the
//!    dataflash region (FlexNVM) to store coredump information but both can be written
//!    to with this port
//!  - __MemfaultCoreStorageStart & __MemfaultCoreStorageEnd must be aligned on sector
//!    boundaries

#include "memfault/panics/coredump.h"
#include "memfault/ports/buffered_coredump_storage.h"

#include <string.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/core.h"
#include "memfault/panics/platform/coredump.h"

#include "device_registers.h"

#ifndef MEMFAULT_COREDUMP_STORAGE_START_ADDR
extern uint32_t __MemfaultCoreStorageStart[];
#define MEMFAULT_COREDUMP_STORAGE_START_ADDR ((uint32_t)__MemfaultCoreStorageStart)
#endif

#ifndef MEMFAULT_COREDUMP_STORAGE_END_ADDR
extern uint32_t __MemfaultCoreStorageEnd[];
#define MEMFAULT_COREDUMP_STORAGE_END_ADDR ((uint32_t)__MemfaultCoreStorageEnd)
#endif

#define MEMFAULT_S32_PF_BASE 0x00000000U
#define MEMFAULT_S32_PF_END (MEMFAULT_S32_PF_BASE + FEATURE_FLS_PF_BLOCK_SIZE)

#define MEMFAULT_S32_DF_BASE 0x10000000U
#define MEMFAULT_S32_DF_END (MEMFAULT_S32_DF_BASE + FEATURE_FLS_DF_BLOCK_SIZE)

#define MEMFAULT_S32_PROG_PHRASE_LEN 8

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

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  const size_t size =
      MEMFAULT_COREDUMP_STORAGE_END_ADDR - MEMFAULT_COREDUMP_STORAGE_START_ADDR;

  *info  = (sMfltCoredumpStorageInfo) {
    .size = size,
    // Two configurations:
    //  Program Flash: 2kB or 4kB depending on configuration
    //  FlexNVM: 2kB or 4kB depending on configuration
    .sector_size = FEATURE_FLS_PF_BLOCK_SECTOR_SIZE,
  };
}

static bool prv_lookup_flash_info(uint32_t start_addr, uint32_t end_addr,
                                 uint32_t *sector_size, uint32_t *flash_addr) {
  if ((start_addr >= MEMFAULT_S32_PF_BASE) && (end_addr <= MEMFAULT_S32_PF_END)) {
    *sector_size = FEATURE_FLS_PF_BLOCK_SECTOR_SIZE;
    *flash_addr = start_addr;
  } else if ((start_addr >= MEMFAULT_S32_DF_BASE) && (end_addr <= MEMFAULT_S32_DF_END)) {
    // FlexNVM is memory mapped to start at 0x10000000 (MEMFAULT_S32_DF_BASE) but the flash
    // address base used to program that range is 0x800000U. We convert to the correct program
    // address below
    *sector_size = FEATURE_FLS_DF_BLOCK_SECTOR_SIZE;
    *flash_addr =  start_addr + 0x800000U - MEMFAULT_S32_DF_BASE;
  } else {
    // not in a known flash range or spans across program and data flash which is unsupported
    return false;
  }
  return true;
}

#define MEMFAULT_FSTAT_ERR_MASK \
  (FTFC_FSTAT_MGSTAT0(1) | FTFC_FSTAT_FPVIOL(1) | FTFC_FSTAT_ACCERR(1) \
   | FTFC_FSTAT_RDCOLERR(1))

static void prv_flash_wait_for_ready(void) {
  // wait for any outstanding flash operation to complete
  while ((FTFC->FSTAT & FTFC_FSTAT_CCIF(1)) == 0) {}
}

static void prv_flash_clear_errors(void) {
  FTFC->FSTAT = MEMFAULT_FSTAT_ERR_MASK;
}

static void prv_flash_start_cmd(void) {
  FTFC->FSTAT |= FTFC_FSTAT_CCIF(1);
}

static bool prv_erase_sector(uint32_t flash_address) {
  prv_flash_wait_for_ready();
  prv_flash_clear_errors();

  FTFC->FCCOB[3] = 0x09; // "Flash Erase Command" - See 37.5.8.2 Flash commands of S32K-RM
  FTFC->FCCOB[2] = (flash_address >> 16) & 0xff;
  FTFC->FCCOB[1] = (flash_address >> 8) & 0xff;
  FTFC->FCCOB[0] = flash_address & 0xff;

  // launch the command
  prv_flash_start_cmd();

  // wait for it to complete
  prv_flash_wait_for_ready();

  return ((FTFC->FSTAT & MEMFAULT_FSTAT_ERR_MASK) == 0);
}

static void prv_erase_sector_assert_success(uint32_t flash_address) {
  if (!prv_erase_sector(flash_address)) {
    prv_coredump_writer_assert_and_reboot(FTFC->FSTAT);
  }
}

static void prv_write_double_word(uint32_t flash_address, uint8_t data[MEMFAULT_S32_PROG_PHRASE_LEN]) {
  prv_flash_wait_for_ready();
  prv_flash_clear_errors();

  FTFC->FCCOB[3] = 0x07; // "Program Phrase" Command - See 37.5.8.2 Flash commands of S32K-RM
  FTFC->FCCOB[2] = (flash_address >> 16) & 0xff;
  FTFC->FCCOB[1] = (flash_address >> 8) & 0xff;
  FTFC->FCCOB[0] = flash_address & 0xff;

  for (size_t i = 0; i < MEMFAULT_S32_PROG_PHRASE_LEN ; i++) {
    FTFC->FCCOB[4 + i] = data[i];
  }

  // launch the command
  prv_flash_start_cmd();

  // wait for it to complete
  prv_flash_wait_for_ready();

  // we are saving a coredump and the write has failed, so just reboot the device since
  // we were going to reboot anyway
  if ((FTFC->FSTAT & MEMFAULT_FSTAT_ERR_MASK) != 0) {
    prv_coredump_writer_assert_and_reboot(FTFC->FSTAT);
  }
}

bool memfault_platform_coredump_storage_buffered_write(sCoredumpWorkingBuffer *blk) {
  const uint32_t start_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR + blk->write_offset;
  const uint32_t end_addr = start_addr + blk->write_offset + sizeof(blk->data);

  uint32_t flash_address;
  uint32_t sector_size;
  if (!prv_lookup_flash_info(start_addr, end_addr, &sector_size, &flash_address)) {
    return false;
  }

  const size_t block_size = sizeof(blk->data);

  if ((block_size % MEMFAULT_S32_PROG_PHRASE_LEN) != 0) {
    // configuration error
    prv_coredump_writer_assert_and_reboot(block_size);
  }

  for (size_t i = 0; i < block_size; i += MEMFAULT_S32_PROG_PHRASE_LEN) {
    prv_write_double_word(flash_address + i, &blk->data[i]);
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

  const uint32_t start_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR + offset;
  const uint32_t end_addr = start_addr + erase_size;

  uint32_t sector_size = 0;
  uint32_t flash_address = 0;
  if (!prv_lookup_flash_info(start_addr, end_addr, &sector_size, &flash_address)) {
    return false;
  }

  if (((start_addr + offset) % erase_size) != 0 ||
      (erase_size % sector_size) != 0) {
    return false;
  }

  for (size_t offset = 0; offset < erase_size; offset += sector_size) {
    prv_erase_sector_assert_success(flash_address + offset);
  }

  return true;
}

void memfault_platform_coredump_storage_clear(void) {
  const uint32_t start_addr = MEMFAULT_COREDUMP_STORAGE_START_ADDR;
  const uint32_t end_addr = MEMFAULT_COREDUMP_STORAGE_END_ADDR;

  uint32_t sector_size = 0;
  uint32_t flash_address = 0;
  if (!prv_lookup_flash_info(start_addr, end_addr, &sector_size, &flash_address)) {
    return;
  }

  if (!prv_erase_sector(flash_address)) {
    MEMFAULT_LOG_ERROR("Failed to clear coredump storage, 0x%" PRIx32, (uint32_t)FTFC->FSTAT);
  }
}
