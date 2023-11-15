/**
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
 * In SAM D20 / SAM D21 / SAM R21 devices, each device has a unique
 * 128-bit serial number which is a concatenation of four 32-bit words
 * contained at the following addresses:
 *
 * Word 0: 0x0080A00C
 * Word 1: 0x0080A040
 * Word 2: 0x0080A044
 * Word 3: 0x0080A048
 */
#include <inttypes.h>
#include <string.h>

#include "hal/hal_bsp.h"

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

int
hal_bsp_hw_id_len(void)
{
    return 16;
}

/*
 * This can be used as the unique hardware identifier for the platform, as
 * it's supposed to be unique for this particular MCU.
 */
int
hal_bsp_hw_id(uint8_t *id, int max_len)
{
    int len, cnt;

    cnt = min(sizeof(uint32_t), max_len);
    memcpy(id, (void *)0x0080A00C, cnt);
    len = cnt;

    cnt = min(3 * sizeof(uint32_t), max_len - len);
    memcpy(id + len, (void *)0x0080A040, cnt);

    return len + cnt;
}
