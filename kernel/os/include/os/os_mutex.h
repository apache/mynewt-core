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
 *   @defgroup OSMutex Mutexes
 *   @{
 */

#ifndef _OS_MUTEX_H_
#define _OS_MUTEX_H_

#include "os/os.h"
#include "os/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * OS mutex structure
 */
struct os_mutex {
    SLIST_HEAD(, os_task) mu_head;
    uint8_t     _pad;
    /** Mutex owner's default priority */
    uint8_t     mu_prio;
    /** Mutex call nesting level */
    uint16_t    mu_level;
    /** Task that owns the mutex */
    struct os_task *mu_owner;
};

/*
  XXX: NOTES
    -> Should we add a magic number or flag to the mutex structure so
    that we know that this is a mutex? We can use it for double checking
    that a proper mutex was passed in to the API.
    -> What debug information should we add to this structure? Who last
    acquired the mutex? File/line where it was released?
    -> Should we add a name to the mutex?
    -> Should we add a "os_mutex_inspect() api?
*/

/* XXX: api to create
os_mutex_inspect();
*/

/**
 * Create a mutex and initialize it.
 *
 * @param mu Pointer to mutex
 *
 * @return os_error_t
 *      OS_INVALID_PARM     Mutex passed in was NULL.
 *      OS_OK               no error.
 */
os_error_t os_mutex_init(struct os_mutex *mu);

/**
 * Release a mutex.
 *
 * @param mu Pointer to the mutex to be released
 *
 * @return os_error_t
 *      OS_INVALID_PARM Mutex passed in was NULL.
 *      OS_BAD_MUTEX    Mutex was not granted to current task (not owner).
 *      OS_OK           No error
 */
os_error_t os_mutex_release(struct os_mutex *mu);

/**
 * Pend (wait) for a mutex.
 *
 * @param mu Pointer to mutex.
 * @param timeout Timeout, in os ticks. A timeout of 0 means do
 *                not wait if not available. A timeout of
 *                0xFFFFFFFF means wait forever.
 *
 *
 * @return os_error_t
 *      OS_INVALID_PARM     Mutex passed in was NULL.
 *      OS_TIMEOUT          Mutex was owned by another task and timeout=0
 *      OS_OK               no error.
 */
os_error_t os_mutex_pend(struct os_mutex *mu, uint32_t timeout);

#ifdef __cplusplus
}
#endif

#endif  /* _OS_MUTEX_H_ */
