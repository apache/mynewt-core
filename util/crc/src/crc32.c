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


#include <crc/crc32.h>

uint32_t
crc32_init(void)
{
    return 0;
}

uint32_t
crc32_calc(uint32_t val, const void *buf, size_t cnt)
{
    int i, j;
    const uint8_t *b = (const uint8_t *)buf;
    uint32_t byte, crc, mask;

    crc = ~val;
    for (i = 0; i < cnt; ++i) {
        byte = *b++;
        crc = crc ^ byte;
        for (j = 0; j < 8; ++j) {
            mask = -(int32_t)(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}
