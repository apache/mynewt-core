#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//!
//! Utilities for exporting data collected by the Memfault SDK ("chunks") to a file or a log
//! stream for upload to the Memfault cloud.
//!
//! This can be used for production use-cases where data is extracted over pre-existing logging
//! facilities or during initial bringup before another transport is in place.
//!
//! The extracted data can be published to the Memfault cloud using the memfault-cli:
//! $ memfault --project-key ${YOUR_PROJECT_KEY} post-chunk --encoding sdk_data_export your_exported_data.txt
//!
//! A step-by-step integration guide with more details can be found at:
//!   https://mflt.io/chunk-data-export

#include <stddef.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/util/base64.h"

#ifdef __cplusplus
extern "C" {
#endif

//! A utility for dumping all the currently collected Memfault Data
//!
//! While there is still data available to send, this API makes calls to
//! 'memfault_data_export_chunk()'
void memfault_data_export_dump_chunks(void);

//! Called by 'memfault_data_export_chunk' once a chunk has been formatted as a string
//!
//! @note Defined as a weak function so an end user can override it and control where the chunk is
//! written. The default implementation prints the "chunk" by calling MEMFAULT_LOG_INFO() but an
//! end user could override to save chunks elsewhere (for example a file).
//!
//! @param chunk_str - A NUL terminated string with a base64 encoded Memfault "chunk" with a "MC:"
//!   as a header and ":" as a footer.
void memfault_data_export_base64_encoded_chunk(const char *chunk_str);

//! Encodes a Memfault "chunk" as a string and calls memfault_data_export_base64_encoded_chunk
//!
//! @note The string is formatted as 'MC:CHUNK_DATA_BASE64_ENCODED:'. We wrap the base64 encoded
//! chunk in a prefix ("MC:") and suffix (":") so the chunks can be even be extracted from logs with
//! other data
//!
//! @note This command can also be used with the Memfault GDB command "memfault
//! install_chunk_handler" to "drain" chunks up to the Memfault cloud directly from GDB. This can
//! be useful when working on integrations and initially getting a transport path in place:
//!
//!   (gdb) source $MEMFAULT_FIRMWARE_SDK/scripts/memfault_gdb.py
//!   (gdb) memfault install_chunk_handler --help
//!   (gdb) memfault install_chunk_handler -pk <YOUR_PROJECT_KEY>
//!
//!   For more details see https://mflt.io/posting-chunks-with-gdb
//!
//! @param chunk_data The binary chunk data to send
//! @param chunk_data_len The length of the chunk data to send. Must be less than
//!  or equal to MEMFAULT_DATA_EXPORT_CHUNK_MAX_LEN
void memfault_data_export_chunk(void *chunk_data, size_t chunk_data_len);

#define MEMFAULT_DATA_EXPORT_BASE64_CHUNK_PREFIX "MC:" // *M*emfault *C*hunk
#define MEMFAULT_DATA_EXPORT_BASE64_CHUNK_PREFIX_LEN \
  MEMFAULT_STATIC_STRLEN(MEMFAULT_DATA_EXPORT_BASE64_CHUNK_PREFIX)

#define MEMFAULT_DATA_EXPORT_BASE64_CHUNK_SUFFIX ":"
#define MEMFAULT_DATA_EXPORT_BASE64_CHUNK_SUFFIX_LEN \
  MEMFAULT_STATIC_STRLEN(MEMFAULT_DATA_EXPORT_BASE64_CHUNK_SUFFIX)

#define MEMFAULT_DATA_EXPORT_BASE64_CHUNK_MAX_LEN                       \
  (MEMFAULT_DATA_EXPORT_BASE64_CHUNK_PREFIX_LEN +                       \
   MEMFAULT_BASE64_ENCODE_LEN(MEMFAULT_DATA_EXPORT_CHUNK_MAX_LEN) +     \
   MEMFAULT_DATA_EXPORT_BASE64_CHUNK_SUFFIX_LEN +                       \
   1 /* '\0' */ )

#ifdef __cplusplus
}
#endif
