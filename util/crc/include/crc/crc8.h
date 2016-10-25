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
 * CRC8-CCITT, with normal polynomial; 0x07.
 */

#ifndef _UTIL_CRC8_H_
#define _UTIL_CRC8_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t crc8_init(void);
uint8_t crc8_calc(uint8_t val, void *buf, int cnt);

#ifdef __cplusplus
}
#endif

#endif
