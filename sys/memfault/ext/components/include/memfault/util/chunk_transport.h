#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Some IOT topologies have an MTU sizes that are less than the size of a Memfault Event or
//! Coredump. In these situations, the Memfault SDK will chunk up the data to be shipped out over
//! the transport and reassembled in the Memfault backend. That is what this API implements.
//!
//! NOTE: Consumers of the Memfault library should never be including this header directly or
//! relying on the details of the serialization format in their own code.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! The minimum buffer size required to generate a chunk.
#define MEMFAULT_MIN_CHUNK_BUF_LEN 9

//! Callback invoked by the chunking transport to read a piece of a message
//!
//! By using a callback, we avoid requiring that the entire message ever need to be allocated in
//! memory at a given time. For example, a coredump saved in flash can be read out piecemeal.
//!
//! @param offset The offset within the message to read
//! @param buf The buffer to copy the read data into
//! @param buf_len The amount of data to be copied.
//! @note The chunk transport will _never_ invoke this callback and request data that is past the
//! total_size of the message being operated on
typedef void (MfltChunkTransportMsgReaderCb)(uint32_t offset, void *buf,
                                             size_t buf_len);

//! Context used to hold the state of the current message being chunked
typedef struct {
  // Input Arguments

  //! The total size of the message to be sent
  uint32_t total_size;
  //! A callback for reading portions of the message to be sent
  MfltChunkTransportMsgReaderCb *read_msg;
  //! Instead of having a "chunk" span one call, allow for a chunk to span across multiple calls to
  //! this API. This is an optimization that allows us to send messages across "one" chunk if the
  //! transport does not have any size restrictions
  bool enable_multi_call_chunk;

  // Output Arguments

  //! The total chunk size of the message being operated on when sent as a single chunk
  uint32_t single_chunk_message_length;
  //! The offset the chunker has read to within the message to send
  uint32_t read_offset;
  //! A CRC computed over the data (up to read_offset). The CRC for the entire message is written
  //! at the end of the last chunk that makes up a message.
  uint16_t crc16_incremental;
} sMfltChunkTransportCtx;

//! Takes a message and chunks it up into smaller messages
//!
//! @param ctx The context tracking this chunking operation
//! @param buf The buffer to copy the chunked message into
//! @param[in,out] buf_len The size of the buffer to copy data into. On return, populated
//! with the amount of data, in bytes, that was copied into the buffer. The length must be
//! at least MEMFAULT_MIN_CHUNK_BUF_LEN
//!
//! @return true if there is more data to send in the message, false otherwise
bool memfault_chunk_transport_get_next_chunk(sMfltChunkTransportCtx *ctx,
                                             void *buf, size_t *buf_len);

//! Computes info about the current chunk being operated on and populates the output arguments of
//! sMfltChunkTransportCtx with the info
void memfault_chunk_transport_get_chunk_info(sMfltChunkTransportCtx *ctx);

#ifdef __cplusplus
}
#endif
