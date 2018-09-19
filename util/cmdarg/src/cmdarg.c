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

/**
 * util/cmd: utility package for parsing `<key>=<value>` pairs among shell
 * command arguments.
 */
#include <string.h>
#include <limits.h>
#include "defs/error.h"
#include "mn_socket/mn_socket.h"
#include "parse/parse.h"
#include "cmdarg/cmdarg.h"

/**
 * Locates the index of the first '=' character in the provided string.
 */
static int
cmdarg_equals_off(const char *arg)
{
    char *eq;

    eq = strchr(arg, '=');
    if (eq == NULL) {
        return -1;
    }

    return eq - arg;
}

/**
 * Given a key, finds the first argument with the following contents:
 *
 *     <key>=[val]
 *
 * @param argc                  The argument count.
 * @param argv                  The argument list.
 * @param key                   The key to search for.
 * @param out_val               On success, points to the start of the located
 *                                  value string.
 *
 * @return                      Argument index on success; -1 on failure.
 */
static int
cmdarg_find_idx(int argc, char **argv, const char *key, char **out_val)
{
    char *arg;
    int eq_off;
    int i;

    for (i = 0; i < argc; i++) {
        arg = argv[i];
        if (arg != NULL) {
            eq_off = cmdarg_equals_off(arg);
            if (eq_off == -1) {
                if (strcmp(arg, key) == 0) {
                    if (out_val != NULL) {
                        *out_val = NULL;
                    }
                    return i;
                }
            } else {
                if (strncmp(arg, key, eq_off) == 0) {
                    if (out_val != NULL) {
                        *out_val = arg + eq_off + 1;
                    }
                    return i;
                }
            }
        }
    }

    return -1;
}

static char *
cmdarg_find_priv(int argc, char **argv, const char *key, bool erase,
                  char **out_val)
{
    char *arg;
    int idx;

    idx = cmdarg_find_idx(argc, argv, key, out_val);
    if (idx == -1) {
        return NULL;
    }

    arg = argv[idx];

    if (erase) {
        argv[idx] = NULL;
    }

    return arg;
}

char *
cmdarg_find(int argc, char **argv, const char *key, char **out_val)
{
    return cmdarg_find_priv(argc, argv, key, false, out_val);
}

char *
cmdarg_extract(int argc, char **argv, const char *key, char **out_val)
{
    return cmdarg_find_priv(argc, argv, key, true, out_val);
}

int
cmdarg_find_str(int argc, char **argv, const char *key, char **out_s)
{
    const char *arg;

    arg = cmdarg_find(argc, argv, key, out_s);
    if (arg == NULL) {
        return SYS_ENOENT;
    }

    return 0;
}

int
cmdarg_extract_str(int argc, char **argv, const char *key, char **out_s)
{
    const char *arg;

    arg = cmdarg_extract(argc, argv, key, out_s);
    if (arg == NULL) {
        return SYS_ENOENT;
    }

    return 0;
}

static int
cmdarg_find_ll_priv(int argc, char **argv, const char *name,
                     long long min, long long max, bool erase,
                     long long *out_ll)
{
    const char *arg;
    char *val;
    long long ll;
    int rc;

    arg = cmdarg_find_priv(argc, argv, name, erase, &val);
    if (arg == NULL) {
        return SYS_ENOENT;
    }

    ll = parse_ll_bounds(val, min, max, &rc);
    if (rc != 0) {
        return rc;
    }

    if (out_ll != NULL) {
        *out_ll = ll;
    }

    return 0;
}

int
cmdarg_find_ll(int argc, char **argv, const char *key,
                long long min, long long max, long long *out_ll)
{
    return cmdarg_find_ll_priv(argc, argv, key, min, max, false, out_ll);
}

int
cmdarg_extract_ll(int argc, char **argv, const char *key,
                   long long min, long long max, long long *out_ll)
{
    return cmdarg_find_ll_priv(argc, argv, key, min, max, true, out_ll);
}

static int
cmdarg_find_ll_dflt_priv(int argc, char **argv, const char *key,
                          long long min, long long max, long long dflt,
                          bool erase, long long *out_ll)
{
    int rc;

    rc = cmdarg_find_ll_priv(argc, argv, key, min, max, erase, out_ll);
    if (rc == SYS_ENOENT) {
        rc = 0;
        if (out_ll != NULL) {
            *out_ll = dflt;
        }
    }

    return rc;
}

int
cmdarg_find_ll_dflt(int argc, char **argv, const char *key,
                     long long min, long long max, long long dflt,
                     long long *out_ll)
{
    return cmdarg_find_ll_dflt_priv(argc, argv, key, min, max, dflt, false,
                                     out_ll);
}

int
cmdarg_extract_ll_dflt(int argc, char **argv, const char *key,
                        long long min, long long max, long long dflt,
                        long long *out_ll)
{
    return cmdarg_find_ll_dflt_priv(argc, argv, key, min, max, dflt, true,
                                     out_ll);
}

static int
cmdarg_find_ull_priv(int argc, char **argv, const char *key,
                      unsigned long long min, unsigned long long max,
                      bool erase, unsigned long long *out_ull)
{
    unsigned long long ull;
    const char *arg;
    char *val;
    int rc;

    arg = cmdarg_find_priv(argc, argv, key, erase, &val);
    if (arg == NULL) {
        return SYS_ENOENT;
    }

    ull = parse_ull_bounds(val, min, max, &rc);
    if (rc != 0) {
        return rc;
    }

    if (out_ull != NULL) {
        *out_ull = ull;
    }

    return 0;
}

int
cmdarg_find_ull(int argc, char **argv, const char *key,
                 unsigned long long min, unsigned long long max,
                 unsigned long long *out_ull)
{
    return cmdarg_find_ull_priv(argc, argv, key, min, max, false, out_ull);
}

int
cmdarg_extract_ull(int argc, char **argv, const char *key,
                    unsigned long long min, unsigned long long max,
                    unsigned long long *out_ull)
{
    return cmdarg_find_ull_priv(argc, argv, key, min, max, true, out_ull);
}

static int
cmdarg_find_ull_dflt_priv(int argc, char **argv, const char *key,
                           unsigned long long min, unsigned long long max,
                           unsigned long long dflt, bool erase,
                           unsigned long long *out_ull)
{
    int rc;

    rc = cmdarg_find_ull_priv(argc, argv, key, min, max, erase, out_ull);
    if (rc == SYS_ENOENT) {
        rc = 0;
        if (out_ull != NULL) {
            *out_ull = dflt;
        }
    }

    return rc;
}

int
cmdarg_find_ull_dflt(int argc, char **argv, const char *key,
                      unsigned long long min, unsigned long long max,
                      unsigned long long dflt, unsigned long long *out_ull)
{
    return cmdarg_find_ull_dflt_priv(argc, argv, key, min, max, dflt, false,
                                      out_ull);
}

int
cmdarg_extract_ull_dflt(int argc, char **argv, const char *key,
                         unsigned long long min, unsigned long long max,
                         unsigned long long dflt, unsigned long long *out_ull)
{
    return cmdarg_find_ull_dflt_priv(argc, argv, key, min, max, dflt,
                                      true, out_ull);
}

static int
cmdarg_find_bool_priv(int argc, char **argv, const char *key, bool erase,
                       bool *out_b)
{
    const char *arg;
    char *val;
    bool b;
    int rc;

    arg = cmdarg_find_priv(argc, argv, key, erase, &val);
    if (arg == NULL) {
        return SYS_ENOENT;
    }

    b = parse_bool(key, &rc);
    if (rc != 0) {
        return rc;
    }

    if (out_b != NULL) {
        *out_b = b;
    }

    return 0;
}

int
cmdarg_find_bool(int argc, char **argv, const char *key, bool *out_b)
{
    return cmdarg_find_bool_priv(argc, argv, key, false, out_b);
}

int
cmdarg_extract_bool(int argc, char **argv, const char *key, bool *out_b)
{
    return cmdarg_find_bool_priv(argc, argv, key, true, out_b);
}

static int
cmdarg_find_bool_dflt_priv(int argc, char **argv, const char *key,
                            bool dflt, bool erase, bool *out_b)
{
    int rc;

    rc = cmdarg_find_bool_priv(argc, argv, key, erase, out_b);
    if (rc == SYS_ENOENT) {
        rc = 0;
        if (out_b != NULL) {
            *out_b = dflt;
        }
    }

    return rc;
}

int
cmdarg_find_bool_dflt(int argc, char **argv, const char *key, bool dflt,
                       bool *out_b)
{
    return cmdarg_find_bool_dflt_priv(argc, argv, key, dflt, false, out_b);
}

int
cmdarg_extract_bool_dflt(int argc, char **argv, const char *key, bool dflt,
                          bool *out_b)
{
    return cmdarg_find_bool_dflt_priv(argc, argv, key, dflt, true, out_b);
}

static int
cmdarg_find_int_priv(int argc, char **argv, const char *key,
                      int min, int max, bool erase,
                      int *out_i)
{
    long long ll;
    int rc;

    rc = cmdarg_find_ll_priv(argc, argv, key, min, max, erase, &ll);
    if (rc == 0 && out_i != NULL) {
        *out_i = ll;
    }

    return rc;
}

int
cmdarg_find_int(int argc, char **argv, const char *key, int min, int max,
                 int *out_i)
{
    return cmdarg_find_int_priv(argc, argv, key, min, max, false, out_i);
}

int
cmdarg_extract_int(int argc, char **argv, const char *key, int min, int max,
                    int *out_i)
{
    return cmdarg_find_int_priv(argc, argv, key, min, max, true, out_i);
}

static int
cmdarg_find_int_dflt_priv(int argc, char **argv, const char *key,
                           int min, int max, int dflt, bool erase,
                           int *out_i)
{
    int rc;

    rc = cmdarg_find_int_priv(argc, argv, key, min, max, erase, out_i);
    if (rc == SYS_ENOENT) {
        rc = 0;
        if (out_i != NULL) {
            *out_i = dflt;
        }
    }

    return rc;
}

int
cmdarg_find_int_dflt(int argc, char **argv, const char *key,
                      int min, int max, int dflt,
                      int *out_i)
{
    return cmdarg_find_int_dflt_priv(argc, argv, key, min, max, dflt, false,
                                      out_i);
}

int
cmdarg_extract_int_dflt(int argc, char **argv, const char *key,
                         int min, int max, int dflt,
                         int *out_i)
{
    return cmdarg_find_int_dflt_priv(argc, argv, key, min, max, dflt, true,
                                      out_i);
}

static int
cmdarg_find_bytes_priv(int argc, char **argv, const char *key,
                        int max_len, bool erase,
                        uint8_t *out_val, int *out_len)
{
    const char *arg;
    char *val;

    arg = cmdarg_find_priv(argc, argv, key, erase, &val);
    if (arg == NULL) {
        return SYS_ENOENT;
    }

    return parse_byte_stream(val, max_len, out_val, out_len);
}

int
cmdarg_find_bytes(int argc, char **argv, const char *key,
                   int max_len, uint8_t *out_val, int *out_len)
{
    return cmdarg_find_bytes_priv(argc, argv, key, max_len, false, out_val,
                                   out_len);
}

int
cmdarg_extract_bytes(int argc, char **argv, const char *key,
                      int max_len, uint8_t *out_val, int *out_len)
{
    return cmdarg_find_bytes_priv(argc, argv, key, max_len, true, out_val,
                                   out_len);
}

static int
cmdarg_find_eui_priv(int argc, char **argv, const char *key, bool erase,
                      uint8_t *out_eui)
{
    const char *arg;
    char *val;

    arg = cmdarg_find_priv(argc, argv, key, erase, &val);
    if (arg == NULL) {
        return SYS_ENOENT;
    }

    return parse_byte_stream_exact_length_base(val, 16, out_eui, 8);
}

int
cmdarg_find_eui(int argc, char **argv, const char *key, uint8_t *out_eui)
{
    return cmdarg_find_eui_priv(argc, argv, key, false, out_eui);
}

int
cmdarg_extract_eui(int argc, char **argv, const char *key, uint8_t *out_eui)
{
    return cmdarg_find_eui_priv(argc, argv, key, true, out_eui);
}

static int
cmdarg_find_ip6_addr_priv(int argc, char **argv, const char *key,
                           bool erase, struct mn_in6_addr *out_addr)
{
    struct mn_in6_addr addr;
    const char *arg;
    char *val;
    int rc;

    arg = cmdarg_find_priv(argc, argv, key, erase, &val);
    if (arg == NULL) {
        return SYS_ENOENT;
    }

    rc = mn_inet_pton(MN_AF_INET6, val, &addr);
    if (rc != 1) {
        return SYS_EINVAL;
    }

    if (out_addr != NULL) {
        *out_addr = addr;
    }

    return 0;
}

int
cmdarg_find_ip6_addr(int argc, char **argv, const char *key,
                      struct mn_in6_addr *out_addr)
{
    return cmdarg_find_ip6_addr_priv(argc, argv, key, false, out_addr);
}

int
cmdarg_extract_ip6_addr(int argc, char **argv, const char *key,
                         struct mn_in6_addr *out_addr)
{
    return cmdarg_find_ip6_addr_priv(argc, argv, key, true, out_addr);
}

static int
cmdarg_find_ip6_net_priv(int argc, char **argv, const char *key,
                          bool erase, struct mn_in6_addr *out_addr,
                          uint8_t *out_prefix_len)
{
    const char *arg;
    char *val;

    arg = cmdarg_find_priv(argc, argv, key, erase, &val);
    if (arg == NULL || val == NULL) {
        return SYS_ENOENT;
    }

    return parse_ip6_net(val, out_addr, out_prefix_len);
}

int
cmdarg_find_ip6_net(int argc, char **argv, const char *key,
                     struct mn_in6_addr *out_addr, uint8_t *out_prefix_len)
{
    return cmdarg_find_ip6_net_priv(argc, argv, key, false, out_addr,
                                     out_prefix_len);
}

int
cmdarg_extract_ip6_net(int argc, char **argv, const char *key,
                        struct mn_in6_addr *out_addr, uint8_t *out_prefix_len)
{
    return cmdarg_find_ip6_net_priv(argc, argv, key, true, out_addr,
                                     out_prefix_len);
}
