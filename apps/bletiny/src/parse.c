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

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include "console/console.h"
#include "host/ble_uuid.h"
#include "bletiny.h"

#define CMD_MAX_ARGS        16

static char *cmd_args[CMD_MAX_ARGS][2];
static int cmd_num_args;

int
parse_err_too_few_args(char *cmd_name)
{
    console_printf("Error: too few arguments for command \"%s\"\n",
                   cmd_name);
    return -1;
}

const struct cmd_entry *
parse_cmd_find(const struct cmd_entry *cmds, char *name)
{
    const struct cmd_entry *cmd;
    int i;

    for (i = 0; cmds[i].name != NULL; i++) {
        cmd = cmds + i;
        if (strcmp(name, cmd->name) == 0) {
            return cmd;
        }
    }

    return NULL;
}

struct kv_pair *
parse_kv_find(struct kv_pair *kvs, char *name)
{
    struct kv_pair *kv;
    int i;

    for (i = 0; kvs[i].key != NULL; i++) {
        kv = kvs + i;
        if (strcmp(name, kv->key) == 0) {
            return kv;
        }
    }

    return NULL;
}

int
parse_arg_find_idx(const char *key)
{
    int i;

    for (i = 0; i < cmd_num_args; i++) {
        if (strcmp(cmd_args[i][0], key) == 0) {
            return i;
        }
    }

    return -1;
}

char *
parse_arg_extract(const char *key)
{
    int i;

    for (i = 0; i < cmd_num_args; i++) {
        if (strcmp(cmd_args[i][0], key) == 0) {
            /* Erase parameter. */
            cmd_args[i][0][0] = '\0';

            return cmd_args[i][1];
        }
    }

    return NULL;
}

/**
 * Determines which number base to use when parsing the specified numeric
 * string.  This just avoids base '0' so that numbers don't get interpreted as
 * octal.
 */
static int
parse_arg_long_base(char *sval)
{
    if (sval[0] == '0' && sval[1] == 'x') {
        return 0;
    } else {
        return 10;
    }
}

long
parse_arg_long_bounds(char *name, long min, long max, int *out_status)
{
    char *endptr;
    char *sval;
    long lval;

    sval = parse_arg_extract(name);
    if (sval == NULL) {
        *out_status = ENOENT;
        return 0;
    }

    lval = strtol(sval, &endptr, parse_arg_long_base(sval));
    if (sval[0] != '\0' && *endptr == '\0' &&
        lval >= min && lval <= max) {

        *out_status = 0;
        return lval;
    }

    *out_status = EINVAL;
    return 0;
}

long
parse_arg_long_bounds_default(char *name, long min, long max,
                              long dflt, int *out_status)
{
    long val;
    int rc;

    val = parse_arg_long_bounds(name, min, max, &rc);
    if (rc == ENOENT) {
        rc = 0;
        val = dflt;
    }

    *out_status = rc;

    return val;
}

uint64_t
parse_arg_uint64_bounds(char *name, uint64_t min, uint64_t max, int *out_status)
{
    char *endptr;
    char *sval;
    uint64_t lval;

    sval = parse_arg_extract(name);
    if (sval == NULL) {
        *out_status = ENOENT;
        return 0;
    }

    lval = strtoull(sval, &endptr, parse_arg_long_base(sval));
    if (sval[0] != '\0' && *endptr == '\0' &&
        lval >= min && lval <= max) {

        *out_status = 0;
        return lval;
    }

    *out_status = EINVAL;
    return 0;
}

long
parse_arg_long(char *name, int *out_status)
{
    return parse_arg_long_bounds(name, LONG_MIN, LONG_MAX, out_status);
}

uint8_t
parse_arg_bool(char *name, int *out_status)
{
    return parse_arg_long_bounds(name, 0, 1, out_status);
}

uint8_t
parse_arg_bool_default(char *name, uint8_t dflt, int *out_status)
{
    return parse_arg_long_bounds_default(name, 0, 1, dflt, out_status);
}

uint8_t
parse_arg_uint8(char *name, int *out_status)
{
    return parse_arg_long_bounds(name, 0, UINT8_MAX, out_status);
}

uint16_t
parse_arg_uint16(char *name, int *out_status)
{
    return parse_arg_long_bounds(name, 0, UINT16_MAX, out_status);
}

uint32_t
parse_arg_uint32(char *name, int *out_status)
{
    return parse_arg_uint64_bounds(name, 0, UINT32_MAX, out_status);
}

uint64_t
parse_arg_uint64(char *name, int *out_status)
{
    return parse_arg_uint64_bounds(name, 0, UINT64_MAX, out_status);
}

uint8_t
parse_arg_uint8_dflt(char *name, uint8_t dflt, int *out_status)
{
    uint8_t val;
    int rc;

    val = parse_arg_uint8(name, &rc);
    if (rc == ENOENT) {
        val = dflt;
        rc = 0;
    }

    *out_status = rc;
    return val;
}

uint16_t
parse_arg_uint16_dflt(char *name, uint16_t dflt, int *out_status)
{
    uint16_t val;
    int rc;

    val = parse_arg_uint16(name, &rc);
    if (rc == ENOENT) {
        val = dflt;
        rc = 0;
    }

    *out_status = rc;
    return val;
}

uint32_t
parse_arg_uint32_dflt(char *name, uint32_t dflt, int *out_status)
{
    uint32_t val;
    int rc;

    val = parse_arg_uint32(name, &rc);
    if (rc == ENOENT) {
        val = dflt;
        rc = 0;
    }

    *out_status = rc;
    return val;
}

int
parse_arg_kv(char *name, struct kv_pair *kvs, int *out_status)
{
    struct kv_pair *kv;
    char *sval;

    sval = parse_arg_extract(name);
    if (sval == NULL) {
        *out_status = ENOENT;
        return -1;
    }

    kv = parse_kv_find(kvs, sval);
    if (kv == NULL) {
        *out_status = EINVAL;
        return -1;
    }

    *out_status = 0;
    return kv->val;
}

int
parse_arg_kv_default(char *name, struct kv_pair *kvs, int def_val,
                     int *out_status)
{
    int val;
    int rc;

    val = parse_arg_kv(name, kvs, &rc);
    if (rc == ENOENT) {
        rc = 0;
        val = def_val;
    }

    *out_status = rc;

    return val;
}


static int
parse_arg_byte_stream_delim(char *sval, char *delims, int max_len,
                            uint8_t *dst, int *out_len)
{
    unsigned long ul;
    char *endptr;
    char *token;
    int i;

    i = 0;
    for (token = strtok(sval, delims);
         token != NULL;
         token = strtok(NULL, delims)) {

        if (i >= max_len) {
            return EINVAL;
        }

        ul = strtoul(token, &endptr, 16);
        if (sval[0] == '\0' || *endptr != '\0' || ul > UINT8_MAX) {
            return -1;
        }

        dst[i] = ul;
        i++;
    }

    *out_len = i;

    return 0;
}

int
parse_arg_byte_stream(char *name, int max_len, uint8_t *dst, int *out_len)
{
    char *sval;

    sval = parse_arg_extract(name);
    if (sval == NULL) {
        return ENOENT;
    }

    return parse_arg_byte_stream_delim(sval, ":-", max_len, dst, out_len);
}

int
parse_arg_byte_stream_exact_length(char *name, uint8_t *dst, int len)
{
    int actual_len;
    int rc;

    rc = parse_arg_byte_stream(name, len, dst, &actual_len);
    if (rc != 0) {
        return rc;
    }

    if (actual_len != len) {
        return EINVAL;
    }

    return 0;
}

static void
parse_reverse_bytes(uint8_t *bytes, int len)
{
    uint8_t tmp;
    int i;

    for (i = 0; i < len / 2; i++) {
        tmp = bytes[i];
        bytes[i] = bytes[len - i - 1];
        bytes[len - i - 1] = tmp;
    }
}

int
parse_arg_mac(char *name, uint8_t *dst)
{
    int rc;

    rc = parse_arg_byte_stream_exact_length(name, dst, 6);
    if (rc != 0) {
        return rc;
    }

    parse_reverse_bytes(dst, 6);

    return 0;
}

int
parse_arg_uuid(char *str, uint8_t *dst_uuid128)
{
    uint16_t uuid16;
    char *tok;
    int rc;

    uuid16 = parse_arg_uint16(str, &rc);
    switch (rc) {
    case ENOENT:
        return ENOENT;

    case 0:
        rc = ble_uuid_16_to_128(uuid16, dst_uuid128);
        if (rc != 0) {
            return EINVAL;
        } else {
            return 0;
        }

    default:
        /* e7add801-b042-4876-aae1112855353cc1 */
        if (strlen(str) == 35) {
            tok = strtok(str, "-");
            if (tok == NULL) {
                return EINVAL;
            }
            rc = parse_arg_byte_stream_exact_length(tok, dst_uuid128 + 0, 4);
            if (rc != 0) {
                return rc;
            }

            tok = strtok(NULL, "-");
            if (tok == NULL) {
                return EINVAL;
            }
            rc = parse_arg_byte_stream_exact_length(tok, dst_uuid128 + 4, 2);
            if (rc != 0) {
                return rc;
            }

            tok = strtok(NULL, "-");
            if (tok == NULL) {
                return EINVAL;
            }
            rc = parse_arg_byte_stream_exact_length(tok, dst_uuid128 + 6, 2);
            if (rc != 0) {
                return rc;
            }

            tok = strtok(NULL, "-");
            if (tok == NULL) {
                return EINVAL;
            }
            rc = parse_arg_byte_stream_exact_length(tok, dst_uuid128 + 8, 8);
            if (rc != 0) {
                return rc;
            }

            return 0;
        }

        rc = parse_arg_byte_stream_exact_length(str, dst_uuid128, 16);
        return rc;
    }
}

int
parse_arg_all(int argc, char **argv)
{
    char *key;
    char *val;
    int i;

    cmd_num_args = 0;

    for (i = 0; i < argc; i++) {
        key = strtok(argv[i], "=");
        val = strtok(NULL, "=");

        if (key != NULL && val != NULL) {
            if (strlen(key) == 0) {
                console_printf("Error: invalid argument: %s\n", argv[i]);
                return -1;
            }

            if (cmd_num_args >= CMD_MAX_ARGS) {
                console_printf("Error: too many arguments");
                return -1;
            }

            cmd_args[cmd_num_args][0] = key;
            cmd_args[cmd_num_args][1] = val;
            cmd_num_args++;
        }
    }

    return 0;
}
