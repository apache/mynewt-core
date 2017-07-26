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

#ifndef H_UTIL_PARSE_
#define H_UTIL_PARSE_

long long
parse_ll_bounds(const char *sval, long long min, long long max,
                int *out_status);

unsigned long long
parse_ull_bounds(const char *sval,
                 unsigned long long min, unsigned long long max,
                 int *out_status);
long long
parse_ll(const char *sval, int *out_status);

unsigned long long
parse_ull(const char *sval, int *out_status);

int
parse_byte_stream_delim(const char *sval, const char *delims, int max_len,
                        uint8_t *dst, int *out_len);

int
parse_byte_stream(const char *sval, int max_len, uint8_t *dst, int *out_len);

int
parse_byte_stream_exact_length(const char *sval, uint8_t *dst, int len);

#endif
