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

#ifndef H_UTIL_CMDARG_
#define H_UTIL_CMDARG_

#include <inttypes.h>
#include <stdbool.h>
struct mn_in6_addr;

/**
 * @brief Finds the argument with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_val               On success, points to the start of the located
 *                                  value string.
 *
 * @return                      Pointer to argument on success; NULL on
 *                                  failure.
 */
char *cmdarg_find(int argc, char **argv, const char *key, char **out_val);

/**
 * @brief Extracts the argument with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_val               On success, points to the start of the located
 *                                  value string.
 *
 * @return                      Pointer to argument on success; NULL on
 *                                  failure.
 */
char *cmdarg_extract(int argc, char **argv, const char *key, char **out_val);

/**
 * @brief Finds a string value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_s                 On success, points to the start of the located
 *                                  value string.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_str(int argc, char **argv, const char *key, char **out_s);

/**
 * @brief Extracts a string value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_s                 On success, points to the start of the located
 *                                  value string.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_str(int argc, char **argv, const char *key, char **out_s);

/**
 * @brief Finds a long long value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to a long long.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param out_ll                On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_ll(int argc, char **argv, const char *key,
                   long long min, long long max, long long *out_ll);

/**
 * @brief Extracts a long long value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to a long long.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param out_ll                On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_ll(int argc, char **argv, const char *key,
                      long long min, long long max, long long *out_ll);

/**
 * @brief Finds a long long value with the specified key, or yields a default.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to a long long.  Otherwise, the default value is retrieved.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param dflt                  If no matching argument is found, this value is
 *                                  written to the result.
 * @param out_ll                On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_ll_dflt(int argc, char **argv, const char *key,
                        long long min, long long max, long long dflt,
                        long long *out_ll);

/**
 * @brief Extracts a long long value with the specified key, or yields a
 * default.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to a long long.  Otherwise, the default value is retrieved.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param dflt                  If no matching argument is found, this value is
 *                                  written to the result.
 * @param out_ll                On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_ll_dflt(int argc, char **argv, const char *key,
                           long long min, long long max, long long dflt,
                           long long *out_ll);

/**
 * @brief Finds an unsigned long long value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an unsigned long long.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_ull               On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_ull(int argc, char **argv, const char *key,
                    unsigned long long min, unsigned long long max,
                    unsigned long long *out_ull);

/**
 * @brief Extracts an unsigned long long value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an unsigned long long.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param out_ull               On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_ull(int argc, char **argv, const char *key,
                       unsigned long long min, unsigned long long max,
                       unsigned long long *out_ull);

/**
 * @brief Finds an unsigned long long value with the specified key, or yields a
 * default.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an unsigned long long.  Otherwise, the default value is retrieved.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param dflt                  If no matching argument is found, this value is
 *                                  written to the result.
 * @param out_ull               On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_ull_dflt(int argc, char **argv, const char *key,
                         unsigned long long min, unsigned long long max,
                         unsigned long long dflt,
                         unsigned long long *out_ull);

/**
 * @brief Extracts an unsigned long long value with the specified key, or
 * yields a default.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an unsigned long long.  Otherwise, the default value is retrieved.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param dflt                  If no matching argument is found, this value is
 *                                  written to the result.
 * @param out_ull               On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_ull_dflt(int argc, char **argv, const char *key,
                            unsigned long long min, unsigned long long max,
                            unsigned long long dflt,
                            unsigned long long *out_ull);

/**
 * @brief Finds a bool value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * Valid bool strings are: "true", "false", "1", "0".
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_b                 On success, contains the located bool value.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_bool(int argc, char **argv, const char *key, bool *out_b);

/**
 * @brief Finds a bool value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * Valid bool strings are: "true", "false", "1", "0".
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_b                 On success, contains the located bool value.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_bool(int argc, char **argv, const char *key, bool *out_b);

/**
 * @brief Finds a bool value with the specified key, or yields a default.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * Valid bool strings are: "true", "false", "1", "0".  If no match is found,
 * the specified default is retrieved.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param dflt                  If no matching argument is found, this value is
 *                                  written to the result.
 * @param out_b                 On success, contains the located bool value.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_bool_dflt(int argc, char **argv, const char *key, bool dflt,
                          bool *out_b);

/**
 * @brief Extracts a bool value with the specified key, or yields a default.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * Valid bool strings are: "true", "false", "1", "0".  If no match is found,
 * the specified default is retrieved.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param dflt                  If no matching argument is found, this value is
 *                                  written to the result.
 * @param out_b                 On success, contains the located bool value.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_bool_dflt(int argc, char **argv, const char *key,
                             bool dflt, bool *out_b);

/**
 * @brief Finds an int value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an int.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_i                 On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_int(int argc, char **argv, const char *key, int min, int max,
                    int *out_i);

/**
 * @brief Extracts an int value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an int.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param out_i                 On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_int(int argc, char **argv, const char *key,
                       int min, int max, int *out_i);

/**
 * @brief Finds an int value with the specified key, or yields a default.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an int.  Otherwise, the default value is retrieved.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param dflt                  If no matching argument is found, this value is
 *                                  written to the result.
 * @param out_i                 On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_int_dflt(int argc, char **argv, const char *key,
                         int min, int max, int dflt, int *out_i);

/**
 * @brief Extracts an int value with the specified key, or yields a
 * default.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an int.  Otherwise, the default value is retrieved.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param min                   The minimum allowed value (inclusive).
 * @param max                   The maximum allowed value (inclusive).
 * @param dflt                  If no matching argument is found, this value is
 *                                  written to the result.
 * @param out_i                 On success, contains the located number.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_int_dflt(int argc, char **argv, const char *key,
                            int min, int max, int dflt, int *out_i);

/**
 * @brief Finds a byte string value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to a byte array.  Each byte can be in decimal, octal, or hexadecimal.
 * Valid delimiter characters are ':' and '-'.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param max_len               The maximum number of bytes to parse.
 * @param out_val               On success, the parsed byte array gets written
 *                                  to this provided buffer.
 * @param out_len               On success, the actual number of bytes parsed
 *                                  gets written here.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_bytes(int argc, char **argv, const char *key,
                      int max_len, uint8_t *out_val, int *out_len);

/**
 * @brief Extracts a byte string value with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to a byte array.  Each byte can be in decimal, octal, or hexadecimal.
 * Valid delimiter characters are ':' and '-'.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param max_len               The maximum number of bytes to parse.
 * @param out_val               On success, the parsed byte array gets written
 *                                  to this provided buffer.
 * @param out_len               On success, the actual number of bytes parsed
 *                                  gets written here.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_bytes(int argc, char **argv, const char *key,
                         int max_len, uint8_t *out_val, int *out_len);

/**
 * @brief Finds an 8-byte EUI with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an 8-byte array.  The value string must have the following form:
 *
 *     XX:XX:XX:XX:XX:XX:XX:XX
 *
 * where each 'XX' pair is a hexadecimal value.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_eui               On success, the parsed EUI gets written to this
 *                                  provided buffer.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_eui(int argc, char **argv, const char *key, uint8_t *out_eui);

/**
 * @brief Extracts an 8-byte EUI with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an 8-byte array.  The value string must have the following form:
 *
 *     XX:XX:XX:XX:XX:XX:XX:XX
 *
 * where each 'XX' pair is a hexadecimal value.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_eui               On success, the parsed EUI gets written to this
 *                                  provided buffer.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_eui(int argc, char **argv, const char *key,
                       uint8_t *out_eui);

/**
 * @brief Finds an IPv6 address with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an IPv6 address.  The value string must *not* specify a prefix
 * length.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_addr              On success, the parsed IPv6 address gets
 *                                  written here.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_ip6_addr(int argc, char **argv, const char *key,
                         struct mn_in6_addr *out_addr);

/**
 * @brief Extracts an IPv6 address with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an IPv6 address.  The value string must *not* specify a prefix
 * length.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_addr              On success, the parsed IPv6 address gets
 *                                  written here.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_ip6_addr(int argc, char **argv, const char *key,
                            struct mn_in6_addr *out_addr);

/**
 * @brief Finds an IPv6 network with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an IPv6 network.  The value string must specify an address and a
 * prefix length.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_addr              On success, the parsed IPv6 address gets
 *                                  written here.
 * @param out_prefix_len        On success, the parsed prefix length gets
 *                                  written here.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_find_ip6_net(int argc, char **argv, const char *key,
                        struct mn_in6_addr *out_addr,
                        uint8_t *out_prefix_len);

/**
 * @brief Extracts an IPv6 network with the specified key.
 *
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * If the requested key is found, this function attempts to convert the located
 * value to an IPv6 network.  The value string must specify an address and a
 * prefix length.
 *
 * This function modifies the argument list such that this argument won't be
 * found on a subsequent search.
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_addr              On success, the parsed IPv6 address gets
 *                                  written here.
 * @param out_prefix_len        On success, the parsed prefix length gets
 *                                  written here.
 *
 * @return                      0 on success; system error code on failure.
 */
int cmdarg_extract_ip6_net(int argc, char **argv, const char *key,
                           struct mn_in6_addr *out_addr,
                           uint8_t *out_prefix_len);

#endif
