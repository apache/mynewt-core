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

#include <syscfg/syscfg.h>

#if MYNEWT_VAL(HAL_SBRK) && !MYNEWT_VAL(BSP_SIMULATED)

static char *sbrk_base;
static char *sbrk_limit;
static char *brk;

void
_sbrkInit(char *base, char *limit)
{
    sbrk_base = base;
    sbrk_limit = limit;
    brk = base;
}

void *
_sbrk(int incr)
{
    char *prev_brk;
    char *new_brk = brk + incr;

    if (new_brk < sbrk_base || new_brk > sbrk_limit) {
        prev_brk = (char *)-1;
    } else {
        prev_brk = brk;
        brk = new_brk;
    }

    return prev_brk;
}

#endif
