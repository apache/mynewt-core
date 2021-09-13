//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief

#include "memfault/core/data_export.h"

#include <string.h>

#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/sdk_assert.h"
#include "memfault/util/base64.h"

MEMFAULT_WEAK
void memfault_data_export_base64_encoded_chunk(const char *base64_chunk) {
  MEMFAULT_LOG_INFO("%s", base64_chunk);
}

static void prv_memfault_data_export_chunk(void *chunk_data, size_t chunk_data_len) {
  MEMFAULT_SDK_ASSERT(chunk_data_len <= MEMFAULT_DATA_EXPORT_CHUNK_MAX_LEN);

  char base64[MEMFAULT_DATA_EXPORT_BASE64_CHUNK_MAX_LEN];

  memcpy(base64, MEMFAULT_DATA_EXPORT_BASE64_CHUNK_PREFIX,
         MEMFAULT_DATA_EXPORT_BASE64_CHUNK_PREFIX_LEN);

  size_t write_offset = MEMFAULT_DATA_EXPORT_BASE64_CHUNK_PREFIX_LEN;

  memfault_base64_encode(chunk_data, chunk_data_len, &base64[write_offset]);
  write_offset += MEMFAULT_BASE64_ENCODE_LEN(chunk_data_len);

  memcpy(&base64[write_offset], MEMFAULT_DATA_EXPORT_BASE64_CHUNK_SUFFIX,
         MEMFAULT_DATA_EXPORT_BASE64_CHUNK_SUFFIX_LEN);
  write_offset += MEMFAULT_DATA_EXPORT_BASE64_CHUNK_SUFFIX_LEN;

  base64[write_offset] = '\0';

  memfault_data_export_base64_encoded_chunk(base64);
}

//! Note: We disable optimizations for this function to guarantee the symbol is
//! always exposed and our GDB test script (https://mflt.io/send-chunks-via-gdb)
//! can be installed to watch and post chunks every time it is called.
MEMFAULT_NO_OPT
void memfault_data_export_chunk(void *chunk_data, size_t chunk_data_len) {
  prv_memfault_data_export_chunk(chunk_data, chunk_data_len);
}

static bool prv_try_send_memfault_data(void) {
  // buffer to copy chunk data into
  uint8_t buf[MEMFAULT_DATA_EXPORT_CHUNK_MAX_LEN];
  size_t buf_len = sizeof(buf);
  bool data_available = memfault_packetizer_get_chunk(buf, &buf_len);
  if (!data_available ) {
    return false; // no more data to send
  }
  // send payload collected to chunks/ endpoint
  memfault_data_export_chunk(buf, buf_len);
  return true;
}

void memfault_data_export_dump_chunks(void) {
  while (prv_try_send_memfault_data()) { }
}
