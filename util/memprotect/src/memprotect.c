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
#include "os/mynewt.h"
#include "memprotect/memprotect.h"

extern int __mynewt_protected_start, __mynewt_protected_end;

/**
 * Clear out sensitive memory area
 */
void
memprotect_clear_data(void)
{
    uint32_t * ptr = (uint32_t *) &__mynewt_protected_start, idx;

    for (idx = 0; i < (uint32_t *) &__mynewt_protected_end; idx+= sizeof(* ptr)) {
        ptr[idx] = MYNEWT_VAL(MEMPROTECT_PATTERN);
    }
}