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

#include "memfault_sdk/memfault_sdk.h"

#if MYNEWT_VAL(MEMFAULT_ENABLE)
#include "memfault/panics/coredump.h"
#include "memfault/panics/arch/arm/cortex_m.h"
#include "memfault/panics/platform/coredump.h"
#endif

#if MYNEWT_VAL(MEMFAULT_ENABLE)
#if MYNEWT_VAL(MEMFAULT_COREDUMP_CB)
void os_coredump_cb(void *tf) {
    memfault_fault_handler((sMfltRegState *)tf, kMfltRebootReason_HardFault);
}
#endif
#endif