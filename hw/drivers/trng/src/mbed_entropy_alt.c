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

#include <trng/trng.h>
#include "os/mynewt.h"

#if MYNEWT_VAL(MBEDTLS_ENTROPY_HARDWARE_ALT) == 1
int
mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    struct trng_dev *trng;

    trng = (struct trng_dev *)os_dev_lookup("trng");
    if (trng == NULL) {
        return -1;
    }
    *olen = trng_read(trng, output, len);
    return 0;
}
#endif
