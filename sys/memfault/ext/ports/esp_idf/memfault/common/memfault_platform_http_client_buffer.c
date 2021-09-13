//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Default implementation for buffer allocation while POSTing memfault chunk data

#include "memfault/esp_port/http_client.h"

#include <stdlib.h>
#include <stdint.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"

#ifndef MEMFAULT_HTTP_CLIENT_MAX_BUFFER_SIZE
#  define MEMFAULT_HTTP_CLIENT_MAX_BUFFER_SIZE  (16 * 1024)
#endif

#if MEMFAULT_HTTP_CLIENT_MAX_BUFFER_SIZE < MEMFAULT_HTTP_CLIENT_MIN_BUFFER_SIZE
#error "MEMFAULT_HTTP_CLIENT_MAX_BUFFER_SIZE must be greater than 1024 bytes"
#endif

MEMFAULT_WEAK
void *memfault_http_client_allocate_chunk_buffer(size_t *buffer_size) {
  // The more data we can pack into one http request, the more efficient things will
  // be from a network perspective. Let's start by trying to use a 16kB buffer and slim
  // things down if there isn't that much space available
  size_t try_alloc_size = MEMFAULT_HTTP_CLIENT_MAX_BUFFER_SIZE;
  const uint32_t min_alloc_size = MEMFAULT_HTTP_CLIENT_MIN_BUFFER_SIZE;

  void *buffer = NULL;
  while (try_alloc_size > min_alloc_size) {
    buffer = malloc(try_alloc_size);
    if (buffer != NULL) {
      *buffer_size = try_alloc_size;
      break;
    }
    try_alloc_size /= 2;
  }

  return buffer;
}

MEMFAULT_WEAK
void memfault_http_client_release_chunk_buffer(void *buffer) {
  free(buffer);
}
