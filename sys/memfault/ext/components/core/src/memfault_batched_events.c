//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!

#include "memfault/core/batched_events.h"

#include <string.h>

#include "memfault/core/sdk_assert.h"
#include "memfault/util/cbor.h"


static void prv_fill_header_cb(void *ctx, uint32_t offset, const void *buf, size_t buf_len) {
  uint8_t *header_buf = (uint8_t *)ctx;
  memcpy(&header_buf[offset], buf, buf_len);
}

void memfault_batched_events_build_header(
    size_t num_events, sMemfaultBatchedEventsHeader *header_out) {
  MEMFAULT_SDK_ASSERT(header_out != NULL);

  if (num_events <= 1) {
    header_out->length = 0;
    return;
  }

  // there's multiple events to read. We will add a header to indicate the total count
  sMemfaultCborEncoder encoder;
  memfault_cbor_encoder_init(&encoder, prv_fill_header_cb, header_out->data,
                             sizeof(header_out->data));
  memfault_cbor_encode_array_begin(&encoder, num_events);
  header_out->length = memfault_cbor_encoder_deinit(&encoder);
}
