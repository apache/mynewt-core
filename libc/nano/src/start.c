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

#include <stdlib.h>
#include "os/mynewt.h"

extern int mynewt_main(int argc, char **argv);
void __libc_init_array(void);

/*
 * Rudimentary startup function.
 */
void _start(void)
{
    /*
     * Run global objects constructors.
     * Call to this function is ofter found in startup files.
     * mynewt did not use this pattern hence we have single place
     * for all MCU just here
     */
    __libc_init_array();

#if !MYNEWT_VAL(OS_SCHEDULING)
    int rc;

    rc = mynewt_main(0, NULL);
    exit(rc);
#else
    os_init(mynewt_main);
    os_start();
#endif
}

__attribute__((weak)) void
_init(void)
{
}

__attribute__((weak)) void
_fini(void)
{
}

extern void (*__preinit_array_start[])(void);
extern void (*__preinit_array_end[])(void);
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

void
__libc_init_array(void)
{
    size_t count;
    size_t i;

    count = __preinit_array_end - __preinit_array_start;
    for (i = 0; i < count; i++)
        __preinit_array_start[i]();

    _init();

    count = __init_array_end - __init_array_start;
    for (i = 0; i < count; i++)
        __init_array_start[i]();
}
