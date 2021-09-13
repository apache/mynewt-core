//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Example implementation of platform dependencies on the ESP32 for the Memfault HTTP APIs

#include "memfault/config.h"

#if MEMFAULT_ESP_HTTP_CLIENT_ENABLE

#include "memfault/http/platform/http_client.h"

#include <string.h>

#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "memfault/core/data_packetizer.h"

#include "memfault/core/debug_log.h"
#include "memfault/core/errors.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/debug_log.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/esp_port/core.h"
#include "memfault/esp_port/http_client.h"
#include "memfault/http/http_client.h"
#include "memfault/http/root_certs.h"
#include "memfault/http/utils.h"
#include "memfault/panics/assert.h"

#ifndef MEMFAULT_HTTP_DEBUG
#  define MEMFAULT_HTTP_DEBUG (0)
#endif

#if MEMFAULT_HTTP_DEBUG
static esp_err_t prv_http_event_handler(esp_http_client_event_t *evt) {
  switch(evt->event_id) {
    case HTTP_EVENT_ERROR:
      memfault_platform_log(kMemfaultPlatformLogLevel_Error, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      memfault_platform_log(kMemfaultPlatformLogLevel_Info, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      memfault_platform_log(kMemfaultPlatformLogLevel_Info, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      memfault_platform_log(kMemfaultPlatformLogLevel_Info,
          "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      memfault_platform_log(kMemfaultPlatformLogLevel_Info,
          "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      if (!esp_http_client_is_chunked_response(evt->client)) {
        // Write out data
        // memfault_platform_log(kMemfaultPlatformLogLevel_Info, "%.*s", evt->data_len, (char*)evt->data);
      }

      break;
    case HTTP_EVENT_ON_FINISH:
      memfault_platform_log(kMemfaultPlatformLogLevel_Info, "HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      memfault_platform_log(kMemfaultPlatformLogLevel_Info, "HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}
#endif  // MEMFAULT_HTTP_DEBUG



static int prv_post_chunks(esp_http_client_handle_t client, void *buffer, size_t buf_len) {
  // drain all the chunks we have
  while (1) {
    // NOTE: Ideally we would be able to enable multi packet chunking which would allow a chunk to
    // span multiple calls to memfault_packetizer_get_next(). Unfortunately the esp-idf does not
    // have a POST mechanism that can use a callback so our POST size is limited by the size of the
    // buffer we can allocate.
    size_t read_size = buf_len;

    const bool more_data = memfault_esp_port_get_chunk(buffer, &read_size);
    if (!more_data) {
      break;
    }

    esp_http_client_set_post_field(client, buffer, read_size);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

    esp_err_t err = esp_http_client_perform(client);
    if (ESP_OK != err) {
      return MEMFAULT_PLATFORM_SPECIFIC_ERROR(err);
    }
  }

  return 0;
}

static char s_mflt_base_url_buffer[MEMFAULT_HTTP_URL_BUFFER_SIZE];

sMfltHttpClient *memfault_platform_http_client_create(void) {
  memfault_http_build_url(s_mflt_base_url_buffer, "");
  const esp_http_client_config_t config = {
#if MEMFAULT_HTTP_DEBUG
    .event_handler = prv_http_event_handler,
#endif
    .url = s_mflt_base_url_buffer,
    .cert_pem = g_mflt_http_client_config.disable_tls ? NULL : MEMFAULT_ROOT_CERTS_PEM,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_header(client, MEMFAULT_HTTP_PROJECT_KEY_HEADER, g_mflt_http_client_config.api_key);
  return (sMfltHttpClient *)client;
}

int memfault_platform_http_client_destroy(sMfltHttpClient *_client) {
  esp_http_client_handle_t client = (esp_http_client_handle_t)_client;
  esp_err_t err = esp_http_client_cleanup(client);
  if (err == ESP_OK) {
    return 0;
  }

  return MEMFAULT_PLATFORM_SPECIFIC_ERROR(err);
}

typedef struct MfltHttpResponse {
  uint16_t status;
} sMfltHttpResponse;

int memfault_platform_http_response_get_status(const sMfltHttpResponse *response, uint32_t *status_out) {
  MEMFAULT_ASSERT(response);
  if (status_out) {
    *status_out = response->status;
  }
  return 0;
}

static int prv_build_latest_release_url(char *buf, size_t buf_len) {
  sMemfaultDeviceInfo device_info;
  memfault_platform_get_device_info(&device_info);
  return snprintf(buf, buf_len,
                  "%s://%s/api/v0/releases/latest/url?device_serial=%s&hardware_version=%s&software_type=%s&software_version=%s",
                  MEMFAULT_HTTP_GET_SCHEME(),
                  MEMFAULT_HTTP_GET_DEVICE_API_HOST(),
                  device_info.device_serial,
                  device_info.hardware_version,
                  device_info.software_type,
                  device_info.software_version);
}

static int prv_get_ota_update_url(char **download_url_out) {
  sMfltHttpClient *http_client = memfault_http_client_create();
  char *url = NULL;
  char *download_url = NULL;
  int status_code = -1;

  int rv = -1;
  if (http_client == NULL) {
    return rv;
  }

  // call once with no buffer to figure out space we need to allocate to hold url
  int url_len = prv_build_latest_release_url(NULL, 0);

  const size_t url_buf_len =  url_len + 1 /* for '\0' */;
  url = calloc(1, url_buf_len);
  if (url == NULL) {
    MEMFAULT_LOG_ERROR("Unable to allocate url buffer (%dB)", (int)url_buf_len);
    goto cleanup;
  }

  if (prv_build_latest_release_url(url, url_buf_len) != url_len) {
    goto cleanup;
  }

  esp_http_client_handle_t client = (esp_http_client_handle_t)http_client;

  // NB: For esp-idf versions > v3.3 will set the Host automatically as part
  // of esp_http_client_set_url() so this call isn't strictly necessary.
  //
  //  https://github.com/espressif/esp-idf/commit/a8755055467f3e6ab44dd802f0254ed0281059cc
  //  https://github.com/espressif/esp-idf/commit/d154723a840f04f3c216df576456830c884e7abd
  esp_http_client_set_header(client, "Host", MEMFAULT_HTTP_GET_DEVICE_API_HOST());

  esp_http_client_set_url(client, url);
  esp_http_client_set_method(client, HTTP_METHOD_GET);
  // to keep the parsing simple, we will request the download url as plaintext
  esp_http_client_set_header(client, "Accept", "text/plain");

  const esp_err_t err = esp_http_client_open(client, 0);
  if (ESP_OK != err) {
    rv = MEMFAULT_PLATFORM_SPECIFIC_ERROR(err);
    goto cleanup;
  }

  const int content_length = esp_http_client_fetch_headers(client);
  if (content_length < 0) {
    rv = MEMFAULT_PLATFORM_SPECIFIC_ERROR(content_length);
    goto cleanup;
  }

  download_url = calloc(1, content_length + 1);
  if (download_url == NULL) {
    MEMFAULT_LOG_ERROR("Unable to allocate download url buffer (%dB)", (int)download_url);
    goto cleanup;
  }

  int bytes_read = 0;
  while (bytes_read != content_length) {
    int len = esp_http_client_read(client, &download_url[bytes_read], content_length - bytes_read);
    if (len < 0) {
      rv = MEMFAULT_PLATFORM_SPECIFIC_ERROR(len);
      goto cleanup;
    }
    bytes_read += len;
  }

  status_code = esp_http_client_get_status_code(client);
  if (status_code != 200 && status_code != 204) {
    MEMFAULT_LOG_ERROR("OTA Query Failure. Status Code: %d", status_code);
    MEMFAULT_LOG_INFO("Request Body: %s", download_url);
    goto cleanup;
  }

  // Lookup to see if a release is available was succesful!
  rv = 0;

cleanup:
  free(url);
  memfault_http_client_destroy(http_client);

  if (status_code == 200) {
    *download_url_out = download_url;
  } else {
    // on failure or if no update is available (204) there is no download url to return
    free(download_url);
    *download_url_out = NULL;
  }
  return rv;
}

int memfault_esp_port_ota_update(const sMemfaultOtaUpdateHandler *handler) {
  if ((handler == NULL) || (handler->handle_update_available == NULL) ||
      (handler->handle_download_complete == NULL)) {
    return MemfaultInternalReturnCode_InvalidInput;
  }

  char *download_url = NULL;
  int rv = prv_get_ota_update_url(&download_url);

  if ((rv != 0) || (download_url == NULL)) {
    return rv;
  }

  const bool perform_ota = handler->handle_update_available(handler->user_ctx);
  if (!perform_ota) {
    goto cleanup;
  }

  esp_http_client_config_t config = {
    .url = download_url,
    .cert_pem = MEMFAULT_ROOT_CERTS_PEM,
  };

  const esp_err_t err = esp_https_ota(&config);
  if (err != ESP_OK) {
    rv = MEMFAULT_PLATFORM_SPECIFIC_ERROR(err);
    goto cleanup;
  }

  const bool success = handler->handle_download_complete(handler->user_ctx);
  rv = success ? 1 : -1;

cleanup:
  free(download_url);
  return rv;
}

int memfault_platform_http_client_post_data(
    sMfltHttpClient *_client, MemfaultHttpClientResponseCallback callback, void *ctx) {
  if (!memfault_esp_port_data_available()) {
    return 0; // no new chunks to send
  }

  MEMFAULT_LOG_DEBUG("Posting Memfault Data");

  size_t buffer_size = 0;
  void *buffer = memfault_http_client_allocate_chunk_buffer(&buffer_size);
  if (buffer == NULL || buffer_size == 0) {
    MEMFAULT_LOG_ERROR("Unable to allocate POST buffer");
    return -1;
  }

  esp_http_client_handle_t client = (esp_http_client_handle_t)_client;
  char url[MEMFAULT_HTTP_URL_BUFFER_SIZE];
  memfault_http_build_url(url, MEMFAULT_HTTP_CHUNKS_API_SUBPATH);
  esp_http_client_set_url(client, url);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Accept", "application/json");

  int rv = prv_post_chunks(client, buffer, buffer_size);
  memfault_http_client_release_chunk_buffer(buffer);
  if (rv != 0) {
    MEMFAULT_LOG_ERROR("%s failed: %d", __func__, (int)rv);
    return rv;
  }

  const sMfltHttpResponse response = {
    .status = (uint32_t)esp_http_client_get_status_code(client),
  };
  if (callback) {
    callback(&response, ctx);
  }
  MEMFAULT_LOG_DEBUG("Posting Memfault Data Complete!");
  return 0;
}

int memfault_platform_http_client_wait_until_requests_completed(
    sMfltHttpClient *client, uint32_t timeout_ms) {
  // No-op because memfault_platform_http_client_post_data() is synchronous
  return 0;
}

bool memfault_esp_port_wifi_connected(void) {
  wifi_ap_record_t ap_info;
  const bool connected = esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
  return connected;
}

// Similar to memfault_platform_http_client_post_data() but just posts
// whatever is pending, if anything.
int memfault_esp_port_http_client_post_data(void) {
  if (!memfault_esp_port_wifi_connected()) {
    MEMFAULT_LOG_INFO("%s: Wifi unavailable", __func__);
    return -1;
  }

  // Check for data available first as nothing else matters if not.
  if (!memfault_esp_port_data_available()) {
    return 0;
  }

  sMfltHttpClient *http_client = memfault_http_client_create();
  if (!http_client) {
    MEMFAULT_LOG_ERROR("Failed to create HTTP client");
    return MemfaultInternalReturnCode_Error;
  }
  const eMfltPostDataStatus rv =
      (eMfltPostDataStatus)memfault_http_client_post_data(http_client);
  if (rv == kMfltPostDataStatus_NoDataFound) {
    MEMFAULT_LOG_INFO("No new data found");
  } else {
    MEMFAULT_LOG_INFO("Result: %d", (int)rv);
  }
  const uint32_t timeout_ms = 30 * 1000;
  memfault_http_client_wait_until_requests_completed(http_client, timeout_ms);
  memfault_http_client_destroy(http_client);
  return (int)rv;
}

#endif /* MEMFAULT_ESP_HTTP_CLIENT_ENABLE */
