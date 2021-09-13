#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Utilities to assist with querying the log buffer
//!
//! @note A user of the Memfault SDK should _never_ call any
//! of these routines directly

#include <stdbool.h>
#include <stdint.h>

#include "memfault/core/compiler.h"
#include "memfault/core/platform/debug_log.h"

#ifdef __cplusplus
extern "C" {
#endif

// Note: We do not use bitfields here to avoid portability complications on the decode side since
// alignment of bitfields as well as the order of bitfields is left up to the compiler per the C
// standard.
//
// Header Layout:
// 0brsxx.tlll
// where
//  r = read (1 if the message has been read, 0 otherwise)
//  s = sent (1 if the message has been sent, 0 otherwise)
//  x = rsvd
//  t = type (0 = formatted log)
//  l = log level (eMemfaultPlatformLogLevel)

#define MEMFAULT_LOG_HDR_LEVEL_POS  0
#define MEMFAULT_LOG_HDR_LEVEL_MASK 0x07u
#define MEMFAULT_LOG_HDR_TYPE_POS   3u
#define MEMFAULT_LOG_HDR_TYPE_MASK  0x08u
#define MEMFAULT_LOG_HDR_READ_MASK  0x80u  // Log has been read through memfault_log_read()
#define MEMFAULT_LOG_HDR_SENT_MASK  0x40u  // Log has been sent through g_memfault_log_data_source

static inline eMemfaultPlatformLogLevel memfault_log_get_level_from_hdr(uint8_t hdr) {
  return (eMemfaultPlatformLogLevel)((hdr & MEMFAULT_LOG_HDR_LEVEL_MASK) >> MEMFAULT_LOG_HDR_LEVEL_POS);
}

typedef MEMFAULT_PACKED_STRUCT {
  // data about the message stored (details below)
  uint8_t hdr;
  // the length of the msg
  uint8_t len;
  // underlying message
  uint8_t msg[];
} sMfltRamLogEntry;

typedef struct {
  uint32_t read_offset;
  void *user_ctx;
  sMfltRamLogEntry entry;
} sMfltLogIterator;

//! The callback invoked when "memfault_log_iterate" is called
//!
//! @param ctx The context provided to "memfault_log_iterate"
//! @param iter The iterator originally passed to "memfault_log_iterate". The
//! iter->entry field gets updated before entering this callback. The
//! iter->read_offset field gets updated after exiting this callback.
//!
//! @return bool to continue iterating, else false
typedef bool (*MemfaultLogIteratorCallback)(sMfltLogIterator *iter);

//! Iterates over the logs in the buffer, calling the callback for each log.
void memfault_log_iterate(MemfaultLogIteratorCallback callback, sMfltLogIterator *iter);

//! Update/rewrite the entry header at the position of the iterator.
//! @note This MUST ONLY be called from a memfault_log_iterate() callback (it
//! assumes memfault_lock has been taken by the caller).
bool memfault_log_iter_update_entry(sMfltLogIterator *iter);

typedef bool (* MemfaultLogMsgCopyCallback)(sMfltLogIterator *iter, size_t offset,
                                            const char *buf, size_t buf_len);

//! @note This MUST ONLY be called from a memfault_log_iterate() callback (it
//! assumes memfault_lock has been taken by the caller).
bool memfault_log_iter_copy_msg(sMfltLogIterator *iter, MemfaultLogMsgCopyCallback callback);

#ifdef __cplusplus
}
#endif
