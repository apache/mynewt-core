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

#ifndef _HW_DRIVERS_IPC_ICBMSG_UTILS_H
#define _HW_DRIVERS_IPC_ICBMSG_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/* Value of x rounded up to the next multiple of align. */
#define ROUND_UP(x, align)                                  \
    ((((unsigned long)(x) + ((unsigned long)(align) - 1)) / \
      (unsigned long)(align)) * (unsigned long)(align))

#define ROUND_DOWN(x, align)                                 \
    (((unsigned long)(x) / (unsigned long)(align)) * (unsigned long)(align))

/* Check if a pointer is aligned for against a specific byte boundary  */
#define IS_PTR_ALIGNED_BYTES(ptr, bytes) ((((uintptr_t)ptr) % bytes) == 0)

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef __cplusplus
}
#endif

#endif /* _HW_DRIVERS_IPC_ICBMSG_UTILS_H */
