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

#ifndef PARSE_ARG_H
#define PARSE_ARG_H

#include <inttypes.h>
#include <stdbool.h>
#include <syscfg/syscfg.h>

#if MYNEWT_VAL(BLE_HOST)
#include <nimble/ble.h>
#include <host/ble_uuid.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup parse_arg Utility functions for parsing command line arguments
 * @{
 */

/*
 * @file
 * @brief Utility functions for parsing command line arguments.
 */

/**
 * Initialize arguments parser. This function should be called every time
 * new line arguments are provided. Follows standard main functions convention
 * of argc and argv parameters.
 *
 * @param argc        Number of a argv pointers.
 * @param argv        Array of parameters string pointers.
 *
 * @return            Zero on success;
 *                    Non-zero on error.
 */
int parse_arg_init(int argc, char **argv);

int parse_arg_find_idx(const char *key);
char *parse_arg_peek(const char *key);
char *parse_arg_extract(const char *key);

/**
 * @brief Used to declare key-value pairs for parsing parameters.
 *
 */
struct parse_arg_kv_pair {
    /** Key name for parameter */
    const char *key;

    /** Value for specified key */
    int val;
};

/**
 * Parses a specified parameter as a key within an optional default value
 * (depends on function variant). Specified parameter's key is converted to
 * value based on provided key-value array. This is useful for "enum like"
 * parameters.
 *
 * @param name        The parameter name.
 * @param kvs         Array of parse_arg_kv_pair, terminated by NULL key.
 * @param dflt        Value used if parameter was not provided.
 * @param status      Written on completion;
 *                      0: success;
 *                      ENOENT: parameter with specified name not found
 *                      EINVAL: invalid string or number out of range.
 *
 * @return            The parsed key value on success;
 *                    unspecified on error.
 */

int parse_arg_kv(const char *name, const struct parse_arg_kv_pair *kvs, int *status);
int parse_arg_kv_dflt(const char *name, const struct parse_arg_kv_pair *kvs, int def_val,
                      int *status);

/**
 * Parses a specified parameter as a long value within an optional imposed range
 * and default value (depends on function variant).
 *
 * @param name        The parameter name.
 * @param min         Values less than this are rejected.
 * @param max         Values greater than this are rejected.
 * @param dflt        Value used if parameter was not provided.
 * @param status      Written on completion;
 *                      0: success;
 *                      ENOENT: parameter with specified name not found
 *                      EINVAL: invalid string or number out of range.
 *
 * @return            The parsed number on success;
 *                    unspecified on error.
 */
long parse_arg_long(const char *name, int *status);
long parse_arg_long_dflt(const char *name, long dflt, int *status);
long parse_arg_long_bounds(const char *name, long min, long max, int *status);
long parse_arg_long_bounds_dflt(const char *name, long min, long max,
                                long dflt, int *status);

/**
 * Parses a specified parameter as a boolean value within an optional default
 * value (depends on function variant).
 *
 * @param name        The parameter name.
 * @param dflt        Value used if parameter was not provided.
 * @param status      Written on completion;
 *                      0: success;
 *                      ENOENT: parameter with specified name not found
 *                      EINVAL: invalid string or number out of range.
 *
 * @return            The parsed boolean on success;
 *                    unspecified on error.
 */
bool parse_arg_bool(const char *name, int *status);
bool parse_arg_bool_dflt(const char *name, bool dflt, int *status);

/**
 * Parses a specified parameter as a uint8_t value within an optional imposed
 * range and default value (depends on function variant).
 *
 * @param name        The parameter name.
 * @param min         Values less than this are rejected.
 * @param max         Values greater than this are rejected.
 * @param dflt        Value used if parameter was not provided.
 * @param status      Written on completion;
 *                      0: success;
 *                      ENOENT: parameter with specified name not found
 *                      EINVAL: invalid string or number out of range.
 *
 * @return            The parsed number on success;
 *                    unspecified on error.
 */
uint8_t parse_arg_uint8(const char *name, int *status);
uint8_t parse_arg_uint8_dflt(const char *name, uint8_t dflt, int *status);
uint8_t parse_arg_uint8_bounds(const char *name, uint8_t min, uint8_t max, int *status);
uint8_t parse_arg_uint8_bounds_dflt(const char *name, uint8_t min, uint8_t max,
                                    uint8_t dflt, int *status);

/**
 * Parses a specified parameter as a uint16_t value within an optional imposed
 * range and default value (depends on function variant).
 *
 * @param name        The parameter name.
 * @param min         Values less than this are rejected.
 * @param max         Values greater than this are rejected.
 * @param dflt        Value used if parameter was not provided.
 * @param status      Written on completion;
 *                      0: success;
 *                      ENOENT: parameter with specified name not found
 *                      EINVAL: invalid string or number out of range.
 *
 * @return            The parsed number on success;
 *                    unspecified on error.
 */
uint16_t parse_arg_uint16(const char *name, int *status);
uint16_t parse_arg_uint16_dflt(const char *name, uint16_t dflt, int *status);
uint16_t parse_arg_uint16_bounds(const char *name, uint16_t min, uint16_t max, int *status);
uint16_t parse_arg_uint16_bounds_dflt(const char *name, uint16_t min, uint16_t max,
                                      uint16_t dflt, int *status);

/**
 * Parses a specified parameter as a uint32_t value within an optional imposed
 * range and default value (depends on function variant).
 *
 * @param name        The parameter name.
 * @param min         Values less than this are rejected.
 * @param max         Values greater than this are rejected.
 * @param dflt        Value used if parameter was not provided.
 * @param status      Written on completion;
 *                      0: success;
 *                      ENOENT: parameter with specified name not found
 *                      EINVAL: invalid string or number out of range.
 *
 * @return            The parsed number on success;
 *                    unspecified on error.
 */
uint32_t parse_arg_uint32(const char *name, int *status);
uint32_t parse_arg_uint32_dflt(const char *name, uint32_t dflt, int *status);
uint32_t parse_arg_uint32_bounds(const char *name, uint32_t min, uint32_t max, int *status);
uint32_t parse_arg_uint32_bounds_dflt(const char *name, uint32_t min, uint32_t max,
                                      uint32_t dflt, int *status);

/**
 * Parses a specified parameter as a uint64_t value within an optional imposed
 * range and default value (depends on function variant).
 *
 * @param name        The parameter name.
 * @param min         Values less than this are rejected.
 * @param max         Values greater than this are rejected.
 * @param dflt        Value used if parameter was not provided.
 * @param status      Written on completion;
 *                      0: success;
 *                      ENOENT: parameter with specified name not found
 *                      EINVAL: invalid string or number out of range.
 *
 * @return            The parsed number on success;
 *                    unspecified on error.
 */
uint64_t parse_arg_uint64(const char *name, int *status);
uint64_t parse_arg_uint64_dflt(const char *name, uint64_t dflt, int *status);
uint64_t parse_arg_uint64_bounds(const char *name, uint64_t min,
                                 uint64_t max, int *status);
uint64_t parse_arg_uint64_bounds_dflt(const char *name, uint64_t min,
                                      uint64_t max, uint64_t dflt, int *status);

/**
 * Parses a specified parameter as byte stream with specified delimiters and
 * length. parse_arg_byte_stream and parse_arg_byte_stream_exact_length are
 * simple wrappers on top of parse_arg_byte_stream_custom with hardcoded
 * delimiters ":-".
 *
 * @param name           The parameter name.
 * @param delims         Delimiters characters used for parsing, NULL terminated.
 * @param dst_size       Size of destination buffer.
 * @param dst            Destination buffer.
 * @param expected_size  Expected parsed size. If non-zero 0 function fails if
 *                       parsed data is not matching length provided.
 * @param out_size       Number of bytes written to destination buffer. Can be
 *                       NULL.
 *
 * @return            Zero on success;
 *                    Non-zero on error.
 */
int parse_arg_byte_stream_custom(const char *name, const char *delims,
                                 unsigned int dst_size, uint8_t *dst,
                                 unsigned int expected_size, unsigned int *out_size);
int parse_arg_byte_stream(const char *name, unsigned int dst_size, uint8_t *dst,
                          unsigned int *out_size);
int parse_arg_byte_stream_exact_length(const char *name, uint8_t *dst,
                                       unsigned int dst_size);

/**
 * Parses a specified parameter as a "time" value with specified step. This is
 * useful for cases where human readable values are converted to time units.
 * eg Bluetooth slots or intervals.
 *
 * @param name        The parameter name.
 * @param step_us     Time step value in microseconds.
 * @param dflt        Value used if parameter was not provided.
 * @param status      Written on completion;
 *                      0: success;
 *                      ENOENT: parameter with specified name not found
 *                      EINVAL: invalid string or number out of range.
 *
 * @return            The parsed number on success;
 *                    unspecified on error.
 */
uint32_t parse_arg_time_dflt(const char *name, unsigned int step_us, uint32_t dflt, int *status);

/**
 * Parses a specified parameter as a MAC address.
 *
 * @param name        The parameter name.
 * @param addr        Parsed MAC address.

 * @return            Zero on success;
 *                    Non-zero on error.
 */
int parse_arg_mac_addr(const char *name, uint8_t addr[6]);

#if MYNEWT_VAL(BLE_HOST)
/**
 * Parses a specified parameter as a Bluetooth address.
 *
 * @param name        The parameter name.
 * @param addr        Parsed Bluetooth address.

 * @return            Zero on success;
 *                    Non-zero on error.
 */
int parse_arg_ble_addr(const char *name, ble_addr_t *addr);

/**
 * Parses a specified parameter as a Bluetooth UUID.
 *
 * @param name        The parameter name.
 * @param uuid        Parsed Bluetooth UUID.

 * @return            Zero on success;
 *                    Non-zero on error.
 */
int parse_arg_ble_uuid(const char *name, ble_uuid_any_t *uuid);
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif
