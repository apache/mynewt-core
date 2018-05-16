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

#include <ctype.h>
#include <stdint.h>

#include "os/mynewt.h"

#include "sysview/vendor/SEGGER_SYSVIEW.h"
#include "sysview/vendor/Global.h"
#include "sysview/SEGGER_SYSVIEW_Mynewt.h"

static void send_task_list_cb(void) {
    struct os_task *prev_task;
    struct os_task_info oti;
    SEGGER_SYSVIEW_TASKINFO Info;

    prev_task = NULL;
    while (1) {
        prev_task = os_task_info_get_next(prev_task, &oti);
        if (prev_task == NULL) {
            break;
        }
        Info.TaskID = oti.oti_taskid;
        Info.sName = oti.oti_name;
        Info.StackSize = oti.oti_stksize;
        Info.Prio = oti.oti_prio;
        SEGGER_SYSVIEW_SendTaskInfo(&Info);
    }
}

static U64 get_time_cb(void) {
    return (U64)os_time_get();
}

const SEGGER_SYSVIEW_OS_API SYSVIEW_X_OS_TraceAPI = {
    get_time_cb,
    send_task_list_cb,
};

/*
 * Package initialization function
 */
void
sysview_init(void)
{
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();
}
