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

#include "trng/trng.h"

uint32_t
trng_get_u32(struct trng_dev *trng)
{
    assert(trng->interface.get_u32);

    return trng->interface.get_u32(trng);
}

size_t
trng_read(struct trng_dev *trng, void *ptr, size_t size)
{
    assert(trng->interface.read);

    return trng->interface.read(trng, ptr, size);
}
