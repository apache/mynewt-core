/**
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
#include <hal/hal_system.h>

void * _sbrk(int c);
int _close(int fd);
int _fstat(int fd, void *s);
void _exit(int s);
int _kill(int pid, int sig);
int _write(int fd, void *b, int nb);
int _isatty(int c);
int _lseek(int fd, int off, int w);
int _read(int fd, void *b, int nb);
int _getpid(void);

int
_close(int fd)
{
    return -1;
}

int
_fstat(int fd, void *s)
{
    return -1;
}


void
_exit(int s)
{
    system_reset();
}

int
_kill(int pid, int sig)
{
    return -1;
}

int
_write(int fd, void *b, int nb)
{
    return -1;
}

int
_isatty(int c)
{
    return -1;
}

int
_lseek(int fd, int off, int w)
{
    return -1;
}

int
_read(int fd, void *b, int nb)
{
    return -1;
}

int
_getpid(void) {
    return -1;
}
