#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Dependency functions required in order to send data directly to Memfault's
//! servers via HTTPS, using memfault_http_client_post_data().

#include <inttypes.h>
#include <stddef.h>

#include "memfault/http/http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Creates a new HTTP client. A client can be reused across requests.
//! This way, the cost of connection set up to the server will be shared with multiple requests.
//!
//! @return The newly created client or NULL in case of an error.
//! @note Make sure to call memfault_platform_http_client_destroy() to close and clean up resources.
sMfltHttpClient *memfault_platform_http_client_create(void);

//! Forward declaration of a HTTP response.
typedef struct MfltHttpResponse sMfltHttpResponse;

//! HTTP response callback function pointer type.
//! @param response The response object or NULL if the request failed (i.e. due to a networking error, timeout, etc.).
//! After returning from the callback, this pointer is no longer valid.
//! @param ctx The user pointer that was passed when performing the request.
typedef void (*MemfaultHttpClientResponseCallback)(const sMfltHttpResponse *response, void *ctx);

//! Get the HTTP status code of a response object.
//! @param response The response object. Guaranteed to be non-NULL.
//! @param[out] status_out Pointer to variable to which to write the HTTP status code.
//! @return 0 on success, else error code
int memfault_platform_http_response_get_status(const sMfltHttpResponse *response, uint32_t *status_out);

//! Posts data that is pending transmission to Memfault's services over HTTPS to the API path defined by
//! MEMFAULT_HTTP_CHUNKS_API_SUBPATH. The implementation is expected to set the project key header (see
//! MEMFAULT_HTTP_PROJECT_KEY_HEADER) as well as the "Content-Type: application/octet-stream" header.
//! See https://docs.memfault.com/ for HTTP API documentation.
//! HTTP redirects are expected to be handled by the implementation -- in other words, when receiving 3xx responses,
//! be called should only be called after following the redirection(s).
//!
//! @param client The client to use to post the request. Guaranteed to be non-NULL.
//! @param callback The callback to call with the response object. This callback MUST ALWAYS be called when this
//! function returned kMfltPostDataStatus_Success!
//! @param ctx Pointer to user data that is expected to be passed into the callback.
//! @return 0 on success, otherwise an error code
int memfault_platform_http_client_post_data(sMfltHttpClient *client,
                                            MemfaultHttpClientResponseCallback callback, void *ctx);

//! Waits until pending requests have been completed.
//! @param client The http client. Guaranteed to be non-NULL.
//! @return 0 on success.
int memfault_platform_http_client_wait_until_requests_completed(
    sMfltHttpClient *client, uint32_t timeout_ms);

//! Destroys a HTTP client that was previously created using memfault_platform_http_client_create().
//! @param client The client to destroy. Guaranteed to be non-NULL.
//! @return 0 iff the client was destroyed successfully.
int memfault_platform_http_client_destroy(sMfltHttpClient *client);

#ifdef __cplusplus
}
#endif
