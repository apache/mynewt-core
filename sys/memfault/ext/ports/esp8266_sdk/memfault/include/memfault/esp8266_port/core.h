#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!

#include <stdbool.h>
#include <stddef.h>

#include "rom/ets_sys.h"

#ifdef __cplusplus
extern "C" {
#endif

//! A printf() routine that is safe to call when interrupts are disabled.
#define MEMFAULT_ESP_PANIC_PRINTF(_fmt, ...) ets_printf(_fmt, ##__VA_ARGS__)

//! Returns whether or not there is Memfault data to send
//!
//! This function is called by memfault_http_client_post_data() when data is being pushed to the
//! memfault service.
//!
//! There is a weak implementation defined by default which checks for locally collected memfault
//! data. If multiple MCUs are forwarding data to the ESP32 for transport, this function can
//! be overridden to check the sources for data as well.
//!
//! @return true if there is memfault data available to send, false otherwise
bool memfault_esp_port_data_available(void);

//! Fills buffer with a Memfault "chunk" when there is data available
//!
//! This function is called by memfault_http_client_post_data() when data is being pushed to the
//! memfault service.
//!
//! There is a weak implementation defined by default which checks for locally collected memfault
//! data. If multiple MCUs are forwarding data to the ESP32 for transport, this function can
//! be overridden to check the sources for data as well.
//!
//! @param[out] buf The buffer to copy data to be sent into
//! @param[in,out] buf_len The size of the buffer to copy data into. On return, populated
//! with the amount of data, in bytes, that was copied into the buffer.
//!
//! @return true if the buffer was filled, false otherwise
bool memfault_esp_port_get_chunk(void *buf, size_t *buf_len);

//! Initializes esp port
void memfault_esp_port_boot(void);

//
// Internal initialization APIs called by memfault_esp_port_boot()
//

void memfault_esp_port_event_collection_boot(void);
void memfault_esp_port_coredump_storage_boot(void);
void memfault_register_cli(void);

#ifdef __cplusplus
}
#endif
