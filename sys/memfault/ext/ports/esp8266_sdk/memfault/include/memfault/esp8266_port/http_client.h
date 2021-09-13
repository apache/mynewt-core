#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! esp-idf port specific functions related to http

#include "sdkconfig.h"

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMFAULT_HTTP_CLIENT_MAX_BUFFER_SIZE  CONFIG_MEMFAULT_HTTP_CLIENT_BUFFER_SIZE
#define MEMFAULT_HTTP_CLIENT_MIN_BUFFER_SIZE 512

//! Called to get a buffer to use for POSTing data to the Memfault cloud
//!
//! @note The default implementation just calls malloc but is defined
//! as weak so a end user can easily override the implementation
//!
//! @param buffer_size[out] Filled with the size of the buffer allocated. The expectation is that
//! the buffer will be >= MEMFAULT_HTTP_CLIENT_MIN_BUFFER_SIZE. The larger the buffer, the less
//! amount of POST requests that will be needed to publish data
//!
//! @return Allocated buffer or NULL on failure
void *memfault_http_client_allocate_chunk_buffer(size_t *buffer_size);

//! Called to release the buffer that was being to POST data to the Memfault cloud
//!
//! @note The default implementation just calls malloc but defined
//! as weak so an end user can easily override the implementation
void memfault_http_client_release_chunk_buffer(void *buffer);

//! POSTs all collected diagnostic data to Memfault Cloud
//!
//! @note This function should only be called when connected to WiFi
//!
//! @return 0 on success, else error code
int memfault_esp_port_http_client_post_data(void);

//! @return true if connected to WiFi, false otherwise
bool memfault_esp_port_wifi_connected(void);

#ifdef __cplusplus
}
#endif
