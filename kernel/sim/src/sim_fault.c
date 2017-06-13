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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "os/os.h"

void
sim_assert_fail(const char *file, int line, const char *func, const char *e)
{
    char msg[256];
    int len;

    if (file) {
        snprintf(msg, sizeof(msg), "assert @ %s:%d\n", file, line);
    } else {
        snprintf(msg, sizeof(msg), "assert @ %p\n",
                 __builtin_return_address(0));
    }
    len = write(1, msg, strlen(msg));
    (void)len;
    _Exit(1);
}
