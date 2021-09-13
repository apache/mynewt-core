#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Platform API for CRC APIs

#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Computes the CRC32 for the provided data.
//!
//! While the actual CRC polynomial used does not matter to Memfault, the recommendation is to
//! compute the standard ZIP file checksum. This is the standard "CRC32" computed by libraries
//! implemented in most languages
//!
//! @param data data buffer to compute the crc over
//! @param data_len length of the buffer to compute the crc32 over
//!
//! @return the crc over the provided buffer
uint32_t memfault_platform_crc32(const void *data, size_t data_len);

#ifdef __cplusplus
}
#endif
