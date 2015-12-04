/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>

extern char _user_heap_start;
extern char _user_heap_end;
static char *_brk = &_user_heap_start;

void *
_sbrk(int incr)
{
    void *prev_brk;

    if (incr < 0) {
        /* Returning memory to the heap. */
        prev_brk = (void *)-1;
    } else {
        /* Allocating memory from the heap. */
        if (&_user_heap_end - _brk >= incr) {
            prev_brk = _brk;
            _brk += incr;
        } else {
            prev_brk = (void *)-1;
            errno = ENOMEM;
        }
    }

    return prev_brk;
}
