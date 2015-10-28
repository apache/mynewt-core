/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _OS_TIME_H
#define _OS_TIME_H

#include <stdint.h>

typedef uint32_t os_time_t;

#ifndef UINT32_MAX
#define UINT32_MAX  0xFFFFFFFFU
#endif
 
/* Used to wait forever for events and mutexs */
#define OS_TIMEOUT_NEVER    (UINT32_MAX)

os_time_t os_time_get(void);
void os_time_tick(void);
void os_time_delay(int32_t osticks);

#define OS_TIME_TICK_LT(__t1, __t2) ((int32_t) ((__t1) - (__t2)) < 0)
#define OS_TIME_TICK_GT(__t1, __t2) ((int32_t) ((__t1) - (__t2)) > 0)
#define OS_TIME_TICK_GEQ(__t1, __t2) ((int32_t) ((__t1) - (__t2)) >= 0)

#endif /* _OS_TIME_H */
