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

#include <assert.h>
#include <string.h>

#include "os/mynewt.h"

#include <hal/hal_system.h>
#include <hal/hal_watchdog.h>

#include <mgmt/mgmt.h>

#include <console/console.h>
#include <datetime/datetime.h>

#if MYNEWT_VAL(TIMEPERSIST)
#include <timepersist/timepersist.h>
#endif

#include "smp_os/smp_os.h"

#include <tinycbor/cbor.h>
#include <cborattr/cborattr.h>

static int smp_def_console_echo(struct mgmt_ctxt *cb);
static int smp_def_mpstat_read(struct mgmt_ctxt *cb);
static int smp_datetime_get(struct mgmt_ctxt *cb);
static int smp_datetime_set(struct mgmt_ctxt *cb);

static const struct mgmt_handler smp_def_group_handlers[] = {
    [SMP_ID_CONS_ECHO_CTRL] = {
        smp_def_console_echo, smp_def_console_echo
    },
    [SMP_ID_MPSTATS] = {
        smp_def_mpstat_read, NULL
    },
    [SMP_ID_DATETIME_STR] = {
        smp_datetime_get, smp_datetime_set
    },
};

#define SMP_DEF_GROUP_SZ                                               \
    (sizeof(smp_def_group_handlers) / sizeof(smp_def_group_handlers[0]))

static struct mgmt_group smp_def_group = {
    .mg_handlers = (struct mgmt_handler *)smp_def_group_handlers,
    .mg_handlers_count = SMP_DEF_GROUP_SZ,
    .mg_group_id = MGMT_GROUP_ID_OS
};

static int
smp_def_console_echo(struct mgmt_ctxt *cb)
{
    long long int echo_on = 1;
    int rc;
    struct cbor_attr_t attrs[2] = {
        [0] = {
            .attribute = "echo",
            .type = CborAttrIntegerType,
            .addr.integer = &echo_on,
            .nodefault = 1
        },
        [1] = { 0 },
    };

    rc = cbor_read_object(&cb->it, attrs);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    if (echo_on) {
        console_echo(1);
    } else {
        console_echo(0);
    }
    return (0);
}

static int
smp_def_mpstat_read(struct mgmt_ctxt *cb)
{
    struct os_mempool *prev_mp;
    struct os_mempool_info omi;
    CborError g_err = CborNoError;
    CborEncoder pools;
    CborEncoder pool;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "mpools");
    g_err |= cbor_encoder_create_map(&cb->encoder, &pools,
                                     CborIndefiniteLength);

    prev_mp = NULL;
    while (1) {
        prev_mp = os_mempool_info_get_next(prev_mp, &omi);
        if (prev_mp == NULL) {
            break;
        }

        g_err |= cbor_encode_text_stringz(&pools, omi.omi_name);
        g_err |= cbor_encoder_create_map(&pools, &pool, CborIndefiniteLength);
        g_err |= cbor_encode_text_stringz(&pool, "blksiz");
        g_err |= cbor_encode_uint(&pool, omi.omi_block_size);
        g_err |= cbor_encode_text_stringz(&pool, "nblks");
        g_err |= cbor_encode_uint(&pool, omi.omi_num_blocks);
        g_err |= cbor_encode_text_stringz(&pool, "nfree");
        g_err |= cbor_encode_uint(&pool, omi.omi_num_free);
        g_err |= cbor_encode_text_stringz(&pool, "min");
        g_err |= cbor_encode_uint(&pool, omi.omi_min_free);
        g_err |= cbor_encoder_close_container(&pools, &pool);
    }

    g_err |= cbor_encoder_close_container(&cb->encoder, &pools);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return (0);
}

static int
smp_datetime_get(struct mgmt_ctxt *cb)
{
    struct os_timeval tv;
    struct os_timezone tz;
    char buf[DATETIME_BUFSIZE];
    int rc;
    CborError g_err = CborNoError;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    /* Display the current datetime */
    rc = os_gettimeofday(&tv, &tz);
    assert(rc == 0);
    rc = datetime_format(&tv, &tz, buf, DATETIME_BUFSIZE);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }
    g_err |= cbor_encode_text_stringz(&cb->encoder, "datetime");
    g_err |= cbor_encode_text_stringz(&cb->encoder, buf);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;

err:
    return (rc);
}

static int
smp_datetime_set(struct mgmt_ctxt *mc)
{
    struct os_timeval tv;
    struct os_timezone tz;
    char buf[DATETIME_BUFSIZE];
    int rc = 0;
    const struct cbor_attr_t datetime_write_attr[] = {
        [0] = {
            .attribute = "datetime",
            .type = CborAttrTextStringType,
            .addr.string = buf,
            .len = sizeof(buf),
        },
        { 0 },
    };

    rc = cbor_read_object(&mc->it, datetime_write_attr);
    if (rc) {
        return MGMT_ERR_EINVAL;
    }

    /* Set the current datetime */
    rc = datetime_parse(buf, &tv, &tz);
    if (!rc) {
        rc = os_settimeofday(&tv, &tz);
        if (rc) {
          return MGMT_ERR_EINVAL;
        }
#if MYNEWT_VAL(TIMEPERSIST)
        timepersist();
#endif

    } else {
        return MGMT_ERR_EINVAL;
    }

    rc = mgmt_write_rsp_status(mc, 0);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

void
smp_os_groups_register(void)
{
    mgmt_register_group(&smp_def_group);
}

void
smp_os_pkg_init(void)
{
    smp_os_groups_register();
}
