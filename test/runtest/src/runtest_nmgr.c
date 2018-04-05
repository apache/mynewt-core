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

static int run_nmgr_test(struct mgmt_cbuf *);
static int run_nmgr_list(struct mgmt_cbuf *);

struct mgmt_group run_nmgr_group;

#define RUN_NMGR_OP_TEST    0
#define RUN_NMGR_OP_LIST    1

const struct mgmt_handler run_nmgr_handlers[] = {
    [RUN_NMGR_OP_TEST] = {NULL, run_nmgr_test},
    [RUN_NMGR_OP_LIST] = {run_nmgr_list, NULL}
};

extern void sanity_start_test();
struct os_event run_test_event;

char run_testname[RUNTEST_REQ_SIZE];
char run_token[RUNTEST_REQ_SIZE];

struct runtest_evq_arg runtest_arg;
os_event_fn *run_callback;

void
run_evcb_set(os_event_fn *cb)
{
    run_callback = cb;
}

/*
 * package "run test" request from newtmgr and enqueue on default queue
 * of the application which is actually running the tests (e.g., mynewtsanity).
 * Application callback was initialized by call to run_evb_set() above.
 */
static int
run_nmgr_test(struct mgmt_cbuf *cb)
{
    int rc;

    const struct cbor_attr_t attr[3] = {
        [0] = {
            .attribute = "testname",
            .type = CborAttrTextStringType,
            .addr.string = run_testname,
            .len = sizeof(run_testname)
        },
        [1] = {
            .attribute = "token",
            .type = CborAttrTextStringType,
            .addr.string = run_token,
            .len = sizeof(run_token)
        },
        [2] = {
            .attribute = NULL
        }
    };

    rc = cbor_read_object(&cb->it, attr);
    if (rc) {
        return rc;
    }

    /*
     * testname is either a specific test or newtmgr passed "all".
     * token is appened to log messages.
     */
    runtest_arg.run_testname = run_testname;
    runtest_arg.run_token = run_token;

    run_test_event.ev_arg = &runtest_arg;
    run_test_event.ev_cb = run_callback;

    os_eventq_put(run_evq_get(), &run_test_event);
            
    return 0;
}

/*
 * List all register tests
 */
static int
run_nmgr_list(struct mgmt_cbuf *cb)
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
run_nmgr_register_group(void)
{
    int rc;

    MGMT_GROUP_SET_HANDLERS(&run_nmgr_group, run_nmgr_handlers);
    run_nmgr_group.mg_group_id = MGMT_GROUP_ID_RUN;

    rc = mgmt_group_register(&run_nmgr_group);
    if (rc != 0) {
        goto err;
    }
    return (0);

err:
    return (rc);
}
#endif /* MYNEWT_VAL(RUN_NEWTMGR) */
