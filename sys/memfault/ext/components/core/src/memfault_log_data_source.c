//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!

#include "memfault/config.h"

#if MEMFAULT_LOG_DATA_SOURCE_ENABLED

#include "memfault_log_data_source_private.h"
#include "memfault_log_private.h"

#include <stdint.h>
#include <string.h>

#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer_source.h"
#include "memfault/core/log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/overrides.h"
#include "memfault/core/platform/system_time.h"
#include "memfault/core/serializer_helper.h"
#include "memfault/core/serializer_key_ids.h"
#include "memfault/util/cbor.h"

typedef struct {
  bool triggered;
  size_t num_logs;
  sMemfaultCurrentTime trigger_time;
} sMfltLogDataSourceCtx;

static sMfltLogDataSourceCtx s_memfault_log_data_source_ctx;

static bool prv_log_is_sent(uint8_t hdr) {
  return hdr & MEMFAULT_LOG_HDR_SENT_MASK;
}

typedef struct {
  size_t num_logs;
} sMfltLogCountingCtx;

static bool prv_log_iterate_counting_callback(sMfltLogIterator *iter) {
  sMfltLogCountingCtx *const ctx = (sMfltLogCountingCtx *)(iter->user_ctx);
  if (!prv_log_is_sent(iter->entry.hdr)) {
    ++ctx->num_logs;
  }
  return true;
}

void memfault_log_trigger_collection(void) {
  if (s_memfault_log_data_source_ctx.triggered) {
    return;
  }

  sMfltLogCountingCtx ctx = { 0 };
  sMfltLogIterator iter = { .user_ctx = &ctx };
  memfault_log_iterate(prv_log_iterate_counting_callback, &iter);
  if (ctx.num_logs == 0) {
    return;
  }

  memfault_lock();
  {
    // Check again in the unlikely case this function was called concurrently:
    if (s_memfault_log_data_source_ctx.triggered) {
      memfault_unlock();
      return;
    }
    s_memfault_log_data_source_ctx.triggered = true;
    if (!memfault_platform_time_get_current(&s_memfault_log_data_source_ctx.trigger_time)) {
      s_memfault_log_data_source_ctx.trigger_time.type = kMemfaultCurrentTimeType_Unknown;
    }
    s_memfault_log_data_source_ctx.num_logs = ctx.num_logs;
  }
  memfault_unlock();
}

bool memfault_log_data_source_has_been_triggered(void) {
  // Note: memfault_lock() is held when this is called by memfault_log
  return s_memfault_log_data_source_ctx.triggered;
}

typedef struct {
  size_t num_logs;
  sMemfaultCurrentTime trigger_time;
  sMemfaultCborEncoder encoder;
  bool has_encoding_error;
  bool should_stop_encoding;
  union {
    size_t num_encoded_logs;
    size_t num_marked_sent_logs;
  };
} sMfltLogEncodingCtx;

static bool prv_copy_msg_callback(sMfltLogIterator *iter, MEMFAULT_UNUSED size_t offset,
                                  const char *buf, size_t buf_len) {
  sMfltLogEncodingCtx *const ctx = (sMfltLogEncodingCtx *)iter->user_ctx;
  return memfault_cbor_encode_string_add(&ctx->encoder, buf, buf_len);
}

static bool prv_encode_current_log(sMemfaultCborEncoder *encoder, sMfltLogIterator *iter) {
  return (
    memfault_cbor_encode_unsigned_integer(encoder, memfault_log_get_level_from_hdr(iter->entry.hdr)) &&
    memfault_cbor_encode_string_begin(encoder, iter->entry.len) &&
    memfault_log_iter_copy_msg(iter, prv_copy_msg_callback)
  );
}

static bool prv_log_iterate_encode_callback(sMfltLogIterator *iter) {
  sMfltLogEncodingCtx *const ctx = (sMfltLogEncodingCtx *)iter->user_ctx;
  if (ctx->should_stop_encoding) {
    return false;
  }
  if (!prv_log_is_sent(iter->entry.hdr)) {
    ctx->has_encoding_error |= !prv_encode_current_log(&ctx->encoder, iter);
    // It's possible more logs have been added to the buffer
    // after the memfault_log_data_source_has_been_triggered() call. They cannot be included,
    // because the total message size has already been communicated to the packetizer.
    if (++ctx->num_encoded_logs == ctx->num_logs) {
      return false;
    }
  }
  return true;
}

static bool prv_encode(sMemfaultCborEncoder *encoder, void *iter) {
  sMfltLogEncodingCtx *ctx = (sMfltLogEncodingCtx *)((sMfltLogIterator *)iter)->user_ctx;
  if (!memfault_serializer_helper_encode_metadata_with_time(
    encoder, kMemfaultEventType_Logs, &ctx->trigger_time)) {
    return false;
  }
  if (!memfault_cbor_encode_unsigned_integer(encoder, kMemfaultEventKey_EventInfo)) {
    return false;
  }
  // To save space, all logs are encoded into a single array (as opposed to using a map or
  // array per log):
  const size_t elements_per_log = 2;  // level, msg
  if (!memfault_cbor_encode_array_begin(encoder, elements_per_log * ctx->num_logs)) {
    return false;
  }
  memfault_log_iterate(prv_log_iterate_encode_callback, iter);
  return ctx->has_encoding_error;
}

static void prv_init_encoding_ctx(sMfltLogEncodingCtx *ctx) {
  *ctx = (sMfltLogEncodingCtx) {
    .num_logs = s_memfault_log_data_source_ctx.num_logs,
    .trigger_time = s_memfault_log_data_source_ctx.trigger_time,
  };
}

static bool prv_has_logs(size_t *total_size) {
  if (!s_memfault_log_data_source_ctx.triggered) {
    return false;
  }

  sMfltLogEncodingCtx ctx;
  prv_init_encoding_ctx(&ctx);

  sMfltLogIterator iter = {
    .read_offset = 0,
    .user_ctx = &ctx
  };

  *total_size = memfault_serializer_helper_compute_size(&ctx.encoder, prv_encode, &iter);
  return true;
}

typedef struct {
  uint32_t offset;
  uint8_t *buf;
  size_t buf_len;
  size_t data_source_bytes_written;
  sMfltLogEncodingCtx encoding_ctx;
} sMfltLogsDestCtx;

static void prv_encoder_callback(void *encoder_ctx,
                                 uint32_t src_offset, const void *src_buf, size_t src_buf_len) {
  sMfltLogsDestCtx *dest = (sMfltLogsDestCtx *)encoder_ctx;

  const size_t dest_end_offset = dest->offset + dest->buf_len;
  // Optimization: stop encoding if the encoder writes are past the destination buffer:
  if (src_offset > dest_end_offset) {
    dest->encoding_ctx.should_stop_encoding = true;
    return;
  }
  const size_t src_end_offset = src_offset + src_buf_len;
  const size_t intersection_start_offset = MEMFAULT_MAX(src_offset, dest->offset);
  const size_t intersection_end_offset = MEMFAULT_MIN(src_end_offset, dest_end_offset);
  if (intersection_end_offset <= intersection_start_offset) {
    return;  // no intersection
  }
  const size_t intersection_len = intersection_end_offset - intersection_start_offset;
  memcpy(dest->buf + (intersection_start_offset - dest->offset),
         ((const uint8_t *)src_buf) + (intersection_start_offset - src_offset),
         intersection_len);
  dest->data_source_bytes_written += intersection_len;
}

static bool prv_logs_read(uint32_t offset, void *buf, size_t buf_len) {
  sMfltLogsDestCtx dest_ctx = (sMfltLogsDestCtx) {
    .offset = offset,
    .buf = buf,
    .buf_len = buf_len,
  };

  sMfltLogIterator iter = {
    .user_ctx = &dest_ctx.encoding_ctx,
  };

  prv_init_encoding_ctx(&dest_ctx.encoding_ctx);
  // Note: UINT_MAX is passed as length, because it is possible and expected that the output is written
  // partially by the callback. The callback takes care of not overrunning the output buffer itself.
  memfault_cbor_encoder_init(&dest_ctx.encoding_ctx.encoder, prv_encoder_callback, &dest_ctx, UINT32_MAX);
  prv_encode(&dest_ctx.encoding_ctx.encoder, &iter);
  return buf_len == dest_ctx.data_source_bytes_written;
}

static bool prv_log_iterate_mark_sent_callback(sMfltLogIterator *iter) {
  sMfltLogEncodingCtx *const ctx = (sMfltLogEncodingCtx *)iter->user_ctx;
  if (!prv_log_is_sent(iter->entry.hdr)) {
    iter->entry.hdr |= MEMFAULT_LOG_HDR_SENT_MASK;
    memfault_log_iter_update_entry(iter);
    if (++ctx->num_marked_sent_logs == ctx->num_logs) {
      return false;
    }
  }
  return true;
}

static void prv_logs_mark_sent(void) {
  sMfltLogEncodingCtx ctx;

  sMfltLogIterator iter = {
    .read_offset = 0,
    .user_ctx = &ctx
  };

  prv_init_encoding_ctx(&ctx);
  memfault_log_iterate(prv_log_iterate_mark_sent_callback, &iter);

  memfault_lock();
  s_memfault_log_data_source_ctx = (sMfltLogDataSourceCtx) { 0 };
  memfault_unlock();
}

//! Expose a data source for use by the Memfault Packetizer
const sMemfaultDataSourceImpl g_memfault_log_data_source  = {
  .has_more_msgs_cb = prv_has_logs,
  .read_msg_cb = prv_logs_read,
  .mark_msg_read_cb = prv_logs_mark_sent,
};

void memfault_log_data_source_reset(void) {
  s_memfault_log_data_source_ctx = (sMfltLogDataSourceCtx) { 0 };
}

size_t memfault_log_data_source_count_unsent_logs(void) {
  sMfltLogCountingCtx ctx = { 0 };
  sMfltLogIterator iter = { .user_ctx = &ctx };
  memfault_log_iterate(prv_log_iterate_counting_callback, &iter);
  return ctx.num_logs;
}

#endif /* MEMFAULT_LOG_DATA_SOURCE_ENABLED */
