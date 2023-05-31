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

#include <sys/stat.h>
#include <syscfg/syscfg.h>

#include <console/console.h>

int
_write(int fd, char *ptr, int len)
{
    (void)fd, (void)ptr;

    if (fd == 1 || fd == 2) {
        console_write(ptr, len);
    }
    return len;
}

int
_close(int fd)
{
    (void)fd;
    return 0;
}

int
_fstat(int fd, struct stat *st)
{
    (void)fd, (void)st;
    return 0;
}

int
_isatty(int fd)
{
    (void)fd;
    return 0;
}

int
_read(int fd, char *ptr, int len)
{
    (void)fd, (void)ptr, (void)len;
    return 0;
}

int
_lseek(int fd, int ptr, int dir)
{
    (void)fd, (void)ptr, (void)dir;
    return 0;
}
