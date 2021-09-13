//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Memfault HTTP Client implementation which can be used to send data to the Memfault cloud for
//! processing

#include "memfault/http/http_client.h"

#include <stdio.h>

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/errors.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/http/platform/http_client.h"

bool memfault_http_build_url(char url_buffer[MEMFAULT_HTTP_URL_BUFFER_SIZE], const char *subpath) {
  sMemfaultDeviceInfo device_info;
  memfault_platform_get_device_info(&device_info);


  const int rv = snprintf(url_buffer, MEMFAULT_HTTP_URL_BUFFER_SIZE, "%s://%s" MEMFAULT_HTTP_CHUNKS_API_PREFIX "%s/%s",
                          MEMFAULT_HTTP_GET_SCHEME(), MEMFAULT_HTTP_GET_CHUNKS_API_HOST(), subpath, device_info.device_serial);
  return (rv < MEMFAULT_HTTP_URL_BUFFER_SIZE);
}

sMfltHttpClient *memfault_http_client_create(void) {
  return memfault_platform_http_client_create();
}

static void prv_handle_post_data_response(const sMfltHttpResponse *response, MEMFAULT_UNUSED void *ctx) {
  if (!response) {
    return;  // Request failed
  }
  uint32_t http_status = 0;
  const int rv = memfault_platform_http_response_get_status(response, &http_status);
  if (rv != 0) {
    MEMFAULT_LOG_ERROR("Request failed. No HTTP status: %d", rv);
    return;
  }
  if (http_status < 200 || http_status >= 300) {
    // Redirections are expected to be handled by the platform implementation
    MEMFAULT_LOG_ERROR("Request failed. HTTP Status: %"PRIu32, http_status);
    return;
  }
}

int memfault_http_client_post_data(sMfltHttpClient *client) {
  if (!client) {
    return MemfaultInternalReturnCode_InvalidInput;
  }
  return memfault_platform_http_client_post_data(client, prv_handle_post_data_response, NULL);
}

int memfault_http_client_wait_until_requests_completed(sMfltHttpClient *client, uint32_t timeout_ms) {
  if (!client) {
    return MemfaultInternalReturnCode_InvalidInput;
  }
  return memfault_platform_http_client_wait_until_requests_completed(client, timeout_ms);
}

int memfault_http_client_destroy(sMfltHttpClient *client) {
  if (!client) {
    return MemfaultInternalReturnCode_InvalidInput;
  }
  return memfault_platform_http_client_destroy(client);
}
