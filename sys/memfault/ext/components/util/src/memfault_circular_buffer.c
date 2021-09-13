//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Convenience circular buffer utility

#include "memfault/util/circular_buffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "memfault/core/math.h"

bool memfault_circular_buffer_init(sMfltCircularBuffer *circular_buf,
                                   void *storage_buf, size_t storage_len) {
  if ((circular_buf == NULL) || (storage_buf == NULL) || (storage_len == 0)) {
    return false;
  }

  // doesn't really matter but put buffer in a clean state for easier debug
  memset(storage_buf, 0x0, storage_len);

  *circular_buf = (sMfltCircularBuffer){.read_offset = 0,
                                        .read_size = 0,
                                        .total_space = storage_len,
                                        .storage = storage_buf};

  return true;
}

bool memfault_circular_buffer_read(sMfltCircularBuffer *circular_buf,
                                   size_t offset, void *data, size_t data_len) {
  if ((circular_buf == NULL) || (data == NULL)) {
    return false;
  }

  if (circular_buf->read_size < (offset + data_len)) {
    return false;
  }

  size_t read_idx =
      (circular_buf->read_offset + offset) % circular_buf->total_space;
  size_t contiguous_space_available = circular_buf->total_space - read_idx;
  size_t bytes_to_read = (contiguous_space_available > data_len)
                             ? data_len
                             : contiguous_space_available;

  uint8_t *buf = data;
  memcpy(buf, &circular_buf->storage[read_idx], bytes_to_read);
  buf += bytes_to_read;
  size_t bytes_rem = data_len - bytes_to_read;
  if (bytes_rem != 0) {
    memcpy(buf, &circular_buf->storage[0], bytes_rem);
  }

  return true;
}

bool memfault_circular_buffer_get_read_pointer(sMfltCircularBuffer *circular_buf, size_t offset,
                                               uint8_t **read_ptr, size_t *read_ptr_len) {
  if ((circular_buf == NULL) || (read_ptr == NULL) || (read_ptr_len == NULL)) {
    return false;
  }

  if (circular_buf->read_size < offset) {
    return false;
  }

  const size_t read_idx =
      (circular_buf->read_offset + offset) % circular_buf->total_space;
  const size_t max_bytes_to_read = circular_buf->read_size - offset;
  const size_t contiguous_space_available = circular_buf->total_space - read_idx;

  *read_ptr = &circular_buf->storage[read_idx];
  *read_ptr_len = MEMFAULT_MIN(contiguous_space_available, max_bytes_to_read);
  return true;
}

bool memfault_circular_buffer_read_with_callback(sMfltCircularBuffer *circular_buf,
                                                 size_t offset, size_t data_len, void *ctx,
                                                 MemfaultCircularBufferReadCallback callback) {
  if (circular_buf == NULL) {
    return false;
  }
  if (callback == NULL) {
    return false;
  }
  if (circular_buf->read_size < (offset + data_len)) {
    return false;
  }

  size_t bytes_left = data_len;
  uint8_t *read_ptr = NULL;
  size_t read_ptr_len = 0;
  while (bytes_left) {
    const size_t dst_offset = data_len - bytes_left;
    if (!memfault_circular_buffer_get_read_pointer(
      circular_buf, offset + dst_offset, &read_ptr, &read_ptr_len)) {
      // Note: At this point, the memfault_circular_buffer_get_read_pointer() calls should never fail. A
      // failure is indicative of memory corruption (e.g calls taking place from multiple tasks without
      // having implemented memfault_lock() / memfault_unlock())
      return false;
    }
    const size_t bytes_to_read = MEMFAULT_MIN(bytes_left, read_ptr_len);
    if (!callback(ctx, dst_offset, read_ptr, bytes_to_read)) {
      return false;
    }
    bytes_left -= bytes_to_read;
  }
  return true;
}

bool memfault_circular_buffer_consume(sMfltCircularBuffer *circular_buf, size_t consume_len) {
  if (circular_buf == NULL) {
    return false;
  }

  if (circular_buf->read_size < consume_len) {
    return false;
  }

  circular_buf->read_offset =
      (circular_buf->read_offset + consume_len) % circular_buf->total_space;
  circular_buf->read_size -= consume_len;
  return true;
}

bool memfault_circular_buffer_consume_from_end(
    sMfltCircularBuffer *circular_buf, size_t consume_len) {
  if (circular_buf == NULL) {
    return false;
  }

  if (circular_buf->read_size < consume_len) {
    return false;
  }

  circular_buf->read_size -= consume_len;
  return true;
}

static size_t prv_get_space_available(const sMfltCircularBuffer *circular_buf) {
  return circular_buf->total_space - circular_buf->read_size;
}

size_t memfault_circular_buffer_get_write_size(const sMfltCircularBuffer *circular_buf) {
  if (circular_buf == NULL) {
    return 0;
  }

  return prv_get_space_available(circular_buf);
}

static bool prv_write_at_offset_from_end(sMfltCircularBuffer *circular_buf, size_t offset_from_end,
                                         const void *data, size_t data_len) {
  if ((circular_buf == NULL) || (data == NULL)) {
    return false;
  }

  if (circular_buf->read_size < offset_from_end) {
    // we can't write to an offset that doesn't exist
    return false;
  }

  const size_t new_bytes_to_write = data_len > offset_from_end ? data_len - offset_from_end : 0;
  if (prv_get_space_available(circular_buf) < new_bytes_to_write) {
    return false;
  }

  size_t write_idx = (circular_buf->read_offset + circular_buf->read_size - offset_from_end) %
                     circular_buf->total_space;
  size_t contiguous_space_available = circular_buf->total_space - write_idx;

  size_t bytes_to_write = (contiguous_space_available > data_len)
                              ? data_len
                              : contiguous_space_available;

  const uint8_t *buf = data;
  memcpy(&circular_buf->storage[write_idx], buf, bytes_to_write);
  buf += bytes_to_write;
  size_t bytes_rem = data_len - bytes_to_write;
  if (bytes_rem != 0) {
    memcpy(&circular_buf->storage[0], buf, bytes_rem);
  }

  circular_buf->read_size += new_bytes_to_write;
  return true;
}

bool memfault_circular_buffer_write(sMfltCircularBuffer *circular_buf,
                                    const void *data, size_t data_len) {
  return prv_write_at_offset_from_end(circular_buf, 0, data, data_len);
}

bool memfault_circular_buffer_write_at_offset(
    sMfltCircularBuffer *circular_buf, size_t offset_from_end, const void *data, size_t data_len) {
  return prv_write_at_offset_from_end(circular_buf, offset_from_end, data, data_len);
}

size_t memfault_circular_buffer_get_read_size(const sMfltCircularBuffer *circular_buf) {
  if (circular_buf == NULL) {
    return 0;
  }
  return circular_buf->read_size;
}
