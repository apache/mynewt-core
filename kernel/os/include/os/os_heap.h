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


/**
 * @addtogroup OSKernel
 * @{
 *   @defgroup OSGeneral
 *   @{
 */


#ifndef H_OS_HEAP_
#define H_OS_HEAP_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Operating system level malloc().   This ensures that a safe malloc occurs
 * within the context of the OS.  Depending on platform, the OS may rely on
 * libc's malloc() implementation, which is not guaranteed to be thread-safe.
 * This malloc() will always be thread-safe.
 *
 * @param size The number of bytes to allocate
 *
 * @return A pointer to the memory region allocated.
 */
void *os_malloc(size_t size);


/**
 * Operating system level free().  See description of os_malloc() for reasoning.
 *
 * Free's memory allocated by malloc.
 *
 * @param mem The memory to free.
 */
void os_free(void *mem);

/**
 * Operating system level realloc(). See description of os_malloc() for reasoning.
 *
 * Reallocates the memory at ptr, to be size contiguouos bytes.
 *
 * @param ptr A pointer to the memory to allocate
 * @param size The number of contiguouos bytes to allocate at that location
 *
 * @return A pointer to memory of size, or NULL on failure to allocate
 */
void *os_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif


/**
 *   @} OSGeneral
 * @} OS Kernel
 */
