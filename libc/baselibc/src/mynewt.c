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
#include <console/console.h>

static size_t
stdin_read(FILE *fp, char *bp, size_t n)
{
    return 0;
}

static size_t
stdout_write(FILE *fp, const char *bp, size_t n)
{
    console_write(bp, n);
    return n;
}

static struct File_methods _stdin_methods = {
    .write = stdout_write,
    .read = stdin_read
};

static struct File _stdin = {
    .vmt = &_stdin_methods
};

struct File *const stdin = &_stdin;
struct File *const stdout = &_stdin;
struct File *const stderr = &_stdin;
