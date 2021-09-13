#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Internals used by the "coredump" subsystem in the "panics" component
//! An end user should _never_ call any of these APIs directly.

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/platform/coredump.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Coredump block types
typedef enum MfltCoredumpBlockType  {
  kMfltCoredumpBlockType_CurrentRegisters = 0,
  kMfltCoredumpBlockType_MemoryRegion = 1,
  kMfltCoredumpRegionType_DeviceSerial = 2,
  // Deprecated: kMfltCoredumpRegionType_FirmwareVersion = 3,
  kMfltCoredumpRegionType_HardwareVersion = 4,
  kMfltCoredumpRegionType_TraceReason = 5,
  kMfltCoredumpRegionType_PaddingRegion = 6,
  kMfltCoredumpRegionType_MachineType = 7,
  kMfltCoredumpRegionType_VendorCoredumpEspIdfV2ToV3_1 = 8,
  kMfltCoredumpRegionType_ArmV6orV7Mpu = 9,
  kMfltCoredumpRegionType_SoftwareVersion = 10,
  kMfltCoredumpRegionType_SoftwareType = 11,
  kMfltCoredumpRegionType_BuildId = 12,
} eMfltCoredumpBlockType;

// All elements are in word-sized units for alignment-friendliness.
typedef struct MfltCachedBlock {
  uint32_t valid_cache;
  uint32_t cached_address;

  uint32_t blk_size;
  uint32_t blk[];
} sMfltCachedBlock;

// We'll point to a properly sized memory block of type MfltCachedBlock.
#define MEMFAULT_CACHE_BLOCK_SIZE_WORDS(blk_size) \
  ((sizeof(sMfltCachedBlock) + blk_size) / sizeof(uint32_t))

//! Computes the amount of space that will be required to save a coredump
//!
//! @param save_info The platform specific information to save as part of the coredump
//! @return The space required to save the coredump or 0 on error
size_t memfault_coredump_get_save_size(const sMemfaultCoredumpSaveInfo *save_info);

//! @param num_regions The number of regions in the list returned
//! @return regions to collect based on the active architecture or NULL if there are no extra
//! regions to collect
const sMfltCoredumpRegion *memfault_coredump_get_arch_regions(size_t *num_regions);

//! Memory regions that are part of the SDK to include in a coredump
//!
//! @param num_regions The number of regions in the list returned
//! @return memory regions with the SDK to collect or NULL if there are no extra
//!   regions to collect
const sMfltCoredumpRegion *memfault_coredump_get_sdk_regions(size_t *num_regions);


#ifdef __cplusplus
}
#endif
