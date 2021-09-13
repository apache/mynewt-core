#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Internal helper functions that are used when serializing Memfault Event based data
//! A user of the sdk should never have to call these routines directly.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memfault/core/event_storage.h"
#include "memfault/core/platform/system_time.h"
#include "memfault/core/serializer_key_ids.h"
#include "memfault/util/cbor.h"

#ifdef __cplusplus
extern "C" {
#endif

bool memfault_serializer_helper_encode_metadata_with_time(sMemfaultCborEncoder *encoder,
                                                          eMemfaultEventType type,
                                                          const sMemfaultCurrentTime *time);

  bool memfault_serializer_helper_encode_metadata(sMemfaultCborEncoder *encoder, eMemfaultEventType type);

bool memfault_serializer_helper_encode_uint32_kv_pair(
    sMemfaultCborEncoder *encoder, uint32_t key, uint32_t value);

bool memfault_serializer_helper_encode_int32_kv_pair(
    sMemfaultCborEncoder *encoder, uint32_t key, int32_t value);

bool memfault_serializer_helper_encode_byte_string_kv_pair(
  sMemfaultCborEncoder *encoder, uint32_t key, const void *buf, size_t buf_len);

typedef struct MemfaultTraceEventHelperInfo {
  eMemfaultTraceInfoEventKey reason_key;
  uint32_t reason_value;
  uint32_t pc;
  uint32_t lr;
  size_t extra_event_info_pairs;
} sMemfaultTraceEventHelperInfo;

bool memfault_serializer_helper_encode_trace_event(sMemfaultCborEncoder *e, const sMemfaultTraceEventHelperInfo *info);

//! @return false if encoding was not successful and the write session needs to be rolled back.
typedef bool (MemfaultSerializerHelperEncodeCallback)(sMemfaultCborEncoder *encoder, void *ctx);

//! Helper to initialize a CBOR encoder, prepare the storage for writing, call the encoder_callback
//! to encode and write any data and finally commit the write to the storage (or rollback in case
//! of an error).
//! @return the value returned from encode_callback
bool memfault_serializer_helper_encode_to_storage(sMemfaultCborEncoder *encoder,
    const sMemfaultEventStorageImpl *storage_impl,
    MemfaultSerializerHelperEncodeCallback encode_callback, void *ctx);

//! Helper to compute the size of encoding operations performed by encode_callback.
//! @return the computed size required to store the encoded data.
size_t memfault_serializer_helper_compute_size(
    sMemfaultCborEncoder *encoder, MemfaultSerializerHelperEncodeCallback encode_callback, void *ctx);

bool memfault_serializer_helper_check_storage_size(
    const sMemfaultEventStorageImpl *storage_impl, size_t (compute_worst_case_size)(void), const char *event_type);

//! Return the number of events that were dropped since last call
//!
//! @note Calling this function resets the counters.
uint32_t memfault_serializer_helper_read_drop_count(void);

#ifdef __cplusplus
}
#endif
