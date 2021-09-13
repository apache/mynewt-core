//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief

#include "memfault/panics/platform/coredump.h"
#include "memfault/panics/arch/arm/cortex_m.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "memfault/core/compiler.h"
#include "memfault/core/math.h"

MEMFAULT_PUT_IN_SECTION(".noinit.mflt_coredump") MEMFAULT_ALIGNED(8)
static uint8_t s_ram_backed_coredump_region[CONFIG_MEMFAULT_RAM_BACKED_COREDUMP_SIZE];

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  *info = (sMfltCoredumpStorageInfo) {
    .size = sizeof(s_ram_backed_coredump_region),
    .sector_size = sizeof(s_ram_backed_coredump_region),
  };
}

bool memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                             size_t read_len) {
  if ((offset + read_len) > sizeof(s_ram_backed_coredump_region)) {
    return false;
  }

  const uint8_t *read_ptr = &s_ram_backed_coredump_region[offset];
  memcpy(data, read_ptr, read_len);
  return true;
}


bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  if ((offset + erase_size) > sizeof(s_ram_backed_coredump_region)) {
    return false;
  }

  uint8_t *erase_ptr = &s_ram_backed_coredump_region[offset];
  memset(erase_ptr, 0x0, erase_size);
  return true;
}

bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                              size_t data_len) {
  if ((offset + data_len) > sizeof(s_ram_backed_coredump_region)) {
    return false;
  }

  uint8_t *write_ptr = &s_ram_backed_coredump_region[offset];
  memcpy(write_ptr, data, data_len);
  return true;
}

void memfault_platform_coredump_storage_clear(void) {
  const uint8_t clear_byte = 0x0;
  memfault_platform_coredump_storage_write(0, &clear_byte, sizeof(clear_byte));
}
