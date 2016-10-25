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

#include <lwip/sys.h>
#include <ip/os_queue.h>

u32_t
sys_arch_sem_wait(sys_sem_t *sem, u32_t timo)
{
    u32_t now;

    if (timo == 0) {
        timo = OS_WAIT_FOREVER;
    } else {
        if (os_time_ms_to_ticks(timo, &timo)) {
            timo = OS_WAIT_FOREVER - 1;
        }
    }
    now = os_time_get();
    if (os_sem_pend(sem, timo) == OS_TIMEOUT) {
        return SYS_ARCH_TIMEOUT;
    }
    return (now - os_time_get()) * 1000 / OS_TICKS_PER_SEC;
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, uint32_t timo)
{
    u32_t now;
    void *val;

    if (timo == 0) {
        timo = OS_WAIT_FOREVER;
    } else {
        if (os_time_ms_to_ticks(timo, &timo)) {
            timo = OS_WAIT_FOREVER - 1;
        }
    }
    now = os_time_get();
    if (os_queue_get(mbox, &val, timo)) {
        return SYS_ARCH_TIMEOUT;
    }
    if (msg != NULL) {
        *msg = val;
    }
    return (now - os_time_get()) * 1000 / OS_TICKS_PER_SEC;
}

