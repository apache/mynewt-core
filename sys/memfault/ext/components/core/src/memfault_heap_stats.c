//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Simple heap allocation tracking utility. Intended to shim into a system's
//! malloc/free implementation to track last allocations with callsite
//! information.

#include "memfault/core/heap_stats.h"
#include "memfault/core/heap_stats_impl.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/debug_log.h"
#include "memfault/core/platform/overrides.h"

#define MEMFAULT_HEAP_STATS_VERSION 1

sMfltHeapStats g_memfault_heap_stats = {
  .version = MEMFAULT_HEAP_STATS_VERSION,
};
sMfltHeapStatEntry g_memfault_heap_stats_pool[MEMFAULT_HEAP_STATS_MAX_COUNT];

static void prv_heap_stats_lock(void) {
#if MEMFAULT_COREDUMP_HEAP_STATS_LOCK_ENABLE
  memfault_lock();
#endif
}

static void prv_heap_stats_unlock(void) {
#if MEMFAULT_COREDUMP_HEAP_STATS_LOCK_ENABLE
  memfault_unlock();
#endif
}

void memfault_heap_stats_reset(void) {
  prv_heap_stats_lock();
  g_memfault_heap_stats = (sMfltHeapStats){0};
  memset(g_memfault_heap_stats_pool, 0, sizeof(g_memfault_heap_stats_pool));
  prv_heap_stats_unlock();
}

bool memfault_heap_stats_empty(void) {
  // if the first entry has a zero size, we know there was no entry ever
  // populated
  return g_memfault_heap_stats_pool[0].info.size == 0;
}

//! Return the next slot to write
static sMfltHeapStatEntry *prv_get_next_entry(void) {
  sMfltHeapStatEntry *slot =
    &g_memfault_heap_stats_pool[g_memfault_heap_stats.stats_pool_head];
  g_memfault_heap_stats.stats_pool_head = (g_memfault_heap_stats.stats_pool_head + 1) %
                                          MEMFAULT_ARRAY_SIZE(g_memfault_heap_stats_pool);
  return slot;
}

void memfault_heap_stats_malloc(const void *lr, const void *ptr, size_t size) {
  prv_heap_stats_lock();

  if (ptr) {
    g_memfault_heap_stats.in_use_block_count++;
    if (g_memfault_heap_stats.in_use_block_count > g_memfault_heap_stats.max_in_use_block_count) {
      g_memfault_heap_stats.max_in_use_block_count = g_memfault_heap_stats.in_use_block_count;
    }
    sMfltHeapStatEntry *slot = prv_get_next_entry();
    *slot = (sMfltHeapStatEntry){
      .lr = lr,
      .ptr = ptr,
      .info =
        {
          .size = size & (~(1u << 31)),
          .in_use = 1u,
        },
    };
  }

  prv_heap_stats_unlock();
}

void memfault_heap_stats_free(const void *ptr) {
  prv_heap_stats_lock();
  if (ptr) {
    g_memfault_heap_stats.in_use_block_count--;

    // if the pointer exists in the tracked stats, mark it as freed
    for (size_t i = 0; i < MEMFAULT_ARRAY_SIZE(g_memfault_heap_stats_pool); i++) {
      if (g_memfault_heap_stats_pool[i].ptr == ptr) {
        g_memfault_heap_stats_pool[i].info.in_use = 0;
        break;
      }
    }
  }
  prv_heap_stats_unlock();
}
