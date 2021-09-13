//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/util/base64.h"

#include <stddef.h>
#include <stdint.h>

static char prv_get_char_from_word(uint32_t word, int offset) {
  const char *base64_table =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const uint8_t base64_mask = 0x3f; // one char per 6 bits
  return base64_table[(word >> (offset * 6)) & base64_mask];
}

void memfault_base64_encode(const void *buf, size_t buf_len, void *base64_out) {
  const uint8_t *bin_inp = (const uint8_t *)buf;
  char *out_bufp = (char *)base64_out;

  int curr_idx = 0;

  for (size_t bin_idx = 0; bin_idx < buf_len;  bin_idx += 3) {
    const uint32_t byte0 = bin_inp[bin_idx];
    const uint32_t byte1 = ((bin_idx + 1) < buf_len) ? bin_inp[bin_idx + 1] : 0;
    const uint32_t byte2 = ((bin_idx + 2) < buf_len) ? bin_inp[bin_idx + 2] : 0;
    const uint32_t triple = (byte0 << 16) + (byte1 << 8) + byte2;

    out_bufp[curr_idx++] = prv_get_char_from_word(triple, 3);
    out_bufp[curr_idx++] = prv_get_char_from_word(triple, 2);
    out_bufp[curr_idx++] = ((bin_idx + 1) < buf_len) ? prv_get_char_from_word(triple, 1) : '=';
    out_bufp[curr_idx++] = ((bin_idx + 2) < buf_len) ? prv_get_char_from_word(triple, 0) : '=';
  }
}
