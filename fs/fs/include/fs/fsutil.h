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

#ifndef H_FSUTIL_
#define H_FSUTIL_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

int fsutil_read_file(const char *path, uint32_t offset, uint32_t len,
                     void *dst, uint32_t *out_len);
int fsutil_write_file(const char *path, const void *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif
