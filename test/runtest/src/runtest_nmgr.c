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

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(RUNTEST_NEWTMGR)
#include <string.h>

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "console/console.h"

#include "runtest/runtest.h"
#include "runtest_priv.h"

static int runtest_nmgr_write(struct mgmt_cbuf *);

static const struct mgmt_handler runtest_nmgr_handler[] = {
    [0] = { runtest_nmgr_write, runtest_nmgr_write }
};

struct mgmt_group runtest_nmgr_group = {
    .mg_handlers = (struct mgmt_handler *)runtest_nmgr_handler,
    .mg_handlers_count = 1,
    .mg_group_id = MGMT_GROUP_ID_RUNTEST
};

/*
 * XXX global used to gate starting test - hack
 */
int runtest_start;

static int
runtest_nmgr_write(struct mgmt_cbuf *cb)
{
    char tmp_str[64];
    const struct cbor_attr_t attr[2] = {
        [0] = {
            .attribute = "u",
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
    if (rc) {
        rc = MGMT_ERR_EINVAL;
    } else {
        /* XXX ugh - drop out of a loop allowing a test to run in an app */
        runtest_start = 0;
        /*
         * we should be doing something more like this where we call
         * a named test suite directly
         */
#ifdef NOTYET
        rc = start_test(tmp_str);
#endif
        if (rc) {
            rc = MGMT_ERR_EINVAL;
        }
    }
    mgmt_cbuf_setoerr(cb, rc);
    return 0;
}

#endif /* MYNEWT_VAL(RUNTEST_NEWTMGR) */
