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
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <parse_arg/parse_arg.h>
#include <syscfg/syscfg.h>

#define CMD_MAX_ARGS        MYNEWT_VAL(PARSE_ARG_MAX_ARGS)

static char *cmd_args[CMD_MAX_ARGS][2];
static int cmd_num_args;

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
parse_arg_peek(const char *key)
{
    int i;

    for (i = 0; i < cmd_num_args; i++) {
        if (strcmp(cmd_args[i][0], key) == 0) {
            return cmd_args[i][1];
        }
    }

    return NULL;
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
parse_arg_long_base(const char *sval)
{
    if (sval[0] == '0' && sval[1] == 'x') {
        return 0;
    } else {
        return 10;
    }
}

static long
parse_long_bounds(const char *sval, long min, long max, int *status)
{
    char *endptr;
    long lval;

    lval = strtol(sval, &endptr, parse_arg_long_base(sval));
    if (sval[0] != '\0' && *endptr == '\0' &&
        lval >= min && lval <= max) {

        *status = 0;
        return lval;
    }

    *status = EINVAL;
    return 0;
}

static long
parse_arg_long_bounds_peek(const char *name, long min, long max, int *status)
{
    char *sval;

    sval = parse_arg_peek(name);
    if (sval == NULL) {
        *status = ENOENT;
        return 0;
    }
    return parse_long_bounds(sval, min, max, status);
}

long
parse_arg_long_bounds(const char *name, long min, long max, int *status)
{
    char *sval;

    sval = parse_arg_extract(name);
    if (sval == NULL) {
        *status = ENOENT;
        return 0;
    }
    return parse_long_bounds(sval, min, max, status);
}

long
parse_arg_long_bounds_dflt(const char *name, long min, long max,
                           long dflt, int *status)
{
    long val;
    int rc;

    val = parse_arg_long_bounds(name, min, max, &rc);
    if (rc == ENOENT) {
        rc = 0;
        val = dflt;
    }

    *status = rc;

    return val;
}

long
parse_arg_long_dflt(const char *name, long dflt, int *status)
{
    return parse_arg_long_bounds_dflt(name, LONG_MIN, LONG_MAX, dflt, status);
}

uint64_t
parse_arg_uint64_bounds(const char *name, uint64_t min, uint64_t max, int *status)
{
    char *endptr;
    char *sval;
    uint64_t lval;

    sval = parse_arg_extract(name);
    if (sval == NULL) {
        *status = ENOENT;
        return 0;
    }

    lval = strtoull(sval, &endptr, parse_arg_long_base(sval));
    if (sval[0] != '\0' && *endptr == '\0' &&
        lval >= min && lval <= max) {

        *status = 0;
        return lval;
    }

    *status = EINVAL;
    return 0;
}

uint64_t
parse_arg_uint64_bounds_dflt(const char *name, uint64_t min,
                             uint64_t max, uint64_t dflt, int *status)
{
    uint64_t val;
    int rc;

    val = parse_arg_uint64_bounds(name, min, max, &rc);
    if (rc == ENOENT) {
        rc = 0;
        val = dflt;
    }

    *status = rc;

    return val;
}

long
parse_arg_long(const char *name, int *status)
{
    return parse_arg_long_bounds(name, LONG_MIN, LONG_MAX, status);
}

bool
parse_arg_bool(const char *name, int *status)
{
    return parse_arg_long_bounds(name, 0, 1, status);
}

bool
parse_arg_bool_dflt(const char *name, bool dflt, int *status)
{
    return parse_arg_long_bounds_dflt(name, 0, 1, dflt ? 1 : 0, status);
}

uint8_t
parse_arg_uint8(const char *name, int *status)
{
    return parse_arg_long_bounds(name, 0, UINT8_MAX, status);
}

uint16_t
parse_arg_uint16(const char *name, int *status)
{
    return parse_arg_long_bounds(name, 0, UINT16_MAX, status);
}

uint16_t
parse_arg_uint16_peek(char *name, int *status)
{
    return parse_arg_long_bounds_peek(name, 0, UINT16_MAX, status);
}

uint32_t
parse_arg_uint32(const char *name, int *status)
{
    return parse_arg_uint64_bounds(name, 0, UINT32_MAX, status);
}

uint64_t
parse_arg_uint64(const char *name, int *status)
{
    return parse_arg_uint64_bounds(name, 0, UINT64_MAX, status);
}

uint64_t
parse_arg_uint64_dflt(const char *name, uint64_t dflt, int *status)
{
    uint64_t val;
    int rc;

    val = parse_arg_uint64(name, &rc);
    if (rc == ENOENT) {
        val = dflt;
        rc = 0;
    }

    *status = rc;
    return val;
}

uint8_t
parse_arg_uint8_dflt(const char *name, uint8_t dflt, int *status)
{
    uint8_t val;
    int rc;

    val = parse_arg_uint8(name, &rc);
    if (rc == ENOENT) {
        val = dflt;
        rc = 0;
    }

    *status = rc;
    return val;
}

uint8_t
parse_arg_uint8_bounds(const char *name, uint8_t min, uint8_t max,
                       int *status)
{
    return parse_arg_long_bounds(name, 0, UINT8_MAX, status);
}

uint8_t
parse_arg_uint8_bounds_dflt(const char *name, uint8_t min, uint8_t max,
                            uint8_t dflt, int *status)
{
    return parse_arg_long_bounds_dflt(name, 0, UINT8_MAX, dflt, status);
}

uint16_t
parse_arg_uint16_dflt(const char *name, uint16_t dflt, int *status)
{
    uint16_t val;
    int rc;

    val = parse_arg_uint16(name, &rc);
    if (rc == ENOENT) {
        val = dflt;
        rc = 0;
    }

    *status = rc;
    return val;
}

uint16_t
parse_arg_uint16_bounds(const char *name, uint16_t min, uint16_t max,
                        int *status)
{
    return parse_arg_long_bounds(name, 0, UINT16_MAX, status);
}

uint16_t
parse_arg_uint16_bounds_dflt(const char *name, uint16_t min, uint16_t max,
                             uint16_t dflt, int *status)
{
    return parse_arg_long_bounds_dflt(name, 0, UINT16_MAX, dflt, status);
}

uint32_t
parse_arg_uint32_dflt(const char *name, uint32_t dflt, int *status)
{
    uint32_t val;
    int rc;

    val = parse_arg_uint32(name, &rc);
    if (rc == ENOENT) {
        val = dflt;
        rc = 0;
    }

    *status = rc;
    return val;
}

uint32_t
parse_arg_uint32_bounds(const char *name, uint32_t min, uint32_t max, int *status)
{
    return parse_arg_uint64_bounds(name, 0, UINT32_MAX, status);
}

uint32_t
parse_arg_uint32_bounds_dflt(const char *name, uint32_t min, uint32_t max,
                             uint32_t dflt, int *status)
{
    return parse_arg_uint64_bounds_dflt(name, 0, UINT32_MAX, dflt, status);
}

static uint32_t
parse_time_unit_mult(const char *str)
{
    if (!strcasecmp(str, "us")) {
        return 1;
    } else if (!strcasecmp(str, "ms")) {
        return 1000;
    } else if (!strcasecmp(str, "s")) {
        return 1000000;
    }

    return 0;
}

static uint32_t
parse_time_us(const char *str, int *status)
{
    uint32_t val = 0;
    uint32_t val_div = 1;
    uint32_t val_mult = 1;
    uint32_t val_us;

    while (isdigit((unsigned char)*str)) {
        val *= 10;
        val += *str - '0';
        str++;
    }

    if (*str == '.') {
        str++;
        while (isdigit((unsigned char)*str)) {
            val *= 10;
            val += *str - '0';
            val_div *= 10;
            str++;
        }
    }

    val_mult = parse_time_unit_mult(str);
    if (val_mult == 0) {
        *status = EINVAL;
        return 0;
    }

    if (val_mult > val_div) {
        val_us = val * (val_mult / val_div);
    } else {
        val_us = val * (val_div / val_mult);
    }

    *status = 0;

    return val_us;
}

uint32_t
parse_arg_time_dflt(const char *name, unsigned int step_us, uint32_t dflt, int *status)
{
    const char *arg;
    uint32_t val;
    int rc;

    arg = parse_arg_peek(name);
    if (!arg) {
        *status = 0;
        return dflt;
    }

    val = parse_time_us(arg, &rc);
    if (rc) {
        val = parse_arg_uint32(name, &rc);
        if (rc == ENOENT) {
            *status = 0;
            return dflt;
        }
    } else {
        val /= step_us;
        parse_arg_extract(name);
    }

    *status = rc;
    return val;
}

static const struct parse_arg_kv_pair *
parse_kv_find(const struct parse_arg_kv_pair *kvs, const char *name)
{
    const struct parse_arg_kv_pair *kv;
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
parse_arg_kv(const char *name, const struct parse_arg_kv_pair *kvs, int *status)
{
    const struct parse_arg_kv_pair *kv;
    char *sval;

    sval = parse_arg_extract(name);
    if (sval == NULL) {
        *status = ENOENT;
        return -1;
    }

    kv = parse_kv_find(kvs, sval);
    if (kv == NULL) {
        *status = EINVAL;
        return -1;
    }

    *status = 0;
    return kv->val;
}

int
parse_arg_kv_dflt(const char *name, const struct parse_arg_kv_pair *kvs,
                  int def_val, int *status)
{
    int val;
    int rc;

    val = parse_arg_kv(name, kvs, &rc);
    if (rc == ENOENT) {
        rc = 0;
        val = def_val;
    }

    *status = rc;

    return val;
}


static int
parse_arg_byte_stream_delim(char *sval, const char *delims, int max_len,
                            uint8_t *dst, unsigned int *out_len)
{
    unsigned long ul;
    char *endptr;
    char *token;
    unsigned int i;
    char *tok_ptr;

    i = 0;
    for (token = strtok_r(sval, delims, &tok_ptr);
         token != NULL;
         token = strtok_r(NULL, delims, &tok_ptr)) {

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
parse_arg_byte_stream_custom(const char *name, const char *delims,
                             unsigned int dst_size, uint8_t *dst,
                             unsigned int expected_size,
                             unsigned int *out_size)
{
    unsigned int actual_size;
    char *sval;
    int rc;

    sval = parse_arg_extract(name);
    if (sval == NULL) {
        return ENOENT;
    }

    rc = parse_arg_byte_stream_delim(sval, delims, dst_size, dst, &actual_size);
    if (rc != 0) {
        return rc;
    }

    if (expected_size > 0 && expected_size != actual_size) {
        return EINVAL;
    }

    if (out_size) {
        *out_size = actual_size;
    }

    return 0;
}

int
parse_arg_byte_stream(const char *name, unsigned int dst_size, uint8_t *dst,
                      unsigned int *out_size)
{
    return parse_arg_byte_stream_custom(name, ":-", dst_size, dst, 0, out_size);
}

int
parse_arg_byte_stream_exact_length(const char *name, uint8_t *dst, unsigned int len)
{
    return parse_arg_byte_stream_custom(name, ":-", len, dst, len, NULL);
}

static void
parse_reverse_bytes(uint8_t *bytes, size_t len)
{
    uint8_t tmp;
    size_t i;

    for (i = 0; i < len / 2; i++) {
        tmp = bytes[i];
        bytes[i] = bytes[len - i - 1];
        bytes[len - i - 1] = tmp;
    }
}

int
parse_arg_mac_addr(const char *name, uint8_t addr[6])
{
    int rc;

    rc = parse_arg_byte_stream_custom(name, ":-", 6, addr, 6, NULL);
    if (rc != 0) {
        return rc;
    }

    parse_reverse_bytes(addr, 6);

    return 0;
}

#if MYNEWT_VAL(BLE_HOST)
int
parse_arg_ble_addr(const char *name, ble_addr_t *addr)
{
    char *arg;
    size_t len;
    uint8_t addr_type;
    bool addr_type_found;
    int rc;

    arg = parse_arg_peek(name);
    if (!arg) {
        return ENOENT;
    }

    len = strlen(arg);
    if (len < 2) {
        return EINVAL;
    }

    addr_type_found = false;
    if ((arg[len - 2] == ':') || (arg[len - 2] == '-')) {
        if (tolower(arg[len - 1]) == 'p') {
            addr_type = BLE_ADDR_PUBLIC;
            addr_type_found = true;
        } else if (tolower(arg[len - 1]) == 'r') {
            addr_type = BLE_ADDR_RANDOM;
            addr_type_found = true;
        }

        if (addr_type_found) {
            arg[len - 2] = '\0';
        }
    }

    rc = parse_arg_mac_addr(name, addr->val);
    if (rc != 0) {
        return rc;
    }

    if (addr_type_found) {
        addr->type = addr_type;
    } else {
        rc = EAGAIN;
    }

    return rc;
}

int
parse_arg_ble_uuid(const char *str, ble_uuid_any_t *uuid)
{
    uint16_t uuid16;
    uint8_t val[16];
    int len;
    int rc;

    uuid16 = parse_arg_long_bounds_peek(str, 0, UINT16_MAX, &rc);
    switch (rc) {
    case ENOENT:
        parse_arg_extract(str);
        return ENOENT;

    case 0:
        len = 2;
        val[0] = uuid16;
        val[1] = uuid16 >> 8;
        parse_arg_extract(str);
        break;

    default:
        len = 16;
        rc = parse_arg_byte_stream_exact_length(str, val, 16);
        if (rc != 0) {
            return EINVAL;
        }
        parse_reverse_bytes(val, 16);
        break;
    }

    rc = ble_uuid_init_from_buf(uuid, val, len);
    if (rc != 0) {
        return EINVAL;
    } else {
        return 0;
    }
}
#endif

int
parse_arg_init(int argc, char **argv)
{
    char *key;
    char *val;
    int i;
    char *tok_ptr;

    cmd_num_args = 0;

    for (i = 0; i < argc; i++) {
        key = strtok_r(argv[i], "=", &tok_ptr);
        val = strtok_r(NULL, "=", &tok_ptr);

        if (key != NULL && val != NULL) {
            if (strlen(key) == 0) {
                return -1;
            }

            if (cmd_num_args >= CMD_MAX_ARGS) {
                return -1;
            }

            cmd_args[cmd_num_args][0] = key;
            cmd_args[cmd_num_args][1] = val;
            cmd_num_args++;
        }
    }

    return 0;
}
