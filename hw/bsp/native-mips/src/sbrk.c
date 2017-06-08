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

#include <sys/mman.h>
#include <unistd.h>

#include <hal/hal_bsp.h>

extern int getpagesize(void);

static void *cont;
static int sys_pagesize;
static int cont_left;

void *
_sbrk(int incr)
{
    void *result;

    if (!sys_pagesize) {
        sys_pagesize = getpagesize();
    }
    if (cont && incr <= cont_left) {
        result = cont;
        cont_left -= incr;
        if (cont_left) {
            cont = (char *)cont + incr;
        } else {
            cont = NULL;
        }
        return result;
    }
    result = mmap(NULL, incr, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED,
      -1, 0);
    if (result) {
        cont_left = sys_pagesize - (incr % sys_pagesize);
        if (cont_left) {
            cont = (char *)result + incr;
        } else {
            cont = NULL;
        }
    }
    return result;
}
