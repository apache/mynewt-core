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

#include <os/os.h>
#include <os/endian.h>

#include <assert.h>

#include <newtmgr/newtmgr.h>

#include <string.h>


int 
nmgr_def_taskstat_read(struct nmgr_jbuf *njb)
{
    struct os_task *prev_task;
    struct os_task_info oti;
    struct json_value jv;

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);

    json_encode_object_key(&njb->njb_enc, "tasks");
    json_encode_object_start(&njb->njb_enc);

    prev_task = NULL;
    while (1) {
        prev_task = os_task_info_get_next(prev_task, &oti);
        if (prev_task == NULL) {
            break;
        }

        json_encode_object_key(&njb->njb_enc, oti.oti_name);

        json_encode_object_start(&njb->njb_enc);
        JSON_VALUE_UINT(&jv, oti.oti_prio);
        json_encode_object_entry(&njb->njb_enc, "prio", &jv);
        JSON_VALUE_UINT(&jv, oti.oti_taskid);
        json_encode_object_entry(&njb->njb_enc, "tid", &jv);
        JSON_VALUE_UINT(&jv, oti.oti_state);
        json_encode_object_entry(&njb->njb_enc, "state", &jv);
        JSON_VALUE_UINT(&jv, oti.oti_stkusage);
        json_encode_object_entry(&njb->njb_enc, "stkuse", &jv);
        JSON_VALUE_UINT(&jv, oti.oti_stksize);
        json_encode_object_entry(&njb->njb_enc, "stksiz", &jv);
        JSON_VALUE_UINT(&jv, oti.oti_cswcnt);
        json_encode_object_entry(&njb->njb_enc, "cswcnt", &jv);
        JSON_VALUE_UINT(&jv, oti.oti_runtime);
        json_encode_object_entry(&njb->njb_enc, "runtime", &jv);
        JSON_VALUE_UINT(&jv, oti.oti_last_checkin);
        json_encode_object_entry(&njb->njb_enc, "last_checkin", &jv);
        JSON_VALUE_UINT(&jv, oti.oti_next_checkin);
        json_encode_object_entry(&njb->njb_enc, "next_checkin", &jv);
        json_encode_object_finish(&njb->njb_enc);
    }
    json_encode_object_finish(&njb->njb_enc);
    json_encode_object_finish(&njb->njb_enc);

    return (0);
}

int 
nmgr_def_taskstat_write(struct nmgr_jbuf *njb)
{
    return (OS_EINVAL);
}
