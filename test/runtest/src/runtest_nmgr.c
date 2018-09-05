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

#if MYNEWT_VAL(RUNTEST_NEWTMGR)
#include <string.h>

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "console/console.h"

#include "testutil/testutil.h"

#include "runtest/runtest.h"
#include "runtest_priv.h"

static int runtest_nmgr_test(struct mgmt_cbuf *);
static int runtest_nmgr_list(struct mgmt_cbuf *);

static struct mgmt_group runtest_nmgr_group;

static const struct mgmt_handler runtest_nmgr_handlers[] = {
    [RUNTEST_NMGR_OP_TEST] = { NULL, runtest_nmgr_test },
    [RUNTEST_NMGR_OP_LIST] = { runtest_nmgr_list, NULL }
};

/*
 * package "run test" request from newtmgr and enqueue on default queue
 * of the application which is actually running the tests (e.g., mynewtsanity).
 * Application callback was initialized by call to run_evb_set() above.
 */
static int
runtest_nmgr_test(struct mgmt_cbuf *cb)
{
    char testname[MYNEWT_VAL(RUNTEST_MAX_TEST_NAME_LEN)] = "";
    char token[MYNEWT_VAL(RUNTEST_MAX_TOKEN_LEN)];
    int rc;

    const struct cbor_attr_t attr[] = {
        [0] = {
            .attribute = "testname",
            .type = CborAttrTextStringType,
            .addr.string = testname,
            .len = sizeof(testname)
        },
        [1] = {
            .attribute = "token",
            .type = CborAttrTextStringType,
            .addr.string = token,
            .len = sizeof(token)
        },
        [2] = {
            .attribute = NULL
        }
    };

    rc = cbor_read_object(&cb->it, attr);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    /*
     * testname is one of:
     * a) a specific test suite name
     * b) "all".
     * c) "" (empty string); equivalent to "all".
     *
     * token is appended to log messages.
     */
    rc = runtest_run(testname, token);
    switch (rc) {
    case 0:
        return 0;

    case SYS_EAGAIN:
        return MGMT_ERR_EBADSTATE;

    case SYS_ENOENT:
        return MGMT_ERR_ENOENT;

    default:
        return MGMT_ERR_EUNKNOWN;
    }
}

/*
 * List all register tests
 */
static int
runtest_nmgr_list(struct mgmt_cbuf *cb)
{
    CborError g_err = CborNoError;
    CborEncoder run_list;
    struct ts_suite *ts;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    g_err |= cbor_encode_text_stringz(&cb->encoder, "run_list");
    g_err |= cbor_encoder_create_array(&cb->encoder, &run_list,
                                       CborIndefiniteLength);

    SLIST_FOREACH(ts, &g_ts_suites, ts_next) {
        g_err |= cbor_encode_text_stringz(&run_list, ts->ts_name);
    }

    g_err |= cbor_encoder_close_container(&cb->encoder, &run_list);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

/*
 * Register nmgr group handlers
 */
int
runtest_nmgr_register_group(void)
{
    MGMT_GROUP_SET_HANDLERS(&runtest_nmgr_group, runtest_nmgr_handlers);
    runtest_nmgr_group.mg_group_id = MGMT_GROUP_ID_RUN;

    return mgmt_group_register(&runtest_nmgr_group);
}

#endif /* MYNEWT_VAL(RUNTEST_NEWTMGR) */
