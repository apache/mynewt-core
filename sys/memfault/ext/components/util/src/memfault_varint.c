//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/util/varint.h"

size_t memfault_encode_varint_u32(uint32_t value, void *buf) {
  uint8_t *res = buf;

  // any value under 128 just looks the same as if it was packed in a uint8_t
  while (value >= 128) {
    *res++ = 0x80 | (value & 0x7f);
    value >>= 7;
  }
  *res++ = value & 0xff;
  return (size_t)(res - (uint8_t *)buf);
}

size_t memfault_encode_varint_si32(int32_t value, void *buf) {
  // A representation that maps negative numbers onto odd positive numbers and
  // positive numbers onto positive even numbers. Some example conversions follow:
  //  0 -> 0
  // -1 -> 1
  //  1 -> 2
  // -2 -> 3
  uint32_t u32_repr = ((uint32_t)value << 1) ^ (uint32_t)(value >> 31);
  return memfault_encode_varint_u32(u32_repr, buf);
}
