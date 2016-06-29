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

#ifdef NEWTMGR_PRESENT

#include <string.h>

#include <newtmgr/newtmgr.h>
#include <json/json.h>
#include <console/console.h>

#include "crash_test/crash_test.h"
#include "crash_test_priv.h"

static int crash_test_nmgr_write(struct nmgr_jbuf *);

static const struct nmgr_handler crash_test_nmgr_handler[] = {
    [0] = { crash_test_nmgr_write, crash_test_nmgr_write }
};

struct nmgr_group crash_test_nmgr_group = {
    .ng_handlers = (struct nmgr_handler *)crash_test_nmgr_handler,
    .ng_handlers_count = 1,
    .ng_group_id = NMGR_GROUP_ID_CRASH
};

static int
crash_test_nmgr_write(struct nmgr_jbuf *njb)
{
    char tmp_str[64];
    const struct json_attr_t attr[2] = {
        [0] = {
            .attribute = "t",
            .type = t_string,
            .addr.string = tmp_str,
            .len = sizeof(tmp_str)
        },
        [1] = {
            .attribute = NULL
        }
    };
    int rc;

    rc = json_read_object(&njb->njb_buf, attr);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
    } else {
        rc = crash_device(tmp_str);
        if (rc) {
            rc = NMGR_ERR_EINVAL;
        }
    }
    nmgr_jbuf_setoerr(njb, rc);
    return 0;
}

#endif
