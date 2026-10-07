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

#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "shell/shell.h"
#include "mgmt/mgmt.h"
#include "tinycbor/cbor.h"

#define TEST_ARGC_MAX MYNEWT_VAL(SHELL_CMD_ARGC_MAX)

static int test_cmd_calls;
static int test_cmd_argc;
static bool test_cmd_argv_terminated;

static int
test_cmd(int argc, char **argv)
{
    test_cmd_calls++;
    test_cmd_argc = argc;
    test_cmd_argv_terminated = argv[argc] == NULL;

    return 0;
}

/* Fill unused stack with non-zero pattern so that uninitialized stack
 * variables of the handler called next do not happen to be zero.
 */
static void __attribute__((noinline))
test_dirty_stack(void)
{
    volatile uint8_t buf[2048];

    memset((void *)buf, 0xa5, sizeof(buf));
}

/* Executes "shell exec" mgmt request with specified arguments. */
static int
test_shell_exec(const char **args, int argc)
{
    const struct mgmt_handler *handler;
    struct mgmt_ctxt ctxt;
    CborEncoder req_enc;
    CborEncoder map;
    CborEncoder arr;
    CborEncoder payload;
    uint8_t req[256];
    uint8_t rsp[512];
    int rc;
    int i;

    /* Encode request: {"argv": [args...]} */
    cbor_encoder_init(&req_enc, req, sizeof(req), 0);
    rc = cbor_encoder_create_map(&req_enc, &map, CborIndefiniteLength);
    rc |= cbor_encode_text_stringz(&map, "argv");
    rc |= cbor_encoder_create_array(&map, &arr, CborIndefiniteLength);
    for (i = 0; i < argc; i++) {
        rc |= cbor_encode_text_stringz(&arr, args[i]);
    }
    rc |= cbor_encoder_close_container(&map, &arr);
    rc |= cbor_encoder_close_container(&req_enc, &map);
    TEST_ASSERT_FATAL(rc == 0);

    rc = cbor_parser_init(req, cbor_encoder_get_buffer_size(&req_enc, req), 0,
                          &ctxt.parser, &ctxt.it);
    TEST_ASSERT_FATAL(rc == 0);

    cbor_encoder_init(&ctxt.encoder, rsp, sizeof(rsp), 0);
    rc = cbor_encoder_create_map(&ctxt.encoder, &payload, CborIndefiniteLength);
    TEST_ASSERT_FATAL(rc == 0);

    handler = mgmt_find_handler(MGMT_GROUP_ID_SHELL, SHELL_NMGR_OP_EXEC);
    TEST_ASSERT_FATAL(handler != NULL);
    TEST_ASSERT_FATAL(handler->mh_write != NULL);

    test_cmd_calls = 0;
    test_cmd_argc = -1;
    test_cmd_argv_terminated = false;

    test_dirty_stack();

    return handler->mh_write(&ctxt);
}

TEST_CASE_SELF(shell_bridge_test_case_exec)
{
    const char *args[TEST_ARGC_MAX];
    int rc;
    int i;

    shell_register_app_cmd_handler(test_cmd);

    /* Command without arguments. */
    args[0] = "testcmd";
    rc = test_shell_exec(args, 1);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_cmd_calls == 1);
    TEST_ASSERT(test_cmd_argv_terminated);

    /* Command with arguments. */
    args[1] = "arg1";
    args[2] = "arg2";
    rc = test_shell_exec(args, 3);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_cmd_calls == 1);
    TEST_ASSERT(test_cmd_argv_terminated);

    /* Maximum number of arguments. */
    for (i = 1; i < TEST_ARGC_MAX; i++) {
        args[i] = "arg";
    }
    rc = test_shell_exec(args, TEST_ARGC_MAX);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_cmd_calls == 1);
    TEST_ASSERT(test_cmd_argv_terminated);
}

TEST_SUITE(shell_bridge_test_suite)
{
    /* Single test case only as shell bridge group is registered on each
     * sysinit.
     */
    shell_bridge_test_case_exec();
}

int
main(int argc, char **argv)
{
    shell_bridge_test_suite();
    return tu_any_failed;
}
