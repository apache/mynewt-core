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

#ifndef H_SYSINIT_
#define H_SYSINIT_

#include "syscfg/syscfg.h"

typedef void sysinit_panic_fn(const char *file, int line);

#if !MYNEWT_VAL(SYSINIT_PANIC_FN)
#define SYSINIT_PANIC() assert(0)
#else
#define SYSINIT_PANIC() MYNEWT_VAL(SYSINIT_PANIC_FN)(__FILE__, __LINE__)
#endif

#define SYSINIT_PANIC_ASSERT(rc) do \
{                                   \
    if (!(rc)) {                    \
        SYSINIT_PANIC();            \
    }                               \
} while (0)

void sysinit(void);

#endif
