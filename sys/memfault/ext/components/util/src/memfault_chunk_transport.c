//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! See header for more details around the motivation
//!
//! Chunk Mesage Types
//!
//!  INIT Message:
//!    HEADER_BYTE || (HEADER_BYTE.MD ? varint(TOTAL_LENGTH) : b"") || CHUNK_DATA  || (HEADER_BYTE.MD ? 0b"" : CRC_16_CCITT)
//!
//!    NOTE: If the entire message can fit in a single MTU, no TOTAL_LENGTH is encoded
//!    NOTE: We expect message integrity from the underlying transport. The CRC16 is only present
//!          to eventually detect if this expectation is broken by the consumer stack.
//!
//! CONTINUATION Message:
//!   HEADER_BYTE || varint(OFFSET) || CHUNK_DATA || (HEADER_BYTE.MD ? 0b"" : CRC_16_CCITT)

#include "memfault/util/chunk_transport.h"

#include <stdbool.h>
#include <string.h>

#include "memfault/core/compiler.h"
#include "memfault/core/math.h"
#include "memfault/util/crc16_ccitt.h"
#include "memfault/util/varint.h"

typedef struct {
  bool md;
  bool continuation;
} sMemfaultHeaderSettings;

static uint8_t prv_build_hdr(const sMemfaultHeaderSettings *settings) {
  // bits 0-2: channel id (0 - 7) Always 0 at the moment but reserved for future where we want
  //           prioritization
  // bit 3-5:  CFG - Protocol configuration settings
  //           For INIT Packet
  //            0b000 indicates crc16 is written in the init chunk
  //            0b001 indicates crc16 is written at the end of the last chunk which makes up the msg
  //            Remaing Values: Reserved for future use
  //           For CONTINUATION:
  //            All zeros. Reserved for future use (i.e. to make TOTAL_LENGTH and CRC16_CCITT
  //            optional)
  // bit 6:    MD: 1 if a CONTINUATION will follow (more data) or 0 if this is the last chunk of this
  //           message.  Right now I'm only using it to conditionally include the TOTAL_LENGTH. But
  //           I think it could also be useful as a trigger for the consumer to run a
  //           "basic_recover" to get all messages in the queue when the final one has arrived and
  //           process them all at once. This only makes sense if we're using more specific queues
  //           (i.e. per-device).
  // bit 7:    CONT: 0 for INIT, 1 for CONTINUATION
  //           The first chunk in a sequence of chunks must use INIT and following chunks must
  //           use CONTINUATION.
  uint8_t hdr = ((uint8_t)(settings->continuation << 7) | (uint8_t)(settings->md << 6));
  if (!settings->continuation) {
    hdr |= 1 << 3;
  }
  return hdr;
}

MEMFAULT_STATIC_ASSERT(
    MEMFAULT_MIN_CHUNK_BUF_LEN == (1 /* hdr */ +  MEMFAULT_UINT32_MAX_VARINT_LENGTH + 2 /* crc16 */ + 1 /* at least one data byte */),
    "Not enough space to chunk up at least one byte in a maximally sized message");

static size_t prv_compute_single_message_chunk_size(sMfltChunkTransportCtx *ctx) {
  return 1 /* hdr */ + 2 /* crc16 */ + ctx->total_size;
}

bool memfault_chunk_transport_get_next_chunk(sMfltChunkTransportCtx *ctx,
                                             void *out_buf, size_t *out_buf_len) {
  // There's not enough space to encode anything. Consumers of this API should be
  // passing a buffer of at least MEMFAULT_MIN_CHUNK_BUF_LEN in length
  if (*out_buf_len < MEMFAULT_MIN_CHUNK_BUF_LEN) {
    *out_buf_len = 0;
    return true;
  }

  uint8_t *chunk_msg = out_buf;

  size_t varint_len = 0;
  size_t chunk_msg_start_offset = 0;
  size_t bytes_to_read = 0;

  const bool init_pkt_type = ctx->read_offset == 0;
  bool more_data;

  const size_t crc16_len = 2;

  if (init_pkt_type) {
    varint_len = memfault_encode_varint_u32(ctx->total_size, &chunk_msg[1]);
    const size_t single_msg_size = prv_compute_single_message_chunk_size(ctx);
    more_data = single_msg_size > *out_buf_len;

    const sMemfaultHeaderSettings init_settings = {
      .md = more_data && !ctx->enable_multi_call_chunk,
      .continuation = false
    };
    ctx->single_chunk_message_length = single_msg_size;

    chunk_msg[0] = prv_build_hdr(&init_settings);
    chunk_msg_start_offset = 1;
    if (init_settings.md) {
      bytes_to_read = *out_buf_len - 1 /* hdr */ - varint_len;
      chunk_msg_start_offset += varint_len;
    } else if (ctx->enable_multi_call_chunk) {
      bytes_to_read = MEMFAULT_MIN(*out_buf_len - 1 /* hdr */, ctx->total_size);
    } else {
      bytes_to_read = ctx->total_size;
    }
  } else if (ctx->enable_multi_call_chunk) {
    const size_t bytes_remaining = ctx->total_size - ctx->read_offset;
    bytes_to_read = MEMFAULT_MIN(*out_buf_len, bytes_remaining);
    const size_t out_buf_space_rem = *out_buf_len - bytes_to_read;
    more_data = (out_buf_space_rem < crc16_len);
  } else {
    varint_len = memfault_encode_varint_u32(ctx->read_offset, &chunk_msg[1]);
    const size_t bytes_remaining = ctx->total_size - ctx->read_offset;

    size_t out_buf_space_rem = *out_buf_len - 1 /* hdr */ - varint_len;
    bytes_to_read = MEMFAULT_MIN(out_buf_space_rem, bytes_remaining);
    out_buf_space_rem -= bytes_to_read;
    more_data = (out_buf_space_rem < crc16_len);
    const sMemfaultHeaderSettings cont_settings = {
      .md = more_data,
      .continuation = true,
    };
    chunk_msg[0] = prv_build_hdr(&cont_settings);
    chunk_msg_start_offset = 1 /* hdr */ + varint_len;
  }

  if (bytes_to_read != 0) {
    uint8_t *msg_bufp = &chunk_msg[chunk_msg_start_offset];
    ctx->read_msg(ctx->read_offset, msg_bufp, bytes_to_read);
    ctx->crc16_incremental = memfault_crc16_ccitt_compute(
        ctx->crc16_incremental, msg_bufp, bytes_to_read);
    chunk_msg_start_offset += bytes_to_read;
  }

  if (!more_data) {
    // The entire message CRC has been computed, add it to the end of the message
    // for sanity checking message integrity
    const uint16_t crc16 = ctx->crc16_incremental;
    chunk_msg[chunk_msg_start_offset] = crc16 & 0xff;
    chunk_msg[chunk_msg_start_offset + 1] = (crc16 >> 8) & 0xff;
    chunk_msg_start_offset += crc16_len;
  }

  ctx->read_offset += bytes_to_read;
  const size_t bytes_written = chunk_msg_start_offset;
  const size_t buf_space_rem = *out_buf_len - bytes_written;
  if (buf_space_rem != 0) {
    // The encoded chunk consumes less space than the buffer provided. This can
    // happen when we reach the end of the underlying message being encoded
    //
    // The Memfault backend allows for the last chunk in a message to exceed the
    // size of the underlying message being sent. This can be useful for topologies
    // that require sending chunks of a fixed size.
    //
    // Scrub the remaining part of the buffer for this situation with a know pattern
    // for debug purposes so it's easier to visually see the end of the chunk and to
    // prevent unintentional data from being sent.
    memset(&chunk_msg[bytes_written], 0xBA, buf_space_rem);
  }

  *out_buf_len = bytes_written;

  return more_data;
}

void memfault_chunk_transport_get_chunk_info(sMfltChunkTransportCtx *ctx) {
  if (ctx->read_offset != 0) {
    // info has already been populated
    return;
  }

  ctx->single_chunk_message_length = prv_compute_single_message_chunk_size(ctx);
}
