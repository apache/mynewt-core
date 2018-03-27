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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "os/mynewt.h"
#include "os_priv.h"
#include "hal/hal_system.h"

void
__assert_func(const char *file, int line, const char *func, const char *e)
{
    OS_PRINT_ASSERT(file, line, func, e);
    _exit(1);
}

uintptr_t
handle_trap(uint32_t cause, void *fault_address, void *exception_frame)
{
    hal_system_reset();
}

void os_default_irq_asm(int num)
{

}
