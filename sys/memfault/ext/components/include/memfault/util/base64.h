#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Utilities for base64 encoding binary data

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Computes how many bytes will be needed to encode a binary blob of bin_len
//! using base64
#define MEMFAULT_BASE64_ENCODE_LEN(bin_len) (4 * (((bin_len) + 2) / 3))

//! Base64 encode a given binary buffer
//!
//! @note Uses the standard base64 alphabet from https://tools.ietf.org/html/rfc4648#section-4
//!
//! @param[in] bin Pointer to the binary buffer to base64 encode
//! @param[in] bin_len Length of the binary buffer
//! @param[out] pointer to buffer to write base64 encoded data into. The length of the buffer must
//!    be >= MEMFAULT_BASE64_ENCODE_LEN(bin_len) bytes
void memfault_base64_encode(const void *buf, size_t buf_len, void *base64_out);

#ifdef __cplusplus
}
#endif
