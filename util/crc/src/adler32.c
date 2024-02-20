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


#include <stdint.h>
#include <crc/adler32.h>

uint32_t
adler32_init(void)
{
    return 1;
}

#define MOD_ADLER 65521

uint32_t
adler32_calc(uint32_t val, const void *buf, size_t cnt)
{
    const uint8_t *data = (const uint8_t *)buf;
    uint32_t a = val & 0xFFFF;
    uint32_t b = val >> 16;

    while (cnt) {
        unsigned tlen = cnt > 5550 ? 5550 : cnt;
        cnt -= tlen;
        do {
            a += *data++;
            b += a;
        } while (--tlen);
        a = (a & 0xffff) + (a >> 16) * (65536 - MOD_ADLER);
        b = (b & 0xffff) + (b >> 16) * (65536 - MOD_ADLER);
    }
    /* It can be shown that a <= 0x1013a here, so a single subtract will do. */
    if (a >= MOD_ADLER)
        a -= MOD_ADLER;
    /* It can be shown that b can reach 0xffef1 here. */
    b = (b & 0xffff) + (b >> 16) * (65536-MOD_ADLER);
    if (b >= MOD_ADLER)
        b -= MOD_ADLER;
    return (b << 16) | a;
}
