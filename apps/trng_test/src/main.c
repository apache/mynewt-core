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

#include <stddef.h>
#include <stdint.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "console/console.h"
#include "trng/trng.h"

static uint8_t buf[128];

static void
print_buffer(void *ptr, size_t size)
{
    while (size) {
        console_printf("%02x", *((uint8_t *) ptr));

        size--;
        ptr++;
    }

    console_printf("\n");
}

int
main(void)
{
    struct trng_dev *trng;

    sysinit();

    trng = (struct trng_dev *) os_dev_open(MYNEWT_VAL(APP_TRNG_DEV),
                                           OS_TIMEOUT_NEVER, NULL);
    assert(trng);

    os_time_delay(OS_TICKS_PER_SEC);

    trng_read(trng, buf, sizeof(buf));
    console_printf("%d bytes from os_dev:\n", sizeof(buf));
    print_buffer(buf, sizeof(buf));

    while (1) {
        console_printf("os_dev -> %08x\n", (unsigned) trng_get_u32(trng));

        os_time_delay(OS_TICKS_PER_SEC / 4);
    }

    return 0;
}
