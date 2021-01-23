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

#include "osdp/osdp_hooks.h"
#include "modlog/modlog.h"

#if MYNEWT_VAL(OSDP_USE_CRYPTO_HOOK)
/*
 * Stub function. Needs to be overridden to use correctly
 */
size_t
osdp_hook_crypto_random_bytes(uint8_t *out, size_t out_len)
{
    return 1;
}

#endif /* OSDP_USE_CRYPTO_HOOK */
