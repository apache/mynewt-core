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

#if MYNEWT_VAL(CRASH_TEST_NEWTMGR)

#include <string.h>

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "console/console.h"

#include "crash_test/crash_test.h"
#include "crash_test_priv.h"

static int crash_test_nmgr_write(struct mgmt_cbuf *);

static const struct mgmt_handler crash_test_nmgr_handler[] = {
    [0] = { NULL, crash_test_nmgr_write }
};

struct mgmt_group crash_test_nmgr_group = {
    .mg_handlers = (struct mgmt_handler *)crash_test_nmgr_handler,
    .mg_handlers_count = 1,
    .mg_group_id = MGMT_GROUP_ID_CRASH
};

static int
crash_test_nmgr_write(struct mgmt_cbuf *cb)
{
    char tmp_str[64];
    const struct cbor_attr_t attr[2] = {
        [0] = {
            .attribute = "t",
            .type = CborAttrTextStringType,
            .addr.string = tmp_str,
            .len = sizeof(tmp_str)
        },
        [1] = {
            .attribute = NULL
        }
    };
    int rc;

    rc = cbor_read_object(&cb->it, attr);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    rc = crash_device(tmp_str);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    rc = mgmt_cbuf_setoerr(cb, 0);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

#endif /* MYNEWT_VAL(CRASH_TEST_NEWTMGR) */
