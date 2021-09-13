#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! esp-idf port specific functions related to http

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMFAULT_HTTP_CLIENT_MIN_BUFFER_SIZE 1024

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

typedef struct {
  //! Optional: Context for use by the caller
  void *user_ctx;

  //! Called if a new ota update is available
  //! @return true to continue, false to abort the OTA download
  bool (*handle_update_available)(void *user_ctx);

  //! Called once the entire ota payload has been saved to flash
  //!
  //! This is typically where any final shutdown handler logic would be performed
  //! and esp_restart() would be called
  bool (*handle_download_complete)(void *user_ctx);
} sMemfaultOtaUpdateHandler;

//! Handler which can be used to run OTA update using Memfault's Release Mgmt Infra
//! For more details see:
//!  https://mflt.io/release-mgmt
//!
//! @param handler Context with info necessary to perform an OTA. See struct
//!  definition for more details.
//!
//! @note This function is blocking. 'handler' callbacks will be invoked prior to the function
//!   returning.
//!
//! @return
//!   < 0 Error while trying to figure out if an update was available
//!     0 Check completed successfully - No new update available
//!     1 New update is available and handlers were invoked
int memfault_esp_port_ota_update(const sMemfaultOtaUpdateHandler *handler);

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
