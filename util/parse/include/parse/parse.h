/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * @file
 * @brief Utility functions for parsing text.
 */

#ifndef H_UTIL_PARSE_
#define H_UTIL_PARSE_

#include <inttypes.h>
#include <stdbool.h>
struct mn_in6_addr;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parses a long long within an imposed range.
 *
 * @param sval                  The string to parse.
 * @param min                   Values less than this are rejected.
 * @param max                   Values greater than this are rejected.
 * @param out_status            Written on completion;
 *                                  0: success;
 *                                  SYS_EINVAL: invalid string or number out of
 *                                      range.
 *
 * @return                      The parsed number on success;
 *                              unspecified on error.
 */
long long
parse_ll_bounds(const char *sval, long long min, long long max,
                int *out_status);

/**
 * Parses an unsigned long long within an imposed range.
 *
 * @param sval                  The string to parse.
 * @param min                   Values less than this are rejected.
 * @param max                   Values greater than this are rejected.
 * @param out_status            Written on completion;
 *                                  0: success;
 *                                  SYS_EINVAL: invalid string or number out of
 *                                      range.
 *
 * @return                      The parsed number on success;
 *                              unspecified on error.
 */
unsigned long long
parse_ull_bounds(const char *sval,
                 unsigned long long min, unsigned long long max,
                 int *out_status);

/**
 * Parses a long long.
 *
 * @param sval                  The string to parse.
 * @param out_status            Written on completion;
 *                                  0: success;
 *                                  SYS_EINVAL: invalid string or number out of
 *                                      range.
 *
 * @return                      The parsed number on success;
 *                              unspecified on error.
 */
long long
parse_ll(const char *sval, int *out_status);

/**
 * Parses an unsigned long long.
 *
 * @param sval                  The string to parse.
 * @param out_status            Written on completion;
 *                                  0: success;
 *                                  SYS_EINVAL: invalid string or number out of
 *                                      range.
 *
 * @return                      The parsed number on success;
 *                              unspecified on error.
 */
unsigned long long
parse_ull(const char *sval, int *out_status);

/**
 * @brief Parses a stream of bytes with the specified delimiter(s) and using
 * the specified base.
 *
 * @param sval                  The string to parse.
 * @param delims                String containing delimiters; each character
 *                                  can act as a delimiter.
 * @param base                  The base to use while parsing each byte string
 *                                  (valid values are: 10, 16, and 0).
 * @param max_len               The maximum number of bytes to write.
 * @param dst                   The destination buffer to write bytes to.
 * @param out_len               Written on success; total number of bytes
 *                                  written to the destination buffer.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on invalid byte stream;
 *                              SYS_ERANGE if result only partially written to
 *                                  buffer due to insufficient space.
 */
int
parse_byte_stream_delim_base(const char *sval, const char *delims, int base,
                             int max_len, uint8_t *dst, int *out_len);

/**
 * @brief Parses a stream of bytes with the specified delimiter(s).
 *
 * Parses a stream of bytes with the specified delimiter(s).  The base of each
 * byte string is inferred from the text (base 16 if prefixed with "0x", base
 * 10 otherwise).
 *
 * @param sval                  The string to parse.
 * @param delims                String containing delimiters; each character
 *                                  can act as a delimiter.
 * @param max_len               The maximum number of bytes to write.
 * @param dst                   The destination buffer to write bytes to.
 * @param out_len               Written on success; total number of bytes
 *                                  written to the destination buffer.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on invalid byte stream;
 *                              SYS_ERANGE if result only partially written to
 *                                  buffer due to insufficient space.
 */
int
parse_byte_stream_delim(const char *sval, const char *delims, int max_len,
                        uint8_t *dst, int *out_len);

/**
 * @brief Parses a stream of bytes using the specified base.
 *
 * @param sval                  The string to parse.
 * @param base                  The base to use while parsing each byte string
 *                                  (valid values are: 10, 16, and 0).
 * @param max_len               The maximum number of bytes to write.
 * @param dst                   The destination buffer to write bytes to.
 * @param out_len               Written on success; total number of bytes
 *                                  written to the destination buffer.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on invalid byte stream;
 *                              SYS_ERANGE if result only partially written to
 *                                  buffer due to insufficient space.
 */
int
parse_byte_stream_base(const char *sval, int base, int max_len,
                       uint8_t *dst, int *out_len);

/**
 * @brief Parses a stream of bytes using ':' or '-' as delimiters.
 *
 * Parses a stream of bytes using ':' or '-' as delimiters.
 * The base of each byte string is inferred from the text (base 16 if prefixed
 * with "0x", base 10 otherwise).
 *
 * @param sval                  The string to parse.
 * @param max_len               The maximum number of bytes to write.
 * @param dst                   The destination buffer to write bytes to.
 * @param out_len               Written on success; total number of bytes
 *                                  written to the destination buffer.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on invalid byte stream;
 *                              SYS_ERANGE if result only partially written to
 *                                  buffer due to insufficient space.
 */
int
parse_byte_stream(const char *sval, int max_len, uint8_t *dst, int *out_len);

/**
 * @brief Parses a stream of bytes using the specified base.
 *
 * Parses a stream of bytes using the specified base.  If the length of the
 * resulting byte array is not equal to the specified length, a SYS_EINVAL
 * failure is reported.
 *
 * @param sval                  The string to parse.
 * @param base                  The base to use while parsing each byte string
 *                                  (valid values are: 10, 16, and 0).
 * @param dst                   The destination buffer to write bytes to.
 * @param len                   The exact number of bytes the result is
 *                                  expected to contain.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on invalid byte stream or if source
 *                                  string specifies an unexpected number of
 *                                  bytes;
 */
int
parse_byte_stream_exact_length_base(const char *sval, int base,
                                    uint8_t *dst, int len);

/**
 * @brief Parses a stream of bytes using ':' or '-' as delimiters.
 *
 * Parses a stream of bytes using ':' or '-' as delimiters.  If the length of
 * the resulting byte array is not equal to the specified length, a SYS_EINVAL
 * failure is reported.  The base of each byte string is inferred from the text
 * (base 16 if prefixed with "0x", base 10 otherwise).
 *
 * @param sval                  The string to parse.
 * @param dst                   The destination buffer to write bytes to.
 * @param len                   The exact number of bytes the result is
 *                                  expected to contain.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on invalid byte stream or if source
 *                                  string specifies an unexpected number of
 *                                  bytes;
 */
int
parse_byte_stream_exact_length(const char *sval, uint8_t *dst, int len);

/**
 * @brief Parses a boolean string.
 *
 * Parses a boolean string.  Valid boolean strings are: "true", "false", and
 * numeric representations of 1 and 0.
 *
 * @param sval                  The string to parse.
 * @param out_status            Written on completion;
 *                                  0: success;
 *                                  SYS_EINVAL: invalid boolean string.
 *
 * @return                      The parsed boolean on success;
 *                              unspecified on error.
 */
bool
parse_bool(const char *sval, int *out_status);

/**
 * @brief Parses an IPv6 network string.  The string 
 *
 * Parses an IPv6 network string.  The string must have the following form:
 * <ipv6-address>/<prefix-length>.
 *
 * @param sval                  The string to parse.
 * @param out_addr              The parsed IPv6 address is written here on
 *                                  success.
 * @param out_prefix_len        The parsed prefix length is written here on
 *                                  success.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL if the IPv6 network string is
 *                                  invalid.
 */
int
parse_ip6_net(const char *sval,
              struct mn_in6_addr *out_addr, uint8_t *out_prefix_len);
#ifdef __cplusplus
}
#endif

#endif
