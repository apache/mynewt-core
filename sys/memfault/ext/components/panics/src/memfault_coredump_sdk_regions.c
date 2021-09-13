//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Enables collection of memory regions used by the Memfault SDK based on compile time defines.
//!
//! This is intended for users that are collecting select RAM regions in a coredump
//! (instead of all of RAM).
//!
//! Regions to collect are controlled by the following compile time defines:
//!
//!  MEMFAULT_COREDUMP_COLLECT_LOG_REGIONS=1
//!   - Collects the regions used by the Memfault logging APIs. When enabled,
//!     any logs collected will be included in the coredump. More details
//!     can be found at https://mflt.io/logging
//!  MEMFAULT_COREDUMP_COLLECT_HEAP_STATS=1
//!   - Collects the regions used by the Memfault heap stat APIs. When enabled,
//!     statistics around heap allocations will be included in the coredump to facilitate
//!     debug

#include "memfault/panics/coredump_impl.h"

#include "memfault/config.h"
#include "memfault/core/heap_stats_impl.h"
#include "memfault/core/log_impl.h"

//! Worst case number of regions that can be collected by the sdk
#define MEMFAULT_TOTAL_SDK_MEMORY_REGIONS                  \
  (MEMFAULT_LOG_NUM_RAM_REGIONS /* log ram region count */ \
   + MEMFAULT_HEAP_STATS_NUM_RAM_REGIONS /* heap stats regions */)

const sMfltCoredumpRegion *memfault_coredump_get_sdk_regions(size_t *num_regions) {
  static sMfltCoredumpRegion s_sdk_coredump_regions[MEMFAULT_TOTAL_SDK_MEMORY_REGIONS];

  size_t total_regions = 0;

#if MEMFAULT_COREDUMP_COLLECT_LOG_REGIONS
  sMemfaultLogRegions regions = { 0 };
  if (memfault_log_get_regions(&regions)) {
    for (size_t i = 0; i < MEMFAULT_LOG_NUM_RAM_REGIONS; i++) {
      sMemfaultLogMemoryRegion *log_region = &regions.region[i];
      s_sdk_coredump_regions[total_regions] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
          log_region->region_start, log_region->region_size);
      total_regions++;
    }
  }
#endif

#if MEMFAULT_COREDUMP_COLLECT_HEAP_STATS
  if (!memfault_heap_stats_empty()) {
    s_sdk_coredump_regions[total_regions] =
      MEMFAULT_COREDUMP_MEMORY_REGION_INIT(&g_memfault_heap_stats, sizeof(g_memfault_heap_stats));
    total_regions++;
    s_sdk_coredump_regions[total_regions] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
      &g_memfault_heap_stats_pool, sizeof(g_memfault_heap_stats_pool));
    total_regions++;
  }
#endif

  *num_regions = total_regions;
  return total_regions != 0 ? &s_sdk_coredump_regions[0] : NULL;
}
