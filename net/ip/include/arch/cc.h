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
#ifndef __LWIP_ARCH_CC_H__
#define __LWIP_ARCH_CC_H__

#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__ ((__packed__))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#define LWIP_RAND()	rand()

/*
 * xxxx hook into sys/log
 */
#define LWIP_PLATFORM_DIAG(__args__)

void __assert_f(const char *file, int line);

#define LWIP_PLATFORM_ASSERT(x) assert(x)

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_ARCH_CC_H__ */
