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

#include "os/mynewt.h"
#include "static_task/static_task.h"

void
static_task_init(void)
{
    const struct static_task_def *td;
    int rc;

    LINK_TABLE_FOREACH(e, static_task_defs) {
        td = *e;
        rc = os_task_init(td->task, td->name, td->func, td->arg, td->prio,
                          OS_WAIT_FOREVER, td->stack, td->stack_size);
        SYSINIT_PANIC_ASSERT(rc == 0);
    }
}
