#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! C Implementation of the CRC-16 CCITT (xmodem) polynomial:
//!   x^16 + x^12 + x^5 + 1 (0x1021)
//! More details:
//!   http://reveng.sourceforge.net/crc-catalogue/16.htm#crc.cat.crc-16-xmodem

#include <stddef.h>
#include <stdint.h>

#define MEMFAULT_CRC16_CCITT_INITIAL_VALUE 0x0

#ifdef __cplusplus
extern "C" {
#endif

//! Computes the CRC16-CCITT value for the given sequence
//!
//! The CRC is used in a wide spread of protocols (i.e HDLC, ISDN) and has a high probability of
//! being able to detect errors when used like a checksum
//!
//! @param crc_initial_value Use MEMFAULT_CRC16_CCITT_INITIAL_VALUE when computing a new checksum over
//!  the entire data set passed in. Use the current CRC16-CCITT value if computing over a data set
//!  incrementally
//! @param data The data to compute the crc over
//! @param data_len_bytes the length of the data to compute the crc over, in bytes
uint16_t memfault_crc16_ccitt_compute(
    uint16_t crc_initial_value, const void *data, size_t data_len_bytes);

#ifdef __cplusplus
}
#endif
