//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/core/data_packetizer.h"

#include <string.h>
#include <inttypes.h>

#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer_source.h"
#include "memfault/core/data_source_rle.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/debug_log.h"
#include "memfault/util/chunk_transport.h"

MEMFAULT_STATIC_ASSERT(MEMFAULT_PACKETIZER_MIN_BUF_LEN == MEMFAULT_MIN_CHUNK_BUF_LEN,
                       "Minimum packetizer payload size must match underlying transport");
//
// Weak definitions which get overridden when the component that implements that data source is
// included and compiled in a project
//

static bool prv_data_source_has_event_stub(size_t *event_size) {
  *event_size = 0;
  return false;
}

static bool prv_data_source_read_stub(MEMFAULT_UNUSED uint32_t offset,
                                      MEMFAULT_UNUSED void *buf,
                                      MEMFAULT_UNUSED size_t buf_len) {
  return false;
}

static void prv_data_source_mark_event_read_stub(void) { }

MEMFAULT_WEAK const sMemfaultDataSourceImpl g_memfault_data_rle_source = {
  .has_more_msgs_cb = prv_data_source_has_event_stub,
  .read_msg_cb = prv_data_source_read_stub,
  .mark_msg_read_cb = prv_data_source_mark_event_read_stub,
};

MEMFAULT_WEAK const sMemfaultDataSourceImpl g_memfault_coredump_data_source = {
  .has_more_msgs_cb = prv_data_source_has_event_stub,
  .read_msg_cb = prv_data_source_read_stub,
  .mark_msg_read_cb = prv_data_source_mark_event_read_stub,
};

MEMFAULT_WEAK const sMemfaultDataSourceImpl g_memfault_event_data_source = {
  .has_more_msgs_cb = prv_data_source_has_event_stub,
  .read_msg_cb = prv_data_source_read_stub,
  .mark_msg_read_cb = prv_data_source_mark_event_read_stub,
};

MEMFAULT_WEAK const sMemfaultDataSourceImpl g_memfault_log_data_source = {
  .has_more_msgs_cb = prv_data_source_has_event_stub,
  .read_msg_cb = prv_data_source_read_stub,
  .mark_msg_read_cb = prv_data_source_mark_event_read_stub,
};

MEMFAULT_WEAK
bool memfault_data_source_rle_encoder_set_active(
    MEMFAULT_UNUSED const sMemfaultDataSourceImpl *active_source) {
  return false;
}

// NOTE: These values are used by the Memfault cloud chunks API
typedef enum {
  kMfltMessageType_None = 0,
  kMfltMessageType_Coredump = 1,
  kMfltMessageType_Event = 2,
  kMfltMessageType_Log = 3,
  kMfltMessageType_NumTypes
} eMfltMessageType;

//! Make sure our externally facing types match the internal ones
MEMFAULT_STATIC_ASSERT((1 << kMfltMessageType_Coredump) == kMfltDataSourceMask_Coredump,
                       "kMfltDataSourceMask_Coredump is incorrectly defined");
MEMFAULT_STATIC_ASSERT((1 << kMfltMessageType_Event) == kMfltDataSourceMask_Event,
                       "kMfltDataSourceMask_Event, is incorrectly defined");
MEMFAULT_STATIC_ASSERT((1 << kMfltMessageType_Log) == kMfltDataSourceMask_Log,
                       "kMfltDataSourceMask_Log is incorrectly defined");
MEMFAULT_STATIC_ASSERT(kMfltMessageType_NumTypes == 4, "eMfltDataSourceMask needs to be updated");


typedef struct MemfaultDataSource {
  eMfltMessageType type;
  bool use_rle;
  const sMemfaultDataSourceImpl *impl;
} sMemfaultDataSource;

static const sMemfaultDataSource s_memfault_data_source[] = {
  {
    .type = kMfltMessageType_Coredump,
    .use_rle = true,
    .impl = &g_memfault_coredump_data_source,
  },
  {
    .type = kMfltMessageType_Event,
    .use_rle = false,
    .impl = &g_memfault_event_data_source,
  },
  {
    .type = kMfltMessageType_Log,
    .use_rle = false,
    .impl = &g_memfault_log_data_source,
  }
};

typedef struct {
  size_t total_size;
  sMemfaultDataSource source;
} sMessageMetadata;

typedef struct {
  bool active_message;
  sMessageMetadata msg_metadata;
  sMfltChunkTransportCtx curr_msg_ctx;
} sMfltTransportState;

typedef MEMFAULT_PACKED_STRUCT {
  uint8_t mflt_msg_type; // eMfltMessageType
} sMfltPacketizerHdr;

static sMfltTransportState s_mflt_packetizer_state;

static uint32_t s_active_data_sources = kMfltDataSourceMask_All;

void memfault_packetizer_set_active_sources(uint32_t mask) {
  memfault_packetizer_abort();
  s_active_data_sources = mask;
}

static void prv_reset_packetizer_state(void) {
  s_mflt_packetizer_state = (sMfltTransportState) {
    .active_message = false,
  };

  memfault_data_source_rle_encoder_set_active(NULL);
}

static void prv_data_source_chunk_transport_msg_reader(uint32_t offset, void *buf,
                                                       size_t buf_len) {
  uint8_t *bufp = buf;
  size_t read_offset = 0;
  const size_t hdr_size = sizeof(sMfltPacketizerHdr);

  const sMessageMetadata *msg_metadata = &s_mflt_packetizer_state.msg_metadata;
  if (offset < hdr_size) {
    const uint8_t rle_enable_mask = 0x80;
    const uint8_t msg_type = (uint8_t)msg_metadata->source.type;

    sMfltPacketizerHdr hdr = {
      .mflt_msg_type = msg_metadata->source.use_rle ? msg_type | rle_enable_mask : msg_type,
    };
    uint8_t *hdr_bytes = (uint8_t *)&hdr;

    const size_t bytes_to_copy = MEMFAULT_MIN(hdr_size - offset, buf_len);
    memcpy(bufp, &hdr_bytes[offset], bytes_to_copy);
    bufp += bytes_to_copy;
    buf_len -= bytes_to_copy;
  } else {
    read_offset = offset - hdr_size;
  }

  if (buf_len == 0) {
    // no space left after writing the header
    return;
  }

  const bool success = msg_metadata->source.impl->read_msg_cb(read_offset, bufp, buf_len);
  if (!success) {
    // Read failures really should never happen. We have no way of knowing if the issue is
    // transient or not. If we aborted the transaction and the failure was persistent, we could get
    // stuck trying to flush out the same data. Instead, we just continue on. We scrub the
    // beginning of the chunk buffer with a known pattern to make the error easier to identify.
    MEMFAULT_LOG_ERROR("Read at offset 0x%" PRIx32 " (%d bytes) for source type %d failed", offset,
                       (int)buf_len, (int)msg_metadata->source.type);
    memset(bufp, 0xEF, MEMFAULT_MIN(16, buf_len));
  }
}

static bool prv_get_source_with_data(size_t *total_size, sMemfaultDataSource *active_source) {
  for (size_t i = 0; i < MEMFAULT_ARRAY_SIZE(s_memfault_data_source); i++) {
    const sMemfaultDataSource *data_source = &s_memfault_data_source[i];

    const bool disabled_source = (((1 << data_source->type) & s_active_data_sources) == 0);
    if (disabled_source) {
      // sdk user has disabled extraction of data for specified source
      continue;
    }

    const bool rle_enabled = data_source->use_rle &&
        memfault_data_source_rle_encoder_set_active(data_source->impl);

    *active_source = (sMemfaultDataSource) {
      .type = data_source->type,
      .use_rle = rle_enabled,
      .impl = rle_enabled ? &g_memfault_data_rle_source : data_source->impl,
    };

    if (active_source->impl->has_more_msgs_cb(total_size)) {
      return true;
    }
  }
  return false;
}

static bool prv_more_messages_to_send(sMessageMetadata *msg_metadata) {
  size_t total_size;
  sMemfaultDataSource active_source;
  if (!prv_get_source_with_data(&total_size, &active_source)) {
    return false;
  }

  if (msg_metadata != NULL) {
    *msg_metadata = (sMessageMetadata) {
      .total_size = total_size,
      .source = active_source,
    };
  }

  return true;
}

static bool prv_load_next_message_to_send(bool enable_multi_packet_chunks,
                                          sMfltTransportState *state) {
  sMessageMetadata msg_metadata;
  if (!prv_more_messages_to_send(&msg_metadata)) {
    return false;
  }

  *state = (sMfltTransportState) {
    .active_message = true,
    .msg_metadata = msg_metadata,
    .curr_msg_ctx = (sMfltChunkTransportCtx) {
      .total_size = msg_metadata.total_size + sizeof(sMfltPacketizerHdr),
      .read_msg = prv_data_source_chunk_transport_msg_reader,
      .enable_multi_call_chunk = enable_multi_packet_chunks,
    },
  };
  memfault_chunk_transport_get_chunk_info(&s_mflt_packetizer_state.curr_msg_ctx);
  return true;
}

static void prv_mark_message_send_complete_and_cleanup(void) {
  // we've finished sending the data so delete it
  s_mflt_packetizer_state.msg_metadata.source.impl->mark_msg_read_cb();

  prv_reset_packetizer_state();
}

void memfault_packetizer_abort(void) {
  prv_reset_packetizer_state();
}

eMemfaultPacketizerStatus memfault_packetizer_get_next(void *buf, size_t *buf_len) {
  if (buf == NULL || buf_len == NULL) {
    // We may want to consider just asserting on these. For now, just log an error
    // and return NoMoreData
    MEMFAULT_LOG_ERROR("%s: NULL input arguments", __func__);
    return kMemfaultPacketizerStatus_NoMoreData;
  }

  if (!s_mflt_packetizer_state.active_message) {
    // To load a new message, memfault_packetizer_begin() must first be called
    return kMemfaultPacketizerStatus_NoMoreData;
  }

  size_t original_size = *buf_len;
  bool md = memfault_chunk_transport_get_next_chunk(
      &s_mflt_packetizer_state.curr_msg_ctx, buf, buf_len);

  if (*buf_len == 0) {
    MEMFAULT_LOG_ERROR("Buffer of %d bytes too small to packetize data",
                       (int)original_size);
  }

  if (!md) {
    // the entire message has been chunked up, perform clean up
    prv_mark_message_send_complete_and_cleanup();

    // we have reached the end of a message
    return kMemfaultPacketizerStatus_EndOfChunk;
  }

  return s_mflt_packetizer_state.curr_msg_ctx.enable_multi_call_chunk ?
      kMemfaultPacketizerStatus_MoreDataForChunk : kMemfaultPacketizerStatus_EndOfChunk;
}

bool memfault_packetizer_begin(const sPacketizerConfig *cfg,
                               sPacketizerMetadata *metadata_out) {
  if ((cfg == NULL) || (metadata_out == NULL)) {
    MEMFAULT_LOG_ERROR("%s: NULL input arguments", __func__);
    return false;
  }

  if (!s_mflt_packetizer_state.active_message) {
    if (!prv_load_next_message_to_send(cfg->enable_multi_packet_chunk, &s_mflt_packetizer_state)) {
      // no new messages to send
      *metadata_out = (sPacketizerMetadata) { 0 };
      return false;
    }
  }

  const bool send_in_progress = s_mflt_packetizer_state.curr_msg_ctx.read_offset != 0;
  *metadata_out = (sPacketizerMetadata) {
    .single_chunk_message_length = s_mflt_packetizer_state.curr_msg_ctx.single_chunk_message_length,
    .send_in_progress = send_in_progress,
  };
  return true;
}

bool memfault_packetizer_data_available(void) {
  if (s_mflt_packetizer_state.active_message) {
    return true;
  }

  return prv_more_messages_to_send(NULL);
}

bool memfault_packetizer_get_chunk(void *buf, size_t *buf_len) {
  const sPacketizerConfig cfg = {
    // By setting this to false, every call to "memfault_packetizer_get_next()" will return one
    // "chunk" that you must send from the device
    .enable_multi_packet_chunk = false,
  };

  sPacketizerMetadata metadata;
  bool data_available = memfault_packetizer_begin(&cfg, &metadata);
  if (!data_available) {
    // there are no more chunks to send
    return false;
  }

  eMemfaultPacketizerStatus packetizer_status = memfault_packetizer_get_next(buf, buf_len);

  // We know data is available from the memfault_packetizer_begin() call above
  // so anything but kMemfaultPacketizerStatus_EndOfChunk is unexpected
  if (packetizer_status != kMemfaultPacketizerStatus_EndOfChunk) {
    MEMFAULT_LOG_ERROR("Unexpected packetizer status: %d", (int)packetizer_status);
    return false;
  }

  return true;
}
