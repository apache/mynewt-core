#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! A utility for run length encoding a given stream of data
//!  https://en.wikipedia.org/wiki/Run-length_encoding
//!
//! In short, run-length-encoding is a very simple lossless compression scheme
//! which shortens runs of the same data pattern. This can be very favorable for
//! embedded applications where runs of 0 (i.e bss) and stack guards are very common
//!
//! The format used is ZigZag Varint | Payload where negative integers indicate the run is a
//! sequence of non-repeating bytes and positive integers indicate that the same value which
//! follows is repeated that number of times

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  kMemfaultRleState_Init = 0,
  kMemfaultRleState_RepeatSeq,
  kMemfaultRleState_NonRepeatSeq,
} eMemfaultRleState;

typedef struct {
  //! true if the data is valid
  bool available;
  //! header that should prefix the write sequence
  uint8_t header[5];
  //! length of the header to write (will always be less than sizeof(header))
  size_t header_len;
  //! The offset within the original data fed into the encoder
  //! that the sequence begins at
  uint32_t write_start_offset;
  //! The number of bytes to write
  size_t write_len;
} sMemfaultRleWriteInfo;

typedef struct {
  //
  // Outputs
  //

  //! The total length of the encoded RLE sequence
  uint32_t total_rle_size;
  //! Populated when a new sequence has been detected.
  //! Reset on every invocation of 'memfault_rle_encode'
  sMemfaultRleWriteInfo write_info;

  //
  // Internals
  //

  //! The value of the last byte which the encoder inspected
  uint8_t last_byte;
  //! The start offset within the backing stream where the current
  //! sequence being inspected began.
  //! For example, for an input pattern of { *4, 4, *5, 6, 7, *8, *9, 9 }
  //! sequences start at the * markings (offset 0, 2, 5 & 6, respectively)
  uint32_t seq_start_offset;
  //! The current state the encoder is in. See eMemfaultRleState for more details
  eMemfaultRleState state;
  //! The number of bytes that make up the sequence
  size_t seq_count;
  //! The number of times the same value has been seen in a row. For example, after the pattern {
  //! *4, 5, 5 } the count would be 1 since 5 has been repeated once
  size_t num_repeats;
  //! The current number of bytes which have been streamed into the encoder and processed
  uint32_t curr_offset;

} sMemfaultRleCtx;

//! A utility for RLE a stream of data
//!
//! @param ctx The context tracking the state for the encoding
//! @param buf Buffer filled with the data to process
//! @param buf_size The size of the input buffer
//! @return the number of bytes processed (may be less than buf_size)
//!  Upon return ctx->write_info.available can be checked to check if a new
//!  write sequence was detected
size_t memfault_rle_encode(sMemfaultRleCtx *ctx, const void *buf, size_t buf_size);

//! Should be called after an entire buffer has been encoded by memfault_rle_encode
//! This will flush the final write needed to encode the sequence to ctx->write_info
void memfault_rle_encode_finalize(sMemfaultRleCtx *ctx);
#ifdef __cplusplus
}
#endif
