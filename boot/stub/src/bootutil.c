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

#include <defs/error.h>
#include "bootutil/bootutil.h"

int boot_current_slot;

int boot_swap_type(void)
{
    return BOOT_SWAP_TYPE_NONE;
}

int boot_set_pending(int permanent)
{
    return SYS_ENOTSUP;
}

int boot_set_confirmed(void)
{
    return SYS_ENOTSUP;
}

int
split_go(int loader_slot, int split_slot, void **entry)
{
    return SYS_ENOTSUP;
}
