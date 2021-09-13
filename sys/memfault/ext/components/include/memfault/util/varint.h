#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Simple utilities for interacting with Base 128 Varints.
//!
//! More details about the encoding scheme can be found at:
//!   https://developers.google.com/protocol-buffers/docs/encoding#varints

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! The maximum length of a VARINT encoding for a uint32_t in bytes
#define MEMFAULT_UINT32_MAX_VARINT_LENGTH 5

//! Given a uint32_t, encodes it as a Varint
//!
//! @param[in] value The value to encode
//! @param[out] buf The buffer to write the Varint encoding to. Needs to be appropriately sized to
//!   hold the encoding. The maximum encoding length is MEMFAULT_UINT32_MAX_VARINT_LENGTH
//! @return The number of bytes written into the buffer
size_t memfault_encode_varint_u32(uint32_t value, void *buf);

//! Given an int32_t, encodes it as a ZigZag varint
//!
//! @note a ZigZag varint can encode the range -2147483648 to 2147483647 and is an optimization
//!   technique that ensures the smallest absolute values take up the smallest amount of
//!   space. More details can be found at the following link:
//!   https://developers.google.com/protocol-buffers/docs/encoding#signed-integers
//! @param[in] value The value to encode
//! @param[out] buf The buffer to write the Varint encoding to. Needs to be appropriately sized to
//!   hold the encoding. The maximum encoding length is MEMFAULT_UINT32_MAX_VARINT_LENGTH
size_t memfault_encode_varint_si32(int32_t value, void *buf);

#ifdef __cplusplus
}
#endif
