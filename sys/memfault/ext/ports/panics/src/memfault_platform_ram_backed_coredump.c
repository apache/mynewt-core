//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! A port for the platform dependencies needed to use the coredump feature from the "panics"
//! component by saving the Memfault coredump data in a "noinit" region of RAM.
//!
//! This can be linked in directly by adding the .c file to the build system or can be
//! copied into your repo and modified to collect different RAM regions.
//!
//! By default, it will collect the top of the stack which was running at the time of the
//! crash. This allows for a reasonable backtrace to be collected while using very little RAM.
//!
//! Place the "noinit" region in an area of RAM that will persist across bootup.
//!    The region must:
//!    - not be placed in .bss
//!    - not be an area of RAM used by any of your bootloaders
//!    For example, with GNU GCC, this can be achieved by adding something like the following to
//!    your linker script:
//!    MEMORY
//!    {
//!      [...]
//!      COREDUMP_NOINIT (rw) :  ORIGIN = <RAM_REGION_START>, LENGTH = 1024
//!    }
//!    SECTIONS
//!    {
//!      [...]
//!      .coredump_noinit (NOLOAD): { KEEP(*(*.noinit.mflt_coredump)) } > COREDUMP_NOINIT
//!    }

#include "memfault/config.h"

#if MEMFAULT_PLATFORM_COREDUMP_STORAGE_USE_RAM

#include "memfault/panics/platform/coredump.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "memfault/core/compiler.h"
#include "memfault/core/math.h"

#if !MEMFAULT_PLATFORM_COREDUMP_STORAGE_RAM_CUSTOM

#if ((MEMFAULT_PLATFORM_COREDUMP_STORAGE_RAM_SIZE % 4) != 0)
#error "MEMFAULT_PLATFORM_COREDUMP_STORAGE_RAM_SIZE must be a multiple of 4"
#endif

MEMFAULT_STATIC_ASSERT(sizeof(uint32_t) == 4, "port expects sizeof(uint32_t) == 4");

MEMFAULT_PUT_IN_SECTION(MEMFAULT_PLATFORM_COREDUMP_NOINIT_SECTION_NAME)
static uint32_t s_ram_backed_coredump_region[MEMFAULT_PLATFORM_COREDUMP_STORAGE_RAM_SIZE / 4];

#define MEMFAULT_PLATFORM_COREDUMP_RAM_START_ADDR ((uint8_t *)&s_ram_backed_coredump_region[0])

#endif /* MEMFAULT_PLATFORM_COREDUMP_STORAGE_RAM_CUSTOM */

#if !MEMFAULT_PLATFORM_COREDUMP_STORAGE_REGIONS_CUSTOM
//! Collect the active stack as part of the coredump capture.
//! User can implement their own version to override the implementation
MEMFAULT_WEAK
const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(
    const sCoredumpCrashInfo *crash_info, size_t *num_regions) {
   static sMfltCoredumpRegion s_coredump_regions[1];

   const size_t stack_size = memfault_platform_sanitize_address_range(
       crash_info->stack_address, MEMFAULT_PLATFORM_ACTIVE_STACK_SIZE_TO_COLLECT);

   s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
       crash_info->stack_address, stack_size);
   *num_regions = MEMFAULT_ARRAY_SIZE(s_coredump_regions);
   return &s_coredump_regions[0];
}
#endif

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  *info = (sMfltCoredumpStorageInfo) {
    .size = MEMFAULT_PLATFORM_COREDUMP_STORAGE_RAM_SIZE,
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

  const uint8_t *storage_ptr = MEMFAULT_PLATFORM_COREDUMP_RAM_START_ADDR;
  const uint8_t *read_ptr = &storage_ptr[offset];
  memcpy(data, read_ptr, read_len);
  return true;
}

bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  if (!prv_op_within_flash_bounds(offset, erase_size)) {
    return false;
  }

  uint8_t *storage_ptr = MEMFAULT_PLATFORM_COREDUMP_RAM_START_ADDR;
  void *erase_ptr = &storage_ptr[offset];
  memset(erase_ptr, 0x0, erase_size);
  return true;
}

bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                              size_t data_len) {
  if (!prv_op_within_flash_bounds(offset, data_len)) {
    return false;
  }

  uint8_t *storage_ptr = MEMFAULT_PLATFORM_COREDUMP_RAM_START_ADDR;
  uint8_t *write_ptr = (uint8_t *)&storage_ptr[offset];
  memcpy(write_ptr, data, data_len);
  return true;
}

void memfault_platform_coredump_storage_clear(void) {
  const uint8_t clear_byte = 0x0;
  memfault_platform_coredump_storage_write(0, &clear_byte, sizeof(clear_byte));
}

#endif /* MEMFAULT_PLATFORM_COREDUMP_STORAGE_USE_RAM */
