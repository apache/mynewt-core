//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! See header for more details

#include "memfault/config.h"

#if MEMFAULT_DATA_SOURCE_RLE_ENABLED

#include "memfault/core/data_source_rle.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer_source.h"
#include "memfault/core/math.h"
#include "memfault/util/rle.h"

//! Helper function that computes the RLE length of the message being processed
//!
//! @note Expects to be fed all the bytes from the backing message sequentially
//! @return true when the check has completed and calling memfault_data_source_rle_has_more_msgs()
//! will result in no more backing flash reads, false otherwise
static bool prv_data_source_rle_has_more_msgs_prepare(const void *data, size_t data_len);

//! @return the offset the next call to memfault_data_source_rle_read_msg() will start reading from
static uint32_t prv_data_source_rle_get_backing_read_offset(void);

//! Helper function which finds the next RLE sequence that will be written out
//!
//! @note Expects to be fed all the bytes from the backing message sequentially starting from
//!   prv_data_source_rle_get_backing_read_offset()
//! @return true when enough data has been processed to figure out what needs to be written
static bool prv_data_source_rle_read_msg_prepare(const void *data, size_t data_len);

//! Helper function that builds buffer returned from memfault_data_source_rle_read_msg()
//!
//! @return 0 the number of bytes populated in buf
static uint32_t prv_data_source_rle_build_msg_incremental(uint8_t *buf, size_t buf_len);

static const sMemfaultDataSourceImpl *s_active_data_source = NULL;

typedef enum {
  kMemfaultDataSourceRleState_Inactive = 0,
  // Searching for the next sequence to encode
  kMemfaultDataSourceRleState_FindingSeqLength,
  // A sequence to encode has been found and is being written out
  // via calls to memfault_data_source_rle_read_msg()
  kMemfaultDataSourceRleState_WritingSequence,
} eMemfaultDataSourceRleState;

typedef struct {
  eMemfaultDataSourceRleState state;
  uint8_t temp_buf[128];
  // The number of bytes written within the current RLE sequence
  uint32_t write_offset;
  // The total number of bytes that have been processed from the backing data source
  uint32_t bytes_processed;
  // The current number of encoded bytes which have been written
  uint32_t curr_encoded_len;
} sMemfaultDataSourceRleEncodeCtx;

typedef struct {
  sMemfaultDataSourceImpl *data_source;
  size_t original_size;
  size_t total_rle_size;
  sMemfaultRleCtx rle_ctx;
  sMemfaultDataSourceRleEncodeCtx encode_ctx;
} sMemfaultDataSourceRleState;

static sMemfaultDataSourceRleState s_ds_rle_state;

bool memfault_data_source_rle_encoder_set_active(const sMemfaultDataSourceImpl *source) {
  if (source == s_active_data_source) {
    return true;
  }

  s_ds_rle_state = (sMemfaultDataSourceRleState) { 0 };
  s_active_data_source = source;
  return true;
}

static bool prv_data_source_rle_has_more_msgs_prepare(const void *data,
                                                      size_t data_len) {
  const uint8_t *buf = data;

  size_t bytes_encoded = 0;
  while (bytes_encoded != data_len) {
    bytes_encoded += memfault_rle_encode(
        &s_ds_rle_state.rle_ctx, &buf[bytes_encoded], data_len - bytes_encoded);
  }

  const bool all_bytes_processed =
      s_ds_rle_state.rle_ctx.curr_offset == s_ds_rle_state.original_size;
  if (!all_bytes_processed) {
    return false;
  }

  memfault_rle_encode_finalize(&s_ds_rle_state.rle_ctx);
  sMemfaultDataSourceRleEncodeCtx *encode_ctx = &s_ds_rle_state.encode_ctx;
  encode_ctx->state = kMemfaultDataSourceRleState_FindingSeqLength;
  return true;
}

static uint32_t prv_data_source_rle_get_backing_read_offset(void) {
  sMemfaultDataSourceRleEncodeCtx *encode_ctx = &s_ds_rle_state.encode_ctx;
  if (encode_ctx->state == kMemfaultDataSourceRleState_FindingSeqLength) {
    return encode_ctx->bytes_processed;
  }

  // else (encode_ctx->state == kMemfaultDataSourceRleState_WritingSequence)
  sMemfaultRleWriteInfo *write_info = &s_ds_rle_state.rle_ctx.write_info;
  if (encode_ctx->write_offset < write_info->header_len) {
    // Note: this should never happen since the read offset should only be looked up once the
    // header has been written but let's check just in case
    return write_info->write_start_offset;
  }

  const size_t data_bytes_written =
      encode_ctx->write_offset - write_info->header_len;
  return write_info->write_start_offset + data_bytes_written;
}

static bool prv_data_source_rle_read_msg_prepare(const void *data,
                                                 size_t data_len) {

  sMemfaultRleCtx *rle_ctx = &s_ds_rle_state.rle_ctx;
  sMemfaultDataSourceRleEncodeCtx *encode_ctx = &s_ds_rle_state.encode_ctx;
  const size_t bytes_encoded = memfault_rle_encode(rle_ctx, data, data_len);
  if (data_len == 0) {
    memfault_rle_encode_finalize(rle_ctx);
  }
  encode_ctx->bytes_processed += bytes_encoded;

  sMemfaultRleWriteInfo *write_info = &s_ds_rle_state.rle_ctx.write_info;
  if (write_info->available) {
    encode_ctx->state = kMemfaultDataSourceRleState_WritingSequence;
  }
  return write_info->available;
}

static uint32_t prv_data_source_rle_build_msg_incremental(uint8_t *buf,
                                                          size_t buf_len) {
  sMemfaultRleWriteInfo *write_info = &s_ds_rle_state.rle_ctx.write_info;
  if (!write_info->available) {
    return 0;
  }

  sMemfaultDataSourceRleEncodeCtx *encode_ctx = &s_ds_rle_state.encode_ctx;
  uint32_t write_offset = encode_ctx->write_offset;

  // write header
  size_t header_bytes_to_write = 0;
  if (write_offset < write_info->header_len) {
    header_bytes_to_write =
        MEMFAULT_MIN(buf_len, write_info->header_len - write_offset);
    memcpy(buf, &write_info->header[write_offset], header_bytes_to_write);
    encode_ctx->write_offset += header_bytes_to_write;
    buf_len -= header_bytes_to_write;
    if (buf_len == 0) {
      encode_ctx->curr_encoded_len += header_bytes_to_write;
      return header_bytes_to_write;
    }
  }

  const size_t total_write_len = write_info->header_len + write_info->write_len;
  size_t data_to_write =
      MEMFAULT_MIN(buf_len, total_write_len - encode_ctx->write_offset);

  const uint32_t start_offset = prv_data_source_rle_get_backing_read_offset();
  s_active_data_source->read_msg_cb(start_offset, &buf[header_bytes_to_write],
                                    data_to_write);
  encode_ctx->write_offset += data_to_write;

  const size_t bytes_written = header_bytes_to_write + data_to_write;
  if (encode_ctx->write_offset == total_write_len) {
    encode_ctx->write_offset = 0;
    encode_ctx->state = kMemfaultDataSourceRleState_FindingSeqLength;
    *write_info = (sMemfaultRleWriteInfo){0};
  }
  encode_ctx->curr_encoded_len += bytes_written;
  return bytes_written;
}

static bool prv_data_source_rle_fill_msg(uint8_t **bufpp, size_t *buf_len) {
  const size_t bytes_written =
      prv_data_source_rle_build_msg_incremental(*bufpp, *buf_len);
  *buf_len -= bytes_written;
  *bufpp += bytes_written;
  return *buf_len == 0;
}

static bool prv_data_source_rle_read(uint32_t offset, void *buf,
                                     size_t buf_len) {
  sMemfaultDataSourceRleEncodeCtx *encode_ctx = &s_ds_rle_state.encode_ctx;
  if (offset != encode_ctx->curr_encoded_len) {
    return false; // Read happened from an unexpected offset
  }

  // if there is already a write pending, flush that data first
  uint8_t *bufp = (uint8_t *)buf;
  bool buf_full = prv_data_source_rle_fill_msg(&bufp, &buf_len);
  if (buf_full) {
    return true;
  }

  while (encode_ctx->bytes_processed != s_ds_rle_state.original_size) {
    uint8_t *working_buf = &encode_ctx->temp_buf[0];
    const size_t working_buf_size = sizeof(encode_ctx->temp_buf);

    const size_t bytes_remaining =
        s_ds_rle_state.original_size - encode_ctx->bytes_processed;
    const size_t bytes_read = MEMFAULT_MIN(bytes_remaining, working_buf_size);

    const size_t read_offset = prv_data_source_rle_get_backing_read_offset();
    s_active_data_source->read_msg_cb(read_offset, working_buf, bytes_read);
    prv_data_source_rle_read_msg_prepare(working_buf, bytes_read);

    // do we know what to write for the next block yet?
    buf_full = prv_data_source_rle_fill_msg(&bufp, &buf_len);
    if (buf_full) {
      return true;
    }
  }

  prv_data_source_rle_read_msg_prepare(NULL, 0);
  prv_data_source_rle_fill_msg(&bufp, &buf_len);
  return true;
}

//! Do one read pass over the data source currently saved in backing
//! storage to compute what the total RLE size of the data we will be encoding
static size_t prv_compute_rle_size(void) {
  sMemfaultRleCtx *rle_ctx = &s_ds_rle_state.rle_ctx;

  size_t bytes_processed = 0;

  while (bytes_processed != s_ds_rle_state.original_size) {
    sMemfaultDataSourceRleEncodeCtx *encode_ctx = &s_ds_rle_state.encode_ctx;
    uint8_t *working_buf = &encode_ctx->temp_buf[0];
    const size_t working_buf_size = sizeof(encode_ctx->temp_buf);
    const size_t bytes_left = s_ds_rle_state.original_size - bytes_processed;
    const size_t bytes_to_read = MEMFAULT_MIN(bytes_left, working_buf_size);
    s_active_data_source->read_msg_cb(bytes_processed, working_buf,
                                      bytes_to_read);
    prv_data_source_rle_has_more_msgs_prepare(working_buf, bytes_to_read);
    bytes_processed += bytes_to_read;
  }

  s_ds_rle_state.total_rle_size = rle_ctx->total_rle_size;

  *rle_ctx = (sMemfaultRleCtx){0};
  return s_ds_rle_state.total_rle_size;
}

MEMFAULT_WEAK
bool memfault_data_source_rle_read_msg(uint32_t offset, void *buf, size_t buf_len) {
  return prv_data_source_rle_read(offset, buf, buf_len);
}

bool memfault_data_source_rle_has_more_msgs(size_t *total_size_out) {
  // Check to see if the data source has any messages queued up
  const bool has_msgs = s_active_data_source->has_more_msgs_cb(&s_ds_rle_state.original_size);
  if (!has_msgs) {
    return has_msgs;
  }

  // we have already computed what the RLE size will be for the data
  // saved in storage, no need to do it again
  if (s_ds_rle_state.total_rle_size != 0) {
    *total_size_out = s_ds_rle_state.total_rle_size;
    return true;
  }

  *total_size_out = prv_compute_rle_size();
  return true;
}

void memfault_data_source_rle_mark_msg_read(void) {
  s_ds_rle_state = (sMemfaultDataSourceRleState) { 0 };
  s_active_data_source->mark_msg_read_cb();
}

//! Expose a data source for use by the Memfault Packetizer
const sMemfaultDataSourceImpl g_memfault_data_rle_source = {
  .has_more_msgs_cb = memfault_data_source_rle_has_more_msgs,
  .read_msg_cb = memfault_data_source_rle_read_msg,
  .mark_msg_read_cb = memfault_data_source_rle_mark_msg_read,
};

#endif /* MEMFAULT_DATA_SOURCE_RLE_ENABLED */
