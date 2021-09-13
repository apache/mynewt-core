#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//!
//! Heap tracking APIs intended for use within the memfault-firmware-sdk

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memfault/config.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Number of regions used by heap stats
#define MEMFAULT_HEAP_STATS_NUM_RAM_REGIONS 2


//! The heap stats data structure, which is exported when capturing a core.
typedef struct MfltHeapStatEntry {
  // LR at time of allocation
  const void *lr;

  // The pointer returned by the actual allocation function
  const void *ptr;

  // Size of this allocation. 0 means the entry is invalid and should be ignored
  struct {
    // 31 bits to represent the size passed to malloc
    uint32_t size : 31;
    // 1 bit indicating whether this entry is still in use or has been freed
    uint32_t in_use : 1;
  } info;
} sMfltHeapStatEntry;


typedef struct MfltHeapStats {
  uint8_t version;

  // Number of blocks currently allocated and not freed
  uint32_t in_use_block_count;

  // Track the max value of 'in_use_block_count'
  uint32_t max_in_use_block_count;

  // Allocation entry list pointer
  size_t stats_pool_head;
} sMfltHeapStats;

extern sMfltHeapStats g_memfault_heap_stats;
extern sMfltHeapStatEntry g_memfault_heap_stats_pool[MEMFAULT_HEAP_STATS_MAX_COUNT];

//! Reset the tracked stats.
void memfault_heap_stats_reset(void);

//! Check if no heap stats have been collected
bool memfault_heap_stats_empty(void);

#ifdef __cplusplus
}
#endif
