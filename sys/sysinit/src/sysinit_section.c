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

#include <stddef.h>
#include <assert.h>
#include "sysinit_priv.h"

#if __APPLE__

#include <mach-o/getsect.h>

#elif BSP_native && __GNUC__

/* Non-Apple sim (Linux).  gcc automatically creates these symbols at the start
 * and end of each section.
 */
extern const struct sysinit_entry __start_sysinit;
extern const struct sysinit_entry __stop_sysinit;

#else

/* These symbols are specified in the linker script. */
extern const struct sysinit_entry __sysinit_start__;
extern const struct sysinit_entry __sysinit_end__;

#endif

void
sysinit_section_bounds(const struct sysinit_entry **out_start,
                       const struct sysinit_entry **out_end)
{
#if __APPLE__
    unsigned long secsize;
    char *secstart;

    secstart = getsectdata("__TEXT", "sysinit", &secsize);
    assert(secstart != NULL);

    *out_start = (void *)secstart;
    *out_end = (void *)(secstart + secsize);
#elif BSP_native
    *out_start = &__start_sysinit;
    *out_end = &__stop_sysinit;
#else
    *out_start = &__sysinit_start__;
    *out_end = &__sysinit_end__;
#endif
}
