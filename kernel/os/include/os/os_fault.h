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

#ifndef _OS_FAULT_H
#define _OS_FAULT_H

#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void __assert_func(const char *file, int line, const char *func, const char *e)
    __attribute((noreturn));

#if MYNEWT_VAL(OS_CRASH_FILE_LINE)
#define OS_CRASH() __assert_func(__FILE__, __LINE__, NULL, NULL)
#else
#define OS_CRASH() __assert_func(NULL, 0, NULL, NULL)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _OS_FAULT_H */
