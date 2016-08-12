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

#include <os/os.h>
#include <os/endian.h>

#include <assert.h>
#include <string.h>

#include <hal/hal_system.h>

#include <newtmgr/newtmgr.h>
#include <util/datetime.h>
#include <reboot/log_reboot.h>

#include "newtmgr_priv.h"

static struct os_callout_func nmgr_reset_callout;

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

int
nmgr_def_mpstat_read(struct nmgr_jbuf *njb)
{
    struct os_mempool *prev_mp;
    struct os_mempool_info omi;
    struct json_value jv;

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);

    json_encode_object_key(&njb->njb_enc, "mpools");
    json_encode_object_start(&njb->njb_enc);

    prev_mp = NULL;
    while (1) {
        prev_mp = os_mempool_info_get_next(prev_mp, &omi);
        if (prev_mp == NULL) {
            break;
        }

        json_encode_object_key(&njb->njb_enc, omi.omi_name);

        json_encode_object_start(&njb->njb_enc);
        JSON_VALUE_UINT(&jv, omi.omi_block_size);
        json_encode_object_entry(&njb->njb_enc, "blksiz", &jv);
        JSON_VALUE_UINT(&jv, omi.omi_num_blocks);
        json_encode_object_entry(&njb->njb_enc, "nblks", &jv);
        JSON_VALUE_UINT(&jv, omi.omi_num_free);
        json_encode_object_entry(&njb->njb_enc, "nfree", &jv);
        json_encode_object_finish(&njb->njb_enc);
    }

    json_encode_object_finish(&njb->njb_enc);
    json_encode_object_finish(&njb->njb_enc);

    return (0);
}

int
nmgr_def_mpstat_write(struct nmgr_jbuf *njb)
{
    return (OS_EINVAL);
}

int
nmgr_datetime_get(struct nmgr_jbuf *njb)
{
    struct os_timeval tv;
    struct os_timezone tz;
    char buf[DATETIME_BUFSIZE];
    struct json_value jv;
    int rc;

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);

    /* Display the current datetime */
    rc = os_gettimeofday(&tv, &tz);
    assert(rc == 0);
    rc = format_datetime(&tv, &tz, buf, DATETIME_BUFSIZE);
    if (rc) {
        rc = OS_EINVAL;
        goto err;
    }

    JSON_VALUE_STRING(&jv, buf)
    json_encode_object_entry(&njb->njb_enc, "datetime", &jv);
    json_encode_object_finish(&njb->njb_enc);

    return OS_OK;
err:
    return (rc);
}

int
nmgr_datetime_set(struct nmgr_jbuf *njb)
{
    struct os_timeval tv;
    struct os_timezone tz;
    struct json_value jv;
    char buf[DATETIME_BUFSIZE];
    int rc = OS_OK;
    const struct json_attr_t datetime_write_attr[2] = {
        [0] = {
            .attribute = "datetime",
            .type = t_string,
            .addr.string = buf,
            .len = sizeof(buf),
        },
        [1] = {
            .attribute = "rc",
            .type = t_uinteger,

        }
    };

    rc = json_read_object(&njb->njb_buf, datetime_write_attr);
    if (rc) {
        rc = OS_EINVAL;
        goto out;
    }

    /* Set the current datetime */
    rc = parse_datetime(buf, &tv, &tz);
    if (!rc) {
        rc = os_settimeofday(&tv, &tz);
        if (rc) {
          rc = OS_EINVAL;
          goto out;
        }
    } else {
        rc = OS_EINVAL;
        goto out;
    }

out:
    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_INT(&jv, rc);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);
    json_encode_object_finish(&njb->njb_enc);
    return OS_OK;
}

static void
nmgr_reset_tmo(void *arg)
{
    system_reset();
}

int
nmgr_reset(struct nmgr_jbuf *njb)
{
    if (nmgr_reset_callout.cf_func == NULL) {
        os_callout_func_init(&nmgr_reset_callout, &g_nmgr_evq,
          nmgr_reset_tmo, NULL);
    }
    log_reboot(SOFT_REBOOT);
    os_callout_reset(&nmgr_reset_callout.cf_c, OS_TICKS_PER_SEC / 4);
    json_encode_object_start(&njb->njb_enc);
    json_encode_object_finish(&njb->njb_enc);

    nmgr_jbuf_setoerr(njb, 0);

    return OS_OK;
}
