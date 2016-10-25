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

#ifndef __COREDUMP_H__
#define __COREDUMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

#define COREDUMP_MAGIC              0x690c47c3

/*
 * Coredump TLV types.
 */
#define COREDUMP_TLV_IMAGE          1   /* SHA256 of image creating this */
#define COREDUMP_TLV_MEM            2   /* Memory dump */
#define COREDUMP_TLV_REGS           3   /* CPU registers */

struct coredump_tlv {
    uint8_t ct_type;
    uint8_t _pad;
    uint16_t ct_len;
    uint32_t ct_off;
};

/*
 * Corefile header.  All fields are in little endian byte order.
 */
struct coredump_header {
    uint32_t ch_magic;
    uint32_t ch_size;                   /* Size of everything */
};

void coredump_dump(void *regs, int regs_sz);

/*
 * Set this to non-zero to prevent coredump from taking place.
 */
extern uint8_t coredump_disabled;

#ifdef __cplusplus
}
#endif

#endif
