#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! The esp-idf spi flash was largely refactored between the v3.x and v4.0 release.  This header
//! maintains a think wrapper around the two implementations so the Memfault coredump collection
//! code is compatible with either major version of the esp-idf.
//!
//! For more details about the spi flash driver check out:
//!  https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/storage/spi_flash.html#overview

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int memfault_esp_spi_flash_coredump_begin(void);

int memfault_esp_spi_flash_erase_range(size_t start_address, size_t size);
int memfault_esp_spi_flash_write(size_t dest_addr, const void *src, size_t size);
int memfault_esp_spi_flash_read(size_t src_addr, void *dest, size_t size);

#ifdef __cplusplus
}
#endif
