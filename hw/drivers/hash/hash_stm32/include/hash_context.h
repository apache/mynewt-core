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

#ifndef __HASH_CONTEXT_H__
#define __HASH_CONTEXT_H__

#include "hash/hash.h"

/*
 * XXX: the STM32 SDK handles context, so here we just have to need to
 *      properly implement the expected interface. Whenever updating these
 *      structs always maintain them in sync because only hash_sha2_context
 *      is used internally.
 * NOTE: In the STM32 only the finish operation is able to write less than
 *      a word (32-bit) so a bit of internal state must be maintained.
 */

#define STATESZ sizeof(uint32_t)

struct hash_sha224_context {
    void *dev;
    uint8_t remain;
    uint8_t state[STATESZ];
};

struct hash_sha256_context {
    void *dev;
    uint8_t remain;
    uint8_t state[STATESZ];
};

struct hash_sha2_context {
    void *dev;
    uint8_t remain;
    uint8_t state[STATESZ];
};

#endif /* __HASH_CONTEXT_H__ */
