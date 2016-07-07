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


#include <assert.h>
#include "os/os_mutex.h"
#include "os/os_heap.h"

static struct os_mutex os_malloc_mutex;

static void
os_malloc_lock(void)
{
    int rc;

    if (g_os_started) {
        rc = os_mutex_pend(&os_malloc_mutex, 0xffffffff);
        assert(rc == 0);
    }
}

static void
os_malloc_unlock(void)
{
    int rc;

    if (g_os_started) {
        rc = os_mutex_release(&os_malloc_mutex);
        assert(rc == 0);
    }
}

void *
os_malloc(size_t size)
{
    void *ptr;

    os_malloc_lock();
    ptr = malloc(size);
    os_malloc_unlock();

    return ptr;
}

void
os_free(void *mem)
{
    os_malloc_lock();
    free(mem);
    os_malloc_unlock();
}

void *
os_realloc(void *ptr, size_t size)
{
    void *new_ptr;

    os_malloc_lock();
    new_ptr = realloc(ptr, size);
    os_malloc_unlock();

    return new_ptr;
}
