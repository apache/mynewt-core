#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! A collection of HTTP utilities *solely* for interacting with the Memfault REST API including
//!  - An API for generating HTTP Requests to Memfault REST API
//!  - An API for incrementally parsing HTTP Response Data [1]
//!
//! @note The expectation is that a typical application making use of HTTP will have a HTTP client
//! and parsing implementation already available to leverage.  This module is provided as a
//! reference and convenience implementation for very minimal environments.
//!
//! [1]: For more info on embedded-C HTTP parsing options, the following commit
//! in the Zephyr RTOS is a good starting point: https://mflt.io/35RWWwp

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Writer invoked by calls to "memfault_http_start_chunk_post"
//!
//! For example, this would be where a user of the API would make a call to send() to push data
//! over a socket
typedef bool(*MfltHttpClientSendCb)(const void *data, size_t data_len, void *ctx);

//! Builds the HTTP 'Request-Line' and Headers for a POST to the Memfault Chunk Endpoint
//!
//! @note Upon completion of this call, a caller then needs to send the raw data received from
//! memfault_packetizer_get_next() out over the active connection.
//!
//! @param callback The callback invoked to send post request data.
//! @param ctx A user specific context that gets passed to 'callback' invocations.
//! @param content_body_length The length of the chunk payload to be sent. This value
//!  will be populated in the HTTP "Content-Length" header.
//!
//! @return true if the post was successful, false otherwise
bool memfault_http_start_chunk_post(
    MfltHttpClientSendCb callback, void *ctx, size_t content_body_length);

//! Builds the HTTP GET request to query the Memfault cloud to see if a new OTA Payload is available
//!
//! For more details about release management and OTA payloads in general, check out:
//!  https://mflt.io/release-mgmt
//!
//! @param callback The callback invoked to send post request data.
//! @param ctx A user specific context that gets passed to 'callback' invocations.
//!
//! @return true if sending the request was successful, false otherwise. On success,
//!  the request response body can be read where the following HTTP Status Codes
//!  are expected:
//!    200: A new firmware (OTA Payload) is available and the response contains a url for it
//!    204: No new firmware is available
//!    4xx, 5xx: Error
bool memfault_http_get_latest_ota_payload_url(MfltHttpClientSendCb write_callback, void *ctx);

//! Builds the HTTP GET request to download a Firmware OTA Payload
//!
//! @param callback The callback invoked to send post request data.
//! @param ctx A user specific context that gets passed to 'callback' invocations.
//! @param url The ota payload url returned in the body of the HTTP request constructed with
//!   memfault_http_get_latest_ota_payload_url().
//! @param url_len The string length of the url param
//!
//! @return true if sending the request was successful, false otherwise. On success,
//!   the request response body can be read where the Content-Length will contain the size
//!   of the OTA payload and the message-body will be the OTA Payload that was uploaded via
//!   the Memfault UI
bool memfault_http_get_ota_payload(MfltHttpClientSendCb write_callback, void *ctx,
                                   const char *url, size_t url_len);

typedef enum MfltHttpParseStatus {
  kMfltHttpParseStatus_Ok = 0,
  MfltHttpParseStatus_ParseStatusLineError,
  MfltHttpParseStatus_ParseHeaderError,
  MfltHttpParseStatus_HeaderTooLongError,
} eMfltHttpParseStatus;

typedef enum MfltHttpParsePhase {
  kMfltHttpParsePhase_ExpectingStatusLine = 0,
  kMfltHttpParsePhase_ExpectingHeader,
  kMfltHttpParsePhase_ExpectingBody,
} eMfltHttpParsePhase;

typedef struct {
  //! true if a error occurred trying to parse the response
  eMfltHttpParseStatus parse_error;
  //! populated with the status code returned as part of the response
  int http_status_code;
  //! Pointer to http_body, may be truncated but always NULL terminated.
  //! This should only be used for debug purposes
  const char *http_body;
  //! The number of bytes processed by the last invocation of
  //! "memfault_http_parse_response" or "memfault_http_parse_response_header"
  int data_bytes_processed;
  //! Populated with the Content-Length found in the HTTP Response Header
  //! Valid upon parsing completion if no parse_error was returned
  int content_length;

  // For internal use only
  eMfltHttpParsePhase phase;
  int content_received;
  size_t line_len;
  char line_buf[128];
} sMemfaultHttpResponseContext;


//! A *minimal* HTTP response parser for Memfault API calls
//!
//! @param ctx The context to be used while a parsing is in progress. It's
//!  expected that when a user first calls this function the context will be
//!  zero initialized.
//! @param data The data to parse
//! @param data_len The length of the data to parse
//! @return True if parsing completed or false if more data is needed for the response
//!   Upon completion the 'parse_error' & 'http_status_code' fields can be checked
//!   within the 'ctx' for the results
bool memfault_http_parse_response(
    sMemfaultHttpResponseContext *ctx, const void *data, size_t data_len);

//! Same as memfault_http_parse_response but only parses the response header
//!
//! This API can be useful when the message body contains information further action
//! is taken on (i.e an OTA Payload)
//!
//! @note Specifically, all data up to the "message-body" is consumed by the parser
//!    Response      = Status-Line
//!                    *(( general-header
//!                     | response-header
//!                     | entity-header ) CRLF)
//!                    CRLF
//!                    [ message-body ]          ; **NOT Consumed**
bool memfault_http_parse_response_header(
    sMemfaultHttpResponseContext *ctx, const void *data, size_t data_len);

typedef enum {
  kMemfaultUriScheme_Unrecognized = 0,

  kMemfaultUriScheme_Http,
  kMemfaultUriScheme_Https,
} eMemfaultUriScheme;

typedef struct {
  //! Protocol detected (Either HTTPS or HTTP)
  eMemfaultUriScheme scheme;

  //! The 'host' component of the uri:
  //!   https://tools.ietf.org/html/rfc3986#section-3.2.2
  //! @note: NOT nul-terminated
  const void *host;
  size_t host_len;

  //! Port to use for connection
  //! @note if no port is specified in URI, default for scheme will be
  //! populated (i.e 80 for http & 443 for https)
  uint32_t port;

  //! The 'path' component of the uri:
  //!   https://tools.ietf.org/html/rfc3986#section-3.3
  //! @note: Path will be NULL when empty in URI. Like host, when
  //!  path is populated, it is not terminated with a nul character.
  const void *path;
  size_t path_len;
} sMemfaultUriInfo;

//! A *minimal* parser for HTTP/HTTPS URIs
//!
//! @note API performs no copies or mallocs. Instead uri_info_out contains
//!  pointers back to the original uri provided and lengths of fields parsed.
//!
//! @note For more details about URIs in general, check out the RFC
//!   https://tools.ietf.org/html/rfc3986#section-1.1.3
//! @note this function parses URIs with 'scheme's of "http" or "https"
//!   URI         = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
//!
//! @param uri The URI to try and parse.
//! @param uri_len The string length of the uri
//! @param[out] uri_info_out On successful parse, populated with various pieces of information
//!   See 'sMemfaultUriInfo' for more details.
//!
//! @return true if parse was successful and uri_info_out is populated, false  otherwise
bool memfault_http_parse_uri(const char *uri, size_t uri_len, sMemfaultUriInfo *uri_info_out);

#ifdef __cplusplus
}
#endif
