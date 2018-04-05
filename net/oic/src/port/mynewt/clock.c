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

#include <stdint.h>
#include "os/mynewt.h"
#include "../oc_clock.h"

void oc_clock_init(void)
{
    /* in mynewt clock is initialized elsewhere */
}
oc_clock_time_t oc_clock_time(void)
{
    return os_time_get();
}

unsigned long oc_clock_seconds(void)
{
    return os_time_get()/OS_TICKS_PER_SEC;
}

void oc_clock_wait(oc_clock_time_t t)
{
    os_time_delay(t);
}
