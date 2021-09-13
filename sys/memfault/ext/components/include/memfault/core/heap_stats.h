#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//!
//! A minimal set of functions to track statistics on heap allocations.
//!
//! @note To integrate with your system heap, add the intrumentation functions
//! to your platforms malloc + free implementations.
//!
//! For example, when using malloc & free from a library with GCC or Clang, you can use the
//! compiler flag `-Wl,--wrap=malloc,--wrap=free` to shim around the library malloc- in that case,
//! these functions should be added to your platform port:
//!
//! #include memfault/components.h
//!
//! extern void *__real_malloc(size_t size);
//! extern void __real_free(void *ptr);
//!
//! void *__wrap_malloc(size_t size) {
//!   void *ptr = __real_malloc(size);
//!   MEMFAULT_HEAP_STATS_MALLOC(ptr, size);
//!   return ptr;
//! }
//!
//! void __wrap_free(void *ptr) {
//!   MEMFAULT_HEAP_STATS_FREE(ptr);
//!   __real_free(ptr);
//! }
//!
//! @note By default, the thread-safety of the module depends on memfault_lock/unlock() API.
//! If calls to the malloc/free stats functions can be made from multiple tasks,
//! these APIs must be implemented. Locks are held while updating the internal
//! stats tracking data structures, which is quick and has a bounded worst-case
//! runtime.
//! @note If the functions you are calling MEMFAULT_HEAP_STATS_MALLOC/FREE from
//! already use a lock of their own, the use of memfault_lock/unlock can be disabled from
//! memfault_platform_config.h:
//!
//! #define MEMFAULT_COREDUMP_HEAP_STATS_LOCK_ENABLE 0


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memfault/core/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Record a single malloc. Called from within malloc handler (eg __wrap_malloc).
//!
//! @note This function usually should not be called directly, but instead via the macro
//! MEMFAULT_HEAP_STATS_MALLOC, which will retrieve the LR for the frame where malloc was called.
//!
//! @param lr Link register from the malloc function frame, stored for minimal context around this
//! @param ptr The pointer returned by the malloc call (can be NULL)
//! @param size The size value passed to the malloc call
//! allocation
void memfault_heap_stats_malloc(const void *lr, const void *ptr, size_t size);

#define MEMFAULT_HEAP_STATS_MALLOC(ptr_, size_)   \
  do {                                            \
    void *lr_;                                    \
    MEMFAULT_GET_LR(lr_);                         \
    memfault_heap_stats_malloc(lr_, ptr_, size_); \
  } while (0)

//! Record a single free. Called from within free handler (eg __wraps_free).
//!
//! @note A macro MEMFAULT_HEAP_STATS_FREE is provided for visual symmetry with
//! MEMFAULT_HEAP_STATS_MALLOC
//!
//! @param ptr The pointer being passed to free (can be NULL)
void memfault_heap_stats_free(const void *ptr);

#define MEMFAULT_HEAP_STATS_FREE(ptr_) memfault_heap_stats_free(ptr_)

#ifdef __cplusplus
}
#endif
