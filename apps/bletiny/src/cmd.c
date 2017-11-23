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

#include <errno.h>
#include <string.h>
#include "bsp/bsp.h"
#include "console/console.h"
#include "shell/shell.h"

#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "nimble/hci_common.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "host/ble_eddystone.h"
#include "host/ble_hs_id.h"
#include "services/gatt/ble_svc_gatt.h"
#include "../src/ble_l2cap_priv.h"
#include "../src/ble_hs_priv.h"

#include "bletiny.h"

#define CMD_BUF_SZ      256

static int cmd_b_exec(int argc, char **argv);
static struct shell_cmd cmd_b = {
    .sc_cmd = "b",
    .sc_cmd_func = cmd_b_exec
};

static bssnz_t uint8_t cmd_buf[CMD_BUF_SZ];

static struct kv_pair cmd_own_addr_types[] = {
    { "public",     BLE_OWN_ADDR_PUBLIC },
    { "random",     BLE_OWN_ADDR_RANDOM },
    { "rpa_pub",    BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT },
    { "rpa_rnd",    BLE_OWN_ADDR_RPA_RANDOM_DEFAULT },
    { NULL }
};

static struct kv_pair cmd_peer_addr_types[] = {
    { "public",     BLE_ADDR_PUBLIC },
    { "random",     BLE_ADDR_RANDOM },
    { "public_id",  BLE_ADDR_PUBLIC_ID },
    { "random_id",  BLE_ADDR_RANDOM_ID },
    { NULL }
};

static struct kv_pair cmd_addr_type[] = {
    { "public",     BLE_ADDR_PUBLIC },
    { "random",     BLE_ADDR_RANDOM },
    { NULL }
};

/*****************************************************************************
 * $misc                                                                     *
 *****************************************************************************/

static int
cmd_exec(const struct cmd_entry *cmds, int argc, char **argv)
{
    const struct cmd_entry *cmd;
    int rc;

    if (argc <= 1) {
        return parse_err_too_few_args(argv[0]);
    }

    cmd = parse_cmd_find(cmds, argv[1]);
    if (cmd == NULL) {
        console_printf("Error: unknown %s command: %s\n", argv[0], argv[1]);
        return -1;
    }

    rc = cmd->cb(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static void
cmd_print_dsc(struct bletiny_dsc *dsc)
{
    console_printf("            dsc_handle=%d uuid=", dsc->dsc.handle);
    print_uuid(&dsc->dsc.uuid.u);
    console_printf("\n");
}

static void
cmd_print_chr(struct bletiny_chr *chr)
{
    struct bletiny_dsc *dsc;

    console_printf("        def_handle=%d val_handle=%d properties=0x%02x "
                   "uuid=", chr->chr.def_handle, chr->chr.val_handle,
                   chr->chr.properties);
    print_uuid(&chr->chr.uuid.u);
    console_printf("\n");

    SLIST_FOREACH(dsc, &chr->dscs, next) {
        cmd_print_dsc(dsc);
    }
}

static void
cmd_print_svc(struct bletiny_svc *svc)
{
    struct bletiny_chr *chr;

    console_printf("    start=%d end=%d uuid=", svc->svc.start_handle,
                   svc->svc.end_handle);
    print_uuid(&svc->svc.uuid.u);
    console_printf("\n");

    SLIST_FOREACH(chr, &svc->chrs, next) {
        cmd_print_chr(chr);
    }
}

static int
cmd_parse_conn_start_end(uint16_t *out_conn, uint16_t *out_start,
                         uint16_t *out_end)
{
    int rc;

    *out_conn = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    *out_start = parse_arg_uint16("start", &rc);
    if (rc != 0) {
        return rc;
    }

    *out_end = parse_arg_uint16("end", &rc);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
cmd_parse_eddystone_url(char *full_url, uint8_t *out_scheme, char *out_body,
                        uint8_t *out_body_len, uint8_t *out_suffix)
{
    static const struct {
        char *s;
        uint8_t scheme;
    } schemes[] = {
        { "http://www.", BLE_EDDYSTONE_URL_SCHEME_HTTP_WWW },
        { "https://www.", BLE_EDDYSTONE_URL_SCHEME_HTTPS_WWW },
        { "http://", BLE_EDDYSTONE_URL_SCHEME_HTTP },
        { "https://", BLE_EDDYSTONE_URL_SCHEME_HTTPS },
    };

    static const struct {
        char *s;
        uint8_t code;
    } suffixes[] = {
        { ".com/", BLE_EDDYSTONE_URL_SUFFIX_COM_SLASH },
        { ".org/", BLE_EDDYSTONE_URL_SUFFIX_ORG_SLASH },
        { ".edu/", BLE_EDDYSTONE_URL_SUFFIX_EDU_SLASH },
        { ".net/", BLE_EDDYSTONE_URL_SUFFIX_NET_SLASH },
        { ".info/", BLE_EDDYSTONE_URL_SUFFIX_INFO_SLASH },
        { ".biz/", BLE_EDDYSTONE_URL_SUFFIX_BIZ_SLASH },
        { ".gov/", BLE_EDDYSTONE_URL_SUFFIX_GOV_SLASH },
        { ".com", BLE_EDDYSTONE_URL_SUFFIX_COM },
        { ".org", BLE_EDDYSTONE_URL_SUFFIX_ORG },
        { ".edu", BLE_EDDYSTONE_URL_SUFFIX_EDU },
        { ".net", BLE_EDDYSTONE_URL_SUFFIX_NET },
        { ".info", BLE_EDDYSTONE_URL_SUFFIX_INFO },
        { ".biz", BLE_EDDYSTONE_URL_SUFFIX_BIZ },
        { ".gov", BLE_EDDYSTONE_URL_SUFFIX_GOV },
    };

    char *prefix;
    char *suffix;
    int full_url_len;
    int prefix_len;
    int suffix_len;
    int suffix_idx;
    int rc;
    int i;

    full_url_len = strlen(full_url);

    rc = BLE_HS_EINVAL;
    for (i = 0; i < sizeof schemes / sizeof schemes[0]; i++) {
        prefix = schemes[i].s;
        prefix_len = strlen(schemes[i].s);

        if (full_url_len >= prefix_len &&
            memcmp(full_url, prefix, prefix_len) == 0) {

            *out_scheme = i;
            rc = 0;
            break;
        }
    }
    if (rc != 0) {
        return rc;
    }

    rc = BLE_HS_EINVAL;
    for (i = 0; i < sizeof suffixes / sizeof suffixes[0]; i++) {
        suffix = suffixes[i].s;
        suffix_len = strlen(suffixes[i].s);

        suffix_idx = full_url_len - suffix_len;
        if (suffix_idx >= prefix_len &&
            memcmp(full_url + suffix_idx, suffix, suffix_len) == 0) {

            *out_suffix = i;
            rc = 0;
            break;
        }
    }
    if (rc != 0) {
        *out_suffix = BLE_EDDYSTONE_URL_SUFFIX_NONE;
        *out_body_len = full_url_len - prefix_len;
    } else {
        *out_body_len = full_url_len - prefix_len - suffix_len;
    }

    memcpy(out_body, full_url + prefix_len, *out_body_len);

    return 0;
}

/*****************************************************************************
 * $advertise                                                                *
 *****************************************************************************/

static struct kv_pair cmd_adv_conn_modes[] = {
    { "non", BLE_GAP_CONN_MODE_NON },
    { "und", BLE_GAP_CONN_MODE_UND },
    { "dir", BLE_GAP_CONN_MODE_DIR },
    { NULL }
};

static struct kv_pair cmd_adv_disc_modes[] = {
    { "non", BLE_GAP_DISC_MODE_NON },
    { "ltd", BLE_GAP_DISC_MODE_LTD },
    { "gen", BLE_GAP_DISC_MODE_GEN },
    { NULL }
};

static struct kv_pair cmd_adv_filt_types[] = {
    { "none", BLE_HCI_ADV_FILT_NONE },
    { "scan", BLE_HCI_ADV_FILT_SCAN },
    { "conn", BLE_HCI_ADV_FILT_CONN },
    { "both", BLE_HCI_ADV_FILT_BOTH },
    { NULL }
};

static void
print_enumerate_options(struct kv_pair *options)
{
    int i;
    for (i = 0; options[i].key != NULL; i++) {
        if (i != 0) {
            console_printf("|");
        }
        console_printf("%s(%d)", options[i].key, options[i].val);
    }
}

static void
help_cmd_long_bounds(const char *cmd_name, long min, long max)
{
    console_printf("\t%s=<%ld-%ld>\n", cmd_name, min, max);
}

static void
help_cmd_long_bounds_dflt(const char *cmd_name, long min, long max, long dflt)
{
    console_printf("\t%s=[%ld-%ld] default=%ld\n", cmd_name, min, max, dflt);
}

static void
help_cmd_val(const char *cmd_name)
{
    console_printf("\t%s=<val>\n", cmd_name);
}

static void
help_cmd_long(const char *cmd_name)
{
    help_cmd_val(cmd_name);
}

static void
help_cmd_bool(const char *cmd_name)
{
    console_printf("\t%s=<0|1>\n", cmd_name);
}

static void
help_cmd_bool_dflt(const char *cmd_name, bool dflt)
{
    console_printf("\t%s=[0|1] default=%d\n", cmd_name, dflt);
}

static void
help_cmd_uint8(const char *cmd_name)
{
    help_cmd_val(cmd_name);
}

static void
help_cmd_uint8_dflt(const char *cmd_name, uint8_t dflt)
{
    console_printf("\t%s=[val] default=%u\n", cmd_name, dflt);
}

static void
help_cmd_uint16(const char *cmd_name)
{
    help_cmd_val(cmd_name);
}

static void
help_cmd_uint16_dflt(const char *cmd_name, uint16_t dflt)
{
    console_printf("\t%s=[val] default=%u\n", cmd_name, dflt);
}

static void
help_cmd_uint32(const char *cmd_name)
{
    help_cmd_val(cmd_name);
}

static void
help_cmd_uint64(const char *cmd_name)
{
    help_cmd_val(cmd_name);
}

static void
help_cmd_kv(const char *cmd_name, struct kv_pair *options)
{
    console_printf("\t%s=<", cmd_name);
    print_enumerate_options(options);
    console_printf(">\n");
}

static void
help_cmd_kv_dflt(const char *cmd_name, struct kv_pair *options, int dflt)
{
    console_printf("\t%s=[", cmd_name);
    print_enumerate_options(options);
    console_printf("] default=%d\n", dflt);
}

static void
help_cmd_byte_stream(const char *cmd_name)
{
    console_printf("\t%s=<xx:xx:xx: ...>\n", cmd_name);
}

static void
help_cmd_byte_stream_exact_length(const char *cmd_name, int len)
{
    console_printf("\t%s=<xx:xx:xx: ...> len=%d\n", cmd_name, len);
}

static void
help_cmd_uuid(const char *cmd_name)
{
    console_printf("\t%s=<UUID>\n", cmd_name);
}

static void
help_cmd_extract(const char *cmd_name)
{
    console_printf("\t%s=<str>\n", cmd_name);
}

static void
help_cmd_conn_start_end(void)
{
    console_printf("\t%s=<val> %s=<val> %s=<val>\n", "conn", "start", "end");
}

#if !MYNEWT_VAL(BLETINY_HELP)
static void
bletiny_help_disabled(void)
{
    console_printf("bletiny help is disabled in this build\n");
}
#endif

static void
bletiny_adv_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available adv commands: \n");
    console_printf("\thelp\n");
    console_printf("\tstop\n");
    console_printf("Available adv params: \n");
    help_cmd_kv_dflt("conn", cmd_adv_conn_modes, BLE_GAP_CONN_MODE_UND);
    help_cmd_kv_dflt("disc", cmd_adv_disc_modes, BLE_GAP_DISC_MODE_GEN);
    help_cmd_kv_dflt("peer_addr_type", cmd_peer_addr_types, BLE_ADDR_PUBLIC);
    help_cmd_byte_stream_exact_length("peer_addr", 6);
    help_cmd_kv_dflt("own_addr_type", cmd_own_addr_types,
                     BLE_OWN_ADDR_PUBLIC);
    help_cmd_long_bounds_dflt("chan_map", 0, 0xff, 0);
    help_cmd_kv_dflt("filt", cmd_adv_filt_types, BLE_HCI_ADV_FILT_NONE);
    help_cmd_long_bounds_dflt("itvl_min", 0, UINT16_MAX, 0);
    help_cmd_long_bounds_dflt("itvl_max", 0, UINT16_MAX, 0);
    help_cmd_long_bounds_dflt("hd", 0, 1, 0);
    help_cmd_long_bounds_dflt("dur", 1, INT32_MAX, BLE_HS_FOREVER);
}

static int
cmd_adv(int argc, char **argv)
{
    struct ble_gap_adv_params params;
    int32_t duration_ms;
    ble_addr_t peer_addr;
    ble_addr_t *peer_addr_param = &peer_addr;
    uint8_t own_addr_type;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_adv_help();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "stop") == 0) {
        rc = bletiny_adv_stop();
        if (rc != 0) {
            console_printf("advertise stop fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    params.conn_mode = parse_arg_kv_default("conn", cmd_adv_conn_modes,
                                            BLE_GAP_CONN_MODE_UND, &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_kv_dflt("conn", cmd_adv_conn_modes, BLE_GAP_CONN_MODE_UND);
        return rc;
    }

    params.disc_mode = parse_arg_kv_default("disc", cmd_adv_disc_modes,
                                            BLE_GAP_DISC_MODE_GEN, &rc);
    if (rc != 0) {
        console_printf("invalid 'disc' parameter\n");
        help_cmd_kv_dflt("disc", cmd_adv_disc_modes, BLE_GAP_DISC_MODE_GEN);
        return rc;
    }

    peer_addr.type = parse_arg_kv_default(
        "peer_addr_type", cmd_peer_addr_types, BLE_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'peer_addr_type' parameter\n");
        help_cmd_kv_dflt("peer_addr_type", cmd_peer_addr_types,
                         BLE_ADDR_PUBLIC);
        return rc;
    }

    rc = parse_arg_mac("peer_addr", peer_addr.val);
    if (rc == ENOENT) {
        peer_addr_param = NULL;
    } else if (rc != 0) {
        console_printf("invalid 'peer_addr' parameter\n");
        help_cmd_byte_stream_exact_length("peer_addr", 6);
        return rc;
    }

    own_addr_type = parse_arg_kv_default(
        "own_addr_type", cmd_own_addr_types, BLE_OWN_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'own_addr_type' parameter\n");
        help_cmd_kv_dflt("own_addr_type", cmd_own_addr_types,
                         BLE_OWN_ADDR_PUBLIC);
        return rc;
    }

    params.channel_map = parse_arg_long_bounds_default("chan_map", 0, 0xff, 0,
                                                       &rc);
    if (rc != 0) {
        console_printf("invalid 'chan_map' parameter\n");
        help_cmd_long_bounds_dflt("chan_map", 0, 0xff, 0);
        return rc;
    }

    params.filter_policy = parse_arg_kv_default("filt", cmd_adv_filt_types,
                                                BLE_HCI_ADV_FILT_NONE, &rc);
    if (rc != 0) {
        console_printf("invalid 'filt' parameter\n");
        help_cmd_kv_dflt("filt", cmd_adv_filt_types, BLE_HCI_ADV_FILT_NONE);
        return rc;
    }

    params.itvl_min = parse_arg_long_bounds_default("itvl_min", 0, UINT16_MAX,
                                                    0, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_min' parameter\n");
        help_cmd_long_bounds_dflt("itvl_min", 0, UINT16_MAX, 0);
        return rc;
    }

    params.itvl_max = parse_arg_long_bounds_default("itvl_max", 0, UINT16_MAX,
                                                    0, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_max' parameter\n");
        help_cmd_long_bounds_dflt("itvl_max", 0, UINT16_MAX, 0);
        return rc;
    }

    params.high_duty_cycle = parse_arg_long_bounds_default("hd", 0, 1, 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'hd' parameter\n");
        help_cmd_long_bounds_dflt("hd", 0, 1, 0);
        return rc;
    }

    duration_ms = parse_arg_long_bounds_default("dur", 1, INT32_MAX,
                                                BLE_HS_FOREVER, &rc);
    if (rc != 0) {
        console_printf("invalid 'dur' parameter\n");
        help_cmd_long_bounds_dflt("dur", 1, INT32_MAX, BLE_HS_FOREVER);
        return rc;
    }

    rc = bletiny_adv_start(own_addr_type, peer_addr_param, duration_ms,
                           &params);
    if (rc != 0) {
        console_printf("advertise fail: %d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $connect                                                                  *
 *****************************************************************************/

static struct kv_pair cmd_ext_phy_opts[] = {
    { "none",        0x00 },
    { "1M",          0x01 },
    { "coded",       0x02 },
    { "both",        0x03 },
    { "all",         0x04 },
    { NULL }
};

static void
bletiny_conn_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available conn commands: \n");
    console_printf("\thelp\n");
    console_printf("\tcancel\n");
    console_printf("Available conn params: \n");
    help_cmd_kv_dflt("ext", cmd_ext_phy_opts, 0);
    help_cmd_kv_dflt("peer_addr_type", cmd_peer_addr_types, BLE_ADDR_PUBLIC);
    help_cmd_byte_stream_exact_length("peer_addr", 6);
    help_cmd_kv_dflt("own_addr_type", cmd_own_addr_types,
                     BLE_OWN_ADDR_PUBLIC);
    help_cmd_uint16_dflt("scan_itvl", 0x0010);
    help_cmd_uint16_dflt("scan_window", 0x0010);
    help_cmd_uint16_dflt("itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
    help_cmd_uint16_dflt("itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
    help_cmd_uint16_dflt("latency", 0);
    help_cmd_uint16_dflt("timeout", 0x0100);
    help_cmd_uint16_dflt("min_ce_len", 0x0010);
    help_cmd_uint16_dflt("max_ce_len", 0x0300);
    help_cmd_long_bounds_dflt("dur", 1, INT32_MAX, 0);
    console_printf("Available conn params when ext != none: \n");
    help_cmd_uint16_dflt("coded_scan_itvl", 0x0010);
    help_cmd_uint16_dflt("coded_scan_window", 0x0010);
    help_cmd_uint16_dflt("coded_itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
    help_cmd_uint16_dflt("coded_itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
    help_cmd_uint16_dflt("coded_latency", 0);
    help_cmd_uint16_dflt("coded_timeout", 0x0100);
    help_cmd_uint16_dflt("coded_min_ce_len", 0x0010);
    help_cmd_uint16_dflt("coded_max_ce_len", 0x0300);
    help_cmd_uint16_dflt("2M_itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
    help_cmd_uint16_dflt("2M_itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
    help_cmd_uint16_dflt("2M_latency", 0);
    help_cmd_uint16_dflt("2M_timeout", 0x0100);
    help_cmd_uint16_dflt("2M_min_ce_len", 0x0010);
    help_cmd_uint16_dflt("2M_max_ce_len", 0x0300);
}

static int
cmd_conn(int argc, char **argv)
{
    struct ble_gap_conn_params phy_1M_params = {0};
    struct ble_gap_conn_params phy_coded_params = {0};
    struct ble_gap_conn_params phy_2M_params = {0};
    int32_t duration_ms;
    ble_addr_t peer_addr;
    ble_addr_t *peer_addr_param = &peer_addr;
    int own_addr_type;
    int rc;
    uint8_t ext;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_conn_help();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "cancel") == 0) {
        rc = bletiny_conn_cancel();
        if (rc != 0) {
            console_printf("connection cancel fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    ext = parse_arg_kv_default("ext", cmd_ext_phy_opts, 0, &rc);
    if (rc != 0) {
        help_cmd_kv_dflt("ext", cmd_ext_phy_opts, 0);
        console_printf("invalid 'ext' parameter\n");
        return rc;
    }

    console_printf("Connection type: %d\n", ext);

    peer_addr.type = parse_arg_kv_default("peer_addr_type", cmd_peer_addr_types,
                                          BLE_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'peer_addr_type' parameter\n");
        help_cmd_kv_dflt("peer_addr_type", cmd_peer_addr_types,
                         BLE_ADDR_PUBLIC);
        return rc;
    }

    rc = parse_arg_mac("peer_addr", peer_addr.val);
    if (rc == ENOENT) {
        /* Allow "addr" for backwards compatibility. */
        rc = parse_arg_mac("addr", peer_addr.val);
    }

    if (rc == ENOENT) {
        /* With no "peer_addr" specified we'll use white list */
        peer_addr_param = NULL;
    } else if (rc != 0) {
        console_printf("invalid 'peer_addr' parameter\n");
        help_cmd_byte_stream_exact_length("peer_addr", 6);
        return rc;
    }

    own_addr_type = parse_arg_kv_default("own_addr_type", cmd_own_addr_types,
                                         BLE_OWN_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'own_addr_type' parameter\n");
        help_cmd_kv_dflt("own_addr_type", cmd_own_addr_types,
                         BLE_OWN_ADDR_PUBLIC);
        return rc;
    }

    phy_1M_params.scan_itvl = parse_arg_uint16_dflt("scan_itvl", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'scan_itvl' parameter\n");
        help_cmd_uint16_dflt("scan_itvl", 0x0010);
        return rc;
    }

    phy_1M_params.scan_window = parse_arg_uint16_dflt("scan_window", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'scan_window' parameter\n");
        help_cmd_uint16_dflt("scan_window", 0x0010);
        return rc;
    }

    phy_1M_params.itvl_min = parse_arg_uint16_dflt(
        "itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_min' parameter\n");
        help_cmd_uint16_dflt("itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
        return rc;
    }

    phy_1M_params.itvl_max = parse_arg_uint16_dflt(
        "itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_max' parameter\n");
        help_cmd_uint16_dflt("itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
        return rc;
    }

    phy_1M_params.latency = parse_arg_uint16_dflt("latency", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'latency' parameter\n");
        help_cmd_uint16_dflt("latency", 0);
        return rc;
    }

    phy_1M_params.supervision_timeout = parse_arg_uint16_dflt("timeout", 0x0100, &rc);
    if (rc != 0) {
        console_printf("invalid 'timeout' parameter\n");
        help_cmd_uint16_dflt("timeout", 0x0100);
        return rc;
    }

    phy_1M_params.min_ce_len = parse_arg_uint16_dflt("min_ce_len", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'min_ce_len' parameter\n");
        help_cmd_uint16_dflt("min_ce_len", 0x0010);
        return rc;
    }

    phy_1M_params.max_ce_len = parse_arg_uint16_dflt("max_ce_len", 0x0300, &rc);
    if (rc != 0) {
        console_printf("invalid 'max_ce_len' parameter\n");
        help_cmd_uint16_dflt("max_ce_len", 0x0300);
        return rc;
    }

    duration_ms = parse_arg_long_bounds_default("dur", 1, INT32_MAX, 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'dur' parameter\n");
        help_cmd_long_bounds_dflt("dur", 1, INT32_MAX, 0);
        return rc;
    }

    if (ext == 0x00) {
        rc = bletiny_conn_initiate(own_addr_type, peer_addr_param, duration_ms,
                                   &phy_1M_params);
        return rc;
    }

    if (ext == 0x01) {
        rc = bletiny_ext_conn_initiate(own_addr_type, peer_addr_param,
                                       duration_ms, &phy_1M_params, NULL, NULL);
        return rc;
    }

    /* Get coded params */
    phy_coded_params.scan_itvl = parse_arg_uint16_dflt("coded_scan_itvl",
                                                           0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'coded_scan_itvl' parameter\n");
        help_cmd_uint16_dflt("coded_scan_itvl", 0x0010);
        return rc;
    }

    phy_coded_params.scan_window = parse_arg_uint16_dflt("coded_scan_window",
                                                         0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'coded_scan_window' parameter\n");
        help_cmd_uint16_dflt("coded_scan_window", 0x0010);
        return rc;
    }

    phy_coded_params.itvl_min = parse_arg_uint16_dflt("coded_itvl_min",
                                                      BLE_GAP_INITIAL_CONN_ITVL_MIN,
                                                      &rc);
    if (rc != 0) {
        console_printf("invalid 'coded_itvl_min' parameter\n");
        help_cmd_uint16_dflt("coded_itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
        return rc;
    }

    phy_coded_params.itvl_max = parse_arg_uint16_dflt("coded_itvl_max",
                                                      BLE_GAP_INITIAL_CONN_ITVL_MAX,
                                                      &rc);
    if (rc != 0) {
        console_printf("invalid 'coded_itvl_max' parameter\n");
        help_cmd_uint16_dflt("coded_itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
        return rc;
    }

    phy_coded_params.latency =
            parse_arg_uint16_dflt("coded_latency", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'coded_latency' parameter\n");
        help_cmd_uint16_dflt("coded_latency", 0);
        return rc;
    }

    phy_coded_params.supervision_timeout =
            parse_arg_uint16_dflt("coded_timeout", 0x0100, &rc);

    if (rc != 0) {
        console_printf("invalid 'coded_timeout' parameter\n");
        help_cmd_uint16_dflt("coded_timeout", 0x0100);
        return rc;
    }

    phy_coded_params.min_ce_len =
            parse_arg_uint16_dflt("coded_min_ce_len", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'coded_min_ce_len' parameter\n");
        help_cmd_uint16_dflt("coded_min_ce_len", 0x0010);
        return rc;
    }

    phy_coded_params.max_ce_len = parse_arg_uint16_dflt("coded_max_ce_len",
                                                        0x0300, &rc);
    if (rc != 0) {
        console_printf("invalid 'coded_max_ce_len' parameter\n");
        help_cmd_uint16_dflt("coded_max_ce_len", 0x0300);
        return rc;
    }

    /* Get 2M params */
    phy_2M_params.itvl_min = parse_arg_uint16_dflt("2m_itvl_min",
                                                   BLE_GAP_INITIAL_CONN_ITVL_MIN,
                                                   &rc);
    if (rc != 0) {
        console_printf("invalid '2m_itvl_min' parameter\n");
        help_cmd_uint16_dflt("2m_itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
        return rc;
    }

    phy_2M_params.itvl_max = parse_arg_uint16_dflt("2m_itvl_max",
                                  BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        console_printf("invalid '2m_itvl_max' parameter\n");
        help_cmd_uint16_dflt("2m_itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
        return rc;
    }

    phy_2M_params.latency =
            parse_arg_uint16_dflt("2m_latency", 0, &rc);
    if (rc != 0) {
        console_printf("invalid '2m_latency' parameter\n");
        help_cmd_uint16_dflt("2m_latency", 0);
        return rc;
    }

    phy_2M_params.supervision_timeout = parse_arg_uint16_dflt("2m_timeout",
                                                              0x0100, &rc);

    if (rc != 0) {
        console_printf("invalid '2m_timeout' parameter\n");
        help_cmd_uint16_dflt("2m_timeout", 0x0100);
        return rc;
    }

    phy_2M_params.min_ce_len = parse_arg_uint16_dflt("2m_min_ce_len", 0x0010,
                                                     &rc);
    if (rc != 0) {
        console_printf("invalid '2m_min_ce_len' parameter\n");
        help_cmd_uint16_dflt("2m_min_ce_len", 0x0010);
        return rc;
    }

    phy_2M_params.max_ce_len = parse_arg_uint16_dflt("2m_max_ce_len",
                                                        0x0300, &rc);
    if (rc != 0) {
        console_printf("invalid '2m_max_ce_len' parameter\n");
        help_cmd_uint16_dflt("2m_max_ce_len", 0x0300);
        return rc;
    }

    if (ext == 0x02) {
        rc = bletiny_ext_conn_initiate(own_addr_type, peer_addr_param,
                                       duration_ms, NULL, NULL, &phy_coded_params);
        return rc;
    }

    if (ext == 0x03) {
        rc = bletiny_ext_conn_initiate(own_addr_type, peer_addr_param,
                                       duration_ms, &phy_1M_params, NULL,
                                       &phy_coded_params);
        return rc;
    }

    rc = bletiny_ext_conn_initiate(own_addr_type, peer_addr_param,
                                           duration_ms, &phy_1M_params,
                                           &phy_2M_params,
                                           &phy_coded_params);
    return rc;
}

/*****************************************************************************
 * $chrup                                                                    *
 *****************************************************************************/

static void
bletiny_chrup_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available chrup commands: \n");
    console_printf("\thelp\n");
    console_printf("Available chrup params: \n");
    help_cmd_long("attr");
}

static int
cmd_chrup(int argc, char **argv)
{
    uint16_t attr_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_chrup_help();
        return 0;
    }

    attr_handle = parse_arg_long("attr", &rc);
    if (rc != 0) {
        console_printf("invalid 'attr' parameter\n");
        help_cmd_long("attr");
        return rc;
    }

    bletiny_chrup(attr_handle);

    return 0;
}

/*****************************************************************************
 * $datalen                                                                  *
 *****************************************************************************/

static void
bletiny_datalen_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available datalen commands: \n");
    console_printf("\thelp\n");
    console_printf("Available datalen params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint16("octets");
    help_cmd_uint16("time");
}

static int
cmd_datalen(int argc, char **argv)
{
    uint16_t conn_handle;
    uint16_t tx_octets;
    uint16_t tx_time;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_datalen_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    tx_octets = parse_arg_long("octets", &rc);
    if (rc != 0) {
        console_printf("invalid 'octets' parameter\n");
        help_cmd_long("octets");
        return rc;
    }

    tx_time = parse_arg_long("time", &rc);
    if (rc != 0) {
        console_printf("invalid 'time' parameter\n");
        help_cmd_long("time");
        return rc;
    }

    rc = bletiny_datalen(conn_handle, tx_octets, tx_time);
    if (rc != 0) {
        console_printf("error setting data length; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $discover                                                                 *
 *****************************************************************************/

static void
bletiny_disc_chr_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available disc chr commands: \n");
    console_printf("\thelp\n");
    console_printf("Available disc chr params: \n");
    help_cmd_conn_start_end();
    help_cmd_uuid("uuid");
}

static int
cmd_disc_chr(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    ble_uuid_any_t uuid;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_disc_chr_help();
        return 0;
    }

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        console_printf("invalid 'conn start end' parameter\n");
        help_cmd_conn_start_end();
        return rc;
    }

    rc = parse_arg_uuid("uuid", &uuid);
    if (rc == 0) {
        rc = bletiny_disc_chrs_by_uuid(conn_handle, start_handle, end_handle,
                                       &uuid.u);
    } else if (rc == ENOENT) {
        rc = bletiny_disc_all_chrs(conn_handle, start_handle, end_handle);
    } else  {
        console_printf("invalid 'uuid' parameter\n");
        help_cmd_uuid("uuid");
        return rc;
    }
    if (rc != 0) {
        console_printf("error discovering characteristics; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static void
bletiny_disc_dsc_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available disc dsc commands: \n");
    console_printf("\thelp\n");
    console_printf("Available disc dsc params: \n");
    help_cmd_conn_start_end();
}

static int
cmd_disc_dsc(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_disc_dsc_help();
        return 0;
    }

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        console_printf("invalid 'conn start end' parameter\n");
        help_cmd_conn_start_end();
        return rc;
    }

    rc = bletiny_disc_all_dscs(conn_handle, start_handle, end_handle);
    if (rc != 0) {
        console_printf("error discovering descriptors; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static void
bletiny_disc_svc_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available disc svc commands: \n");
    console_printf("\thelp\n");
    console_printf("Available disc svc params: \n");
    help_cmd_uint16("conn");
    help_cmd_uuid("uuid");
}

static int
cmd_disc_svc(int argc, char **argv)
{
    ble_uuid_any_t uuid;
    int conn_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_disc_svc_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    rc = parse_arg_uuid("uuid", &uuid);
    if (rc == 0) {
        rc = bletiny_disc_svc_by_uuid(conn_handle, &uuid.u);
    } else if (rc == ENOENT) {
        rc = bletiny_disc_svcs(conn_handle);
    } else  {
        console_printf("invalid 'uuid' parameter\n");
        help_cmd_uuid("uuid");
        return rc;
    }

    if (rc != 0) {
        console_printf("error discovering services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static void
bletiny_disc_full_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available disc full commands: \n");
    console_printf("\thelp\n");
    console_printf("Available disc full params: \n");
    help_cmd_uint16("conn");
}

static int
cmd_disc_full(int argc, char **argv)
{
    int conn_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_disc_full_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    rc = bletiny_disc_full(conn_handle);
    if (rc != 0) {
        console_printf("error discovering all; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static struct cmd_entry cmd_disc_entries[];

static int
cmd_disc_help(int argc, char **argv)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return 0;
#endif

    int i;

    console_printf("Available disc commands:\n");
    for (i = 0; cmd_disc_entries[i].name != NULL; i++) {
        console_printf("\t%s\n", cmd_disc_entries[i].name);
    }
    return 0;
}

static struct cmd_entry cmd_disc_entries[] = {
    { "chr", cmd_disc_chr },
    { "dsc", cmd_disc_dsc },
    { "svc", cmd_disc_svc },
    { "full", cmd_disc_full },
    { "help", cmd_disc_help },
    { NULL, NULL }
};

static int
cmd_disc(int argc, char **argv)
{
    int rc;

    rc = cmd_exec(cmd_disc_entries, argc, argv);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $find                                                                     *
 *****************************************************************************/

static void
bletiny_find_inc_svcs_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available find inc svcs commands: \n");
    console_printf("\thelp\n");
    console_printf("Available find inc svcs params: \n");
    help_cmd_conn_start_end();
}

static int
cmd_find_inc_svcs(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_find_inc_svcs_help();
        return 0;
    }

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        console_printf("invalid 'conn start end' parameter\n");
        help_cmd_conn_start_end();
        return rc;
    }

    rc = bletiny_find_inc_svcs(conn_handle, start_handle, end_handle);
    if (rc != 0) {
        console_printf("error finding included services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct cmd_entry cmd_find_entries[];

static int
cmd_find_help(int argc, char **argv)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return 0;
#endif

    int i;

    console_printf("Available find commands:\n");
    for (i = 0; cmd_find_entries[i].name != NULL; i++) {
        console_printf("\t%s\n", cmd_find_entries[i].name);
    }
    return 0;
}

static const struct cmd_entry cmd_find_entries[] = {
    { "inc_svcs", cmd_find_inc_svcs },
    { "help", cmd_find_help },
    { NULL, NULL }
};

static int
cmd_find(int argc, char **argv)
{
    int rc;

    rc = cmd_exec(cmd_find_entries, argc, argv);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $l2cap                                                                    *
 *****************************************************************************/

static void
bletiny_l2cap_update_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available l2cap update commands: \n");
    console_printf("\thelp\n");
    console_printf("Available l2cap update params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint16_dflt("itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
    help_cmd_uint16_dflt("itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
    help_cmd_uint16_dflt("latency", 0);
    help_cmd_uint16_dflt("timeout", 0x0100);
}

static int
cmd_l2cap_update(int argc, char **argv)
{
    struct ble_l2cap_sig_update_params params;
    uint16_t conn_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_l2cap_update_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    params.itvl_min = parse_arg_uint16_dflt(
        "itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_min' parameter\n");
        help_cmd_uint16_dflt("itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
        return rc;
    }

    params.itvl_max = parse_arg_uint16_dflt(
        "itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_max' parameter\n");
        help_cmd_uint16_dflt("itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
        return rc;
    }

    params.slave_latency = parse_arg_uint16_dflt("latency", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'latency' parameter\n");
        help_cmd_uint16_dflt("latency", 0);
        return rc;
    }

    params.timeout_multiplier = parse_arg_uint16_dflt("timeout", 0x0100, &rc);
    if (rc != 0) {
        console_printf("invalid 'timeout' parameter\n");
        help_cmd_uint16_dflt("timeout", 0x0100);
        return rc;
    }

    rc = bletiny_l2cap_update(conn_handle, &params);
    if (rc != 0) {
        console_printf("error txing l2cap update; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static void
bletiny_l2cap_create_srv_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available l2cap create_srv commands: \n");
    console_printf("\thelp\n");
    console_printf("Available l2cap create_srv params: \n");
    help_cmd_uint16("psm");
    help_cmd_uint16("mtu");
}

static int
cmd_l2cap_create_srv(int argc, char **argv)
{
    uint16_t psm = 0;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
            bletiny_l2cap_create_srv_help();
        return 0;
    }

    psm = parse_arg_uint16("psm", &rc);
    if (rc != 0) {
        console_printf("invalid 'psm' parameter\n");
        help_cmd_uint16("psm");
        return rc;
    }

    rc = bletiny_l2cap_create_srv(psm);
    if (rc) {
        console_printf("Server create error: 0x%02x", rc);
    }

    return 0;
}

static void
bletiny_l2cap_connect_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available l2cap connect commands: \n");
    console_printf("\thelp\n");
    console_printf("Available l2cap connect params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint16("psm");
}

static int
cmd_l2cap_connect(int argc, char **argv)
{
    uint16_t conn = 0;
    uint16_t psm = 0;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
            bletiny_l2cap_connect_help();
        return 0;
    }

    conn = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    psm = parse_arg_uint16("psm", &rc);
    if (rc != 0) {
        console_printf("invalid 'psm' parameter\n");
        help_cmd_uint16("psm");
        return rc;
    }

    return bletiny_l2cap_connect(conn, psm);
}

static void
bletiny_l2cap_disconnect_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available l2cap disconnect commands: \n");
    console_printf("\thelp\n");
    console_printf("Available l2cap disconnect params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint16("idx");
    console_printf("\n Use 'b show coc' to get those parameters \n");
}

static int
cmd_l2cap_disconnect(int argc, char **argv)
{
    uint16_t conn;
    uint16_t idx;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_l2cap_disconnect_help();
        return 0;
    }

    conn = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    idx = parse_arg_uint16("idx", &rc);
    if (rc != 0) {
        console_printf("invalid 'idx' parameter\n");
        help_cmd_uint16("idx");
        return rc;
    }

    return bletiny_l2cap_disconnect(conn, idx);
}

static void
bletiny_l2cap_send_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available l2cap send commands: \n");
    console_printf("\thelp\n");
    console_printf("Available l2cap disconnect params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint16("idx");
    help_cmd_uint16("bytes");
    console_printf("\n Use 'b show coc' to get conn and idx parameters.\n");
    console_printf("bytes stands for number of bytes to send .\n");
}

static int
cmd_l2cap_send(int argc, char **argv)
{
    uint16_t conn;
    uint16_t idx;
    uint16_t bytes;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_l2cap_send_help();
        return 0;
    }
    conn = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
       console_printf("invalid 'conn' parameter\n");
       help_cmd_uint16("conn");
       return rc;
    }

    idx = parse_arg_uint16("idx", &rc);
    if (rc != 0) {
       console_printf("invalid 'idx' parameter\n");
       help_cmd_uint16("idx");
       return rc;
    }

    bytes = parse_arg_uint16("bytes", &rc);
    if (rc != 0) {
       console_printf("invalid 'bytes' parameter\n");
       help_cmd_uint16("bytes");
       return rc;
    }

    return bletiny_l2cap_send(conn, idx, bytes);
}

static const struct cmd_entry cmd_l2cap_entries[];

static int
cmd_l2cap_help(int argc, char **argv)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return 0;
#endif

    int i;

    console_printf("Available l2cap commands:\n");
    for (i = 0; cmd_l2cap_entries[i].name != NULL; i++) {
        console_printf("\t%s\n", cmd_l2cap_entries[i].name);
    }
    return 0;
}

static const struct cmd_entry cmd_l2cap_entries[] = {
    { "update", cmd_l2cap_update },
    { "create_srv", cmd_l2cap_create_srv },
    { "connect", cmd_l2cap_connect },
    { "disconnect", cmd_l2cap_disconnect },
    { "send", cmd_l2cap_send },
    { "help", cmd_l2cap_help },
    { NULL, NULL }
};

static int
cmd_l2cap(int argc, char **argv)
{
    int rc;

    rc = cmd_exec(cmd_l2cap_entries, argc, argv);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $mtu                                                                      *
 *****************************************************************************/

static void
bletiny_mtu_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available mtu commands: \n");
    console_printf("\thelp\n");
    console_printf("Available mtu params: \n");
    help_cmd_uint16("conn");
}

static int
cmd_mtu(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_mtu_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    rc = bletiny_exchange_mtu(conn_handle);
    if (rc != 0) {
        console_printf("error exchanging mtu; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $read                                                                     *
 *****************************************************************************/

#define CMD_READ_MAX_ATTRS  8

static void
bletiny_read_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available read commands: \n");
    console_printf("\thelp\n");
    console_printf("Available read params: \n");
    help_cmd_uint16("conn");
    help_cmd_long("long");
    help_cmd_uint16("attr");
    help_cmd_uuid("uuid");
    help_cmd_uint16("start");
    help_cmd_uint16("end");
    help_cmd_uint16("offset");
}

static int
cmd_read(int argc, char **argv)
{
    static uint16_t attr_handles[CMD_READ_MAX_ATTRS];
    uint16_t conn_handle;
    uint16_t start;
    uint16_t end;
    uint16_t offset;
    ble_uuid_any_t uuid;
    uint8_t num_attr_handles;
    int is_uuid;
    int is_long;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_read_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    is_long = parse_arg_long("long", &rc);
    if (rc == ENOENT) {
        is_long = 0;
    } else if (rc != 0) {
        console_printf("invalid 'long' parameter\n");
        help_cmd_long("long");
        return rc;
    }

    for (num_attr_handles = 0;
         num_attr_handles < CMD_READ_MAX_ATTRS;
         num_attr_handles++) {

        attr_handles[num_attr_handles] = parse_arg_uint16("attr", &rc);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            console_printf("invalid 'attr' parameter\n");
            help_cmd_uint16("attr");
            return rc;
        }
    }

    rc = parse_arg_uuid("uuid", &uuid);
    if (rc == ENOENT) {
        is_uuid = 0;
    } else if (rc == 0) {
        is_uuid = 1;
    } else {
        console_printf("invalid 'uuid' parameter\n");
        help_cmd_uuid("uuid");
        return rc;
    }

    start = parse_arg_uint16("start", &rc);
    if (rc == ENOENT) {
        start = 0;
    } else if (rc != 0) {
        console_printf("invalid 'start' parameter\n");
        help_cmd_uint16("start");
        return rc;
    }

    end = parse_arg_uint16("end", &rc);
    if (rc == ENOENT) {
        end = 0;
    } else if (rc != 0) {
        console_printf("invalid 'end' parameter\n");
        help_cmd_uint16("end");
        return rc;
    }

    offset = parse_arg_uint16("offset", &rc);
    if (rc == ENOENT) {
        offset = 0;
    } else if (rc != 0) {
        console_printf("invalid 'offset' parameter\n");
        help_cmd_uint16("offset");
        return rc;
    }

    if (num_attr_handles == 1) {
        if (is_long) {
            rc = bletiny_read_long(conn_handle, attr_handles[0], offset);
        } else {
            rc = bletiny_read(conn_handle, attr_handles[0]);
        }
    } else if (num_attr_handles > 1) {
        rc = bletiny_read_mult(conn_handle, attr_handles, num_attr_handles);
    } else if (is_uuid) {
        if (start == 0 || end == 0) {
            rc = EINVAL;
        } else {
            rc = bletiny_read_by_uuid(conn_handle, start, end, &uuid.u);
        }
    } else {
        rc = EINVAL;
    }

    if (rc != 0) {
        console_printf("error reading characteristic; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $rssi                                                                     *
 *****************************************************************************/

static void
bletiny_rssi_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available rssi commands: \n");
    console_printf("\thelp\n");
    console_printf("Available rssi params: \n");
    help_cmd_uint16("conn");
}

static int
cmd_rssi(int argc, char **argv)
{
    uint16_t conn_handle;
    int8_t rssi;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_rssi_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    rc = bletiny_rssi(conn_handle, &rssi);
    if (rc != 0) {
        console_printf("error reading rssi; rc=%d\n", rc);
        return rc;
    }

    console_printf("conn=%d rssi=%d\n", conn_handle, rssi);

    return 0;
}

/*****************************************************************************
 * $scan                                                                     *
 *****************************************************************************/

static struct kv_pair cmd_scan_filt_policies[] = {
    { "no_wl", BLE_HCI_SCAN_FILT_NO_WL },
    { "use_wl", BLE_HCI_SCAN_FILT_USE_WL },
    { "no_wl_inita", BLE_HCI_SCAN_FILT_NO_WL_INITA },
    { "use_wl_inita", BLE_HCI_SCAN_FILT_USE_WL_INITA },
    { NULL }
};

static struct kv_pair cmd_scan_ext_types[] = {
    { "1M",         0x01 },
    { "coded",      0x02 },
    { "both",       0x03 },
    { NULL }
};

static void
bletiny_scan_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available scan commands: \n");
    console_printf("\thelp\n");
    console_printf("\tcancel\n");
    console_printf("Available scan params: \n");
    help_cmd_kv_dflt("ext", cmd_scan_ext_types, 0);
    help_cmd_long_bounds_dflt("dur_ms", 1, INT32_MAX, BLE_HS_FOREVER);
    help_cmd_bool_dflt("ltd", 0);
    help_cmd_bool_dflt("passive", 0);
    help_cmd_uint16_dflt("itvl", 0);
    help_cmd_uint16_dflt("window", 0);
    help_cmd_kv_dflt("filt", cmd_scan_filt_policies,
                     BLE_HCI_SCAN_FILT_NO_WL);
    help_cmd_uint16_dflt("nodups", 0);
    help_cmd_kv_dflt("own_addr_type", cmd_own_addr_types,
                     BLE_OWN_ADDR_PUBLIC);
    console_printf("Available scan params when ext != none: \n");
    help_cmd_uint16_dflt("duration", 0);
    help_cmd_uint16_dflt("period", 0);
    help_cmd_bool_dflt("lr_passive", 0);
    help_cmd_uint16_dflt("lr_itvl", 0);
    help_cmd_uint16_dflt("lr_window", 0);

}

static int
cmd_scan(int argc, char **argv)
{
    struct ble_gap_disc_params params = {0};
    struct ble_gap_ext_disc_params uncoded = {0};
    struct ble_gap_ext_disc_params coded = {0};
    uint8_t extended;
    int32_t duration_ms;
    uint8_t own_addr_type;
    uint16_t duration;
    uint16_t period;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_scan_help();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "cancel") == 0) {
        rc = bletiny_scan_cancel();
        if (rc != 0) {
            console_printf("connection cancel fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    extended = parse_arg_kv_default("ext", cmd_scan_ext_types, 0, &rc);
    if (rc != 0) {
        help_cmd_kv_dflt("ext", cmd_scan_ext_types, 0);
        console_printf("invalid 'ext' parameter\n");
        return rc;
    }

    console_printf("Scan type: %d\n", extended);

    duration_ms = parse_arg_long_bounds_default("dur", 1, INT32_MAX,
                                                BLE_HS_FOREVER, &rc);
    if (rc != 0) {
        console_printf("invalid 'dur' parameter\n");
        help_cmd_long_bounds_dflt("dur", 1, INT32_MAX, BLE_HS_FOREVER);
        return rc;
    }

    params.limited = parse_arg_bool_default("ltd", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'ltd' parameter\n");
        help_cmd_bool_dflt("ltd", 0);
        return rc;
    }

    params.passive = parse_arg_bool_default("passive", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'passive' parameter\n");
        help_cmd_bool_dflt("passive", 0);
        return rc;
    }

    params.itvl = parse_arg_uint16_dflt("itvl", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl' parameter\n");
        help_cmd_uint16_dflt("itvl", 0);
        return rc;
    }

    params.window = parse_arg_uint16_dflt("window", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'window' parameter\n");
        help_cmd_uint16_dflt("window", 0);
        return rc;
    }

    params.filter_policy = parse_arg_kv_default(
        "filt", cmd_scan_filt_policies, BLE_HCI_SCAN_FILT_NO_WL, &rc);
    if (rc != 0) {
        console_printf("invalid 'filt' parameter\n");
        help_cmd_kv_dflt("filt", cmd_scan_filt_policies,
                         BLE_HCI_SCAN_FILT_NO_WL);
        return rc;
    }

    params.filter_duplicates = parse_arg_bool_default("nodups", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'nodups' parameter\n");
        help_cmd_uint16_dflt("nodups", 0);
        return rc;
    }

    own_addr_type = parse_arg_kv_default("own_addr_type", cmd_own_addr_types,
                                         BLE_OWN_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'own_addr_type' parameter\n");
        help_cmd_kv_dflt("own_addr_type", cmd_own_addr_types,
                         BLE_OWN_ADDR_PUBLIC);
        return rc;
    }

    if (extended == 0) {
        goto regular_scan;
    }

    /* Copy above parameters to uncoded params */
    uncoded.passive = params.passive;
    uncoded.itvl = params.itvl;
    uncoded.window = params.window;

    duration = parse_arg_uint16_dflt("duration", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'duration' parameter\n");
        help_cmd_uint16_dflt("duration", 0);
        return rc;
    }

    period = parse_arg_uint16_dflt("period", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'period' parameter\n");
        help_cmd_uint16_dflt("period", 0);
        return rc;
    }

    coded.itvl = parse_arg_uint16_dflt("lr_itvl", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'lr_itvl' parameter\n");
        help_cmd_uint16_dflt("lr_itvl", 0);
        return rc;
    }

    coded.window = parse_arg_uint16_dflt("lr_window", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'lr_window' parameter\n");
        help_cmd_uint16_dflt("lr_window", 0);
        return rc;
    }

    coded.passive = parse_arg_uint16_dflt("lr_passive", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'lr_passive' parameter\n");
        help_cmd_uint16_dflt("lr_window", 0);
        return rc;
    }

    switch (extended) {
    case 0x01:
        rc = bletiny_ext_scan(own_addr_type, duration, period,
                         params.filter_duplicates, params.filter_policy,
                         params.limited, &uncoded, NULL);
        break;
    case 0x02:
        rc = bletiny_ext_scan(own_addr_type, duration, period,
                              params.filter_duplicates, params.filter_policy,
                              params.limited, NULL, &coded);
        break;
    case 0x03:
        rc = bletiny_ext_scan(own_addr_type, duration, period,
                              params.filter_duplicates, params.filter_policy,
                              params.limited, &uncoded, &coded);
        break;
    default:
        rc = -1;
        console_printf("Something went wrong :)\n");
        break;
    }

    return rc;

regular_scan:
    rc = bletiny_scan(own_addr_type, duration_ms, &params);
    if (rc != 0) {
        console_printf("error scanning; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $show                                                                     *
 *****************************************************************************/

static int
cmd_show_addr(int argc, char **argv)
{
    uint8_t id_addr[6];
    int rc;

    console_printf("public_id_addr=");
    rc = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, id_addr, NULL);
    if (rc == 0) {
        print_addr(id_addr);
    } else {
        console_printf("none");
    }

    console_printf(" random_id_addr=");
    rc = ble_hs_id_copy_addr(BLE_ADDR_RANDOM, id_addr, NULL);
    if (rc == 0) {
        print_addr(id_addr);
    } else {
        console_printf("none");
    }
    console_printf("\n");

    return 0;
}

static int
cmd_show_chr(int argc, char **argv)
{
    struct bletiny_conn *conn;
    struct bletiny_svc *svc;
    int i;

    for (i = 0; i < bletiny_num_conns; i++) {
        conn = bletiny_conns + i;

        console_printf("CONNECTION: handle=%d\n", conn->handle);

        SLIST_FOREACH(svc, &conn->svcs, next) {
            cmd_print_svc(svc);
        }
    }

    return 0;
}

static int
cmd_show_conn(int argc, char **argv)
{
    struct ble_gap_conn_desc conn_desc;
    struct bletiny_conn *conn;
    int rc;
    int i;

    for (i = 0; i < bletiny_num_conns; i++) {
        conn = bletiny_conns + i;

        rc = ble_gap_conn_find(conn->handle, &conn_desc);
        if (rc == 0) {
            print_conn_desc(&conn_desc);
        }
    }

    return 0;
}

static int
cmd_show_coc(int argc, char **argv)
{
    struct bletiny_conn *conn = NULL;
    struct bletiny_l2cap_coc *coc;
    int i, j;

    for (i = 0; i < bletiny_num_conns; i++) {
        conn = bletiny_conns + i;

        if (SLIST_EMPTY(&conn->coc_list)) {
            continue;
        }

        console_printf("conn_handle: 0x%04x\n", conn->handle);
        j = 0;
        SLIST_FOREACH(coc, &conn->coc_list, next) {
            console_printf("    idx: %i, chan pointer = %p\n", j++, coc->chan);
        }
    }

    return 0;
}

static struct cmd_entry cmd_show_entries[];

static int
cmd_show_help(int argc, char **argv)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return 0;
#endif

    int i;

    console_printf("Available show commands:\n");
    for (i = 0; cmd_show_entries[i].name != NULL; i++) {
        console_printf("\t%s\n", cmd_show_entries[i].name);
    }
    return 0;
}

static struct cmd_entry cmd_show_entries[] = {
    { "addr", cmd_show_addr },
    { "chr", cmd_show_chr },
    { "conn", cmd_show_conn },
    { "coc", cmd_show_coc },
    { "help", cmd_show_help },
    { NULL, NULL }
};

static int
cmd_show(int argc, char **argv)
{
    int rc;

    rc = cmd_exec(cmd_show_entries, argc, argv);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $sec                                                                      *
 *****************************************************************************/

static void
bletiny_sec_pair_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available sec pair commands: \n");
    console_printf("\thelp\n");
    console_printf("Available sec pair params: \n");
    help_cmd_uint16("conn");
}

static int
cmd_sec_pair(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_sec_pair_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    rc = bletiny_sec_pair(conn_handle);
    if (rc != 0) {
        console_printf("error initiating pairing; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static void
bletiny_sec_start_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available sec start commands: \n");
    console_printf("\thelp\n");
    console_printf("Available sec start params: \n");
    help_cmd_uint16("conn");
}

static int
cmd_sec_start(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_sec_start_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    rc = bletiny_sec_start(conn_handle);
    if (rc != 0) {
        console_printf("error starting security; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static void
bletiny_sec_enc_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available sec enc commands: \n");
    console_printf("\thelp\n");
    console_printf("Available sec enc params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint64("rand");
    help_cmd_bool("auth");
    help_cmd_byte_stream_exact_length("ltk", 16);
}

static int
cmd_sec_enc(int argc, char **argv)
{
    uint16_t conn_handle;
    uint16_t ediv;
    uint64_t rand_val;
    uint8_t ltk[16];
    int rc;
    int auth;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_sec_enc_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    ediv = parse_arg_uint16("ediv", &rc);
    if (rc == ENOENT) {
        rc = bletiny_sec_restart(conn_handle, NULL, 0, 0, 0);
    } else {
        rand_val = parse_arg_uint64("rand", &rc);
        if (rc != 0) {
            console_printf("invalid 'rand' parameter\n");
            help_cmd_uint64("rand");
            return rc;
        }

        auth = parse_arg_bool("auth", &rc);
        if (rc != 0) {
            console_printf("invalid 'auth' parameter\n");
            help_cmd_bool("auth");
            return rc;
        }

        rc = parse_arg_byte_stream_exact_length("ltk", ltk, 16);
        if (rc != 0) {
            console_printf("invalid 'ltk' parameter\n");
            help_cmd_byte_stream_exact_length("ltk", 16);
            return rc;
        }

        rc = bletiny_sec_restart(conn_handle, ltk, ediv, rand_val, auth);
    }

    if (rc != 0) {
        console_printf("error initiating encryption; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static struct cmd_entry cmd_sec_entries[];

static int
cmd_sec_help(int argc, char **argv)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return 0;
#endif

    int i;

    console_printf("Available sec commands:\n");
    for (i = 0; cmd_sec_entries[i].name != NULL; i++) {
        console_printf("\t%s\n", cmd_sec_entries[i].name);
    }
    return 0;
}

static struct cmd_entry cmd_sec_entries[] = {
    { "pair", cmd_sec_pair },
    { "start", cmd_sec_start },
    { "enc", cmd_sec_enc },
    { "help", cmd_sec_help },
    { NULL }
};

static int
cmd_sec(int argc, char **argv)
{
    int rc;

    rc = cmd_exec(cmd_sec_entries, argc, argv);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $set                                                                      *
 *****************************************************************************/

#define CMD_ADV_DATA_MAX_UUIDS16                8
#define CMD_ADV_DATA_MAX_UUIDS32                8
#define CMD_ADV_DATA_MAX_UUIDS128               2
#define CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS       8
#define CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN    BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN    BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN   BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_URI_MAX_LEN                BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_MFG_DATA_MAX_LEN           BLE_HS_ADV_MAX_FIELD_SZ

static void
bletiny_set_adv_data_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available set adv_data params: \n");
    help_cmd_long_bounds("flags", 0, UINT8_MAX);
    help_cmd_uint16("uuid16");
    help_cmd_long("uuids16_is_complete");
    help_cmd_uint32("uuid32");
    help_cmd_long("uuids32_is_complete");
    help_cmd_byte_stream_exact_length("uuid128", 16);
    help_cmd_long("uuids128_is_complete");
    help_cmd_long_bounds("tx_pwr_lvl", INT8_MIN, INT8_MAX);
    help_cmd_byte_stream_exact_length("slave_itvl_range",
                                      BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
    help_cmd_byte_stream("svc_data_uuid16");
    help_cmd_byte_stream_exact_length("public_tgt_addr",
                                      BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
    help_cmd_uint16("appearance");
    help_cmd_extract("name");
    help_cmd_uint16("adv_itvl");
    help_cmd_byte_stream("svc_data_uuid32");
    help_cmd_byte_stream("svc_data_uuid128");
    help_cmd_byte_stream("uri");
    help_cmd_byte_stream("mfg_data");
}

static int
cmd_set_adv_data(void)
{
    static bssnz_t ble_uuid16_t uuids16[CMD_ADV_DATA_MAX_UUIDS16];
    static bssnz_t ble_uuid32_t uuids32[CMD_ADV_DATA_MAX_UUIDS32];
    static bssnz_t ble_uuid128_t uuids128[CMD_ADV_DATA_MAX_UUIDS128];
    static bssnz_t uint8_t
        public_tgt_addrs[CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS]
                        [BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN];
    static bssnz_t uint8_t slave_itvl_range[BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN];
    static bssnz_t uint8_t
        svc_data_uuid16[CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN];
    static bssnz_t uint8_t
        svc_data_uuid32[CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN];
    static bssnz_t uint8_t
        svc_data_uuid128[CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN];
    static bssnz_t uint8_t uri[CMD_ADV_DATA_URI_MAX_LEN];
    static bssnz_t uint8_t mfg_data[CMD_ADV_DATA_MFG_DATA_MAX_LEN];
    struct ble_hs_adv_fields adv_fields;
    uint32_t uuid32;
    uint16_t uuid16;
    uint8_t uuid128[16];
    uint8_t public_tgt_addr[BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN];
    uint8_t eddystone_url_body_len;
    uint8_t eddystone_url_suffix;
    uint8_t eddystone_url_scheme;
    char eddystone_url_body[BLE_EDDYSTONE_URL_MAX_LEN];
    char *eddystone_url_full;
    int svc_data_uuid16_len;
    int svc_data_uuid32_len;
    int svc_data_uuid128_len;
    int uri_len;
    int mfg_data_len;
    int tmp;
    int rc;

    memset(&adv_fields, 0, sizeof adv_fields);

    tmp = parse_arg_long_bounds("flags", 0, UINT8_MAX, &rc);
    if (rc == 0) {
        adv_fields.flags = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'flags' parameter\n");
        help_cmd_long_bounds("flags", 0, UINT8_MAX);
        return rc;
    }

    while (1) {
        uuid16 = parse_arg_uint16("uuid16", &rc);
        if (rc == 0) {
            if (adv_fields.num_uuids16 >= CMD_ADV_DATA_MAX_UUIDS16) {
                console_printf("invalid 'uuid16' parameter\n");
                help_cmd_uint16("uuid16");
                return EINVAL;
            }
            uuids16[adv_fields.num_uuids16] = (ble_uuid16_t) BLE_UUID16_INIT(uuid16);
            adv_fields.num_uuids16++;
        } else if (rc == ENOENT) {
            break;
        } else {
            console_printf("invalid 'uuid16' parameter\n");
            help_cmd_uint16("uuid16");
            return rc;
        }
    }
    if (adv_fields.num_uuids16 > 0) {
        adv_fields.uuids16 = uuids16;
    }

    tmp = parse_arg_long("uuids16_is_complete", &rc);
    if (rc == 0) {
        adv_fields.uuids16_is_complete = !!tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'uuids16_is_complete' parameter\n");
        help_cmd_long("uuids16_is_complete");
        return rc;
    }

    while (1) {
        uuid32 = parse_arg_uint32("uuid32", &rc);
        if (rc == 0) {
            if (adv_fields.num_uuids32 >= CMD_ADV_DATA_MAX_UUIDS32) {
                console_printf("invalid 'uuid32' parameter\n");
                help_cmd_uint32("uuid32");
                return EINVAL;
            }
            uuids32[adv_fields.num_uuids32] = (ble_uuid32_t) BLE_UUID32_INIT(uuid32);
            adv_fields.num_uuids32++;
        } else if (rc == ENOENT) {
            break;
        } else {
            console_printf("invalid 'uuid32' parameter\n");
            help_cmd_uint32("uuid32");
            return rc;
        }
    }
    if (adv_fields.num_uuids32 > 0) {
        adv_fields.uuids32 = uuids32;
    }

    tmp = parse_arg_long("uuids32_is_complete", &rc);
    if (rc == 0) {
        adv_fields.uuids32_is_complete = !!tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'uuids32_is_complete' parameter\n");
        help_cmd_long("uuids32_is_complete");
        return rc;
    }

    while (1) {
        rc = parse_arg_byte_stream_exact_length("uuid128", uuid128, 16);
        if (rc == 0) {
            if (adv_fields.num_uuids128 >= CMD_ADV_DATA_MAX_UUIDS128) {
                console_printf("invalid 'uuid128' parameter\n");
                help_cmd_byte_stream_exact_length("uuid128", 16);
                return EINVAL;
            }
            ble_uuid_init_from_buf((ble_uuid_any_t *) &uuids128[adv_fields.num_uuids128],
                                   uuid128, 16);
            adv_fields.num_uuids128++;
        } else if (rc == ENOENT) {
            break;
        } else {
            console_printf("invalid 'uuid128' parameter\n");
            help_cmd_byte_stream_exact_length("uuid128", 16);
            return rc;
        }
    }
    if (adv_fields.num_uuids128 > 0) {
        adv_fields.uuids128 = uuids128;
    }

    tmp = parse_arg_long("uuids128_is_complete", &rc);
    if (rc == 0) {
        adv_fields.uuids128_is_complete = !!tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'uuids128_is_complete' parameter\n");
        help_cmd_long("uuids128_is_complete");
        return rc;
    }

    adv_fields.name = (uint8_t *)parse_arg_extract("name");
    if (adv_fields.name != NULL) {
        adv_fields.name_len = strlen((char *)adv_fields.name);
    }

    tmp = parse_arg_long_bounds("tx_pwr_lvl", INT8_MIN, INT8_MAX, &rc);
    if (rc == 0) {
        adv_fields.tx_pwr_lvl = tmp;
        adv_fields.tx_pwr_lvl_is_present = 1;
    } else if (rc != ENOENT) {
        console_printf("invalid 'tx_pwr_lvl' parameter\n");
        help_cmd_long_bounds("tx_pwr_lvl", INT8_MIN, INT8_MAX);
        return rc;
    }

    rc = parse_arg_byte_stream_exact_length("slave_itvl_range",
                                            slave_itvl_range,
                                            BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
    if (rc == 0) {
        adv_fields.slave_itvl_range = slave_itvl_range;
    } else if (rc != ENOENT) {
        console_printf("invalid 'slave_itvl_range' parameter\n");
        help_cmd_byte_stream_exact_length("slave_itvl_range",
                                          BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
        return rc;
    }

    rc = parse_arg_byte_stream("svc_data_uuid16",
                               CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN,
                               svc_data_uuid16, &svc_data_uuid16_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid16 = svc_data_uuid16;
        adv_fields.svc_data_uuid16_len = svc_data_uuid16_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'svc_data_uuid16' parameter\n");
        help_cmd_byte_stream("svc_data_uuid16");
        return rc;
    }

    while (1) {
        rc = parse_arg_byte_stream_exact_length(
            "public_tgt_addr", public_tgt_addr,
            BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
        if (rc == 0) {
            if (adv_fields.num_public_tgt_addrs >=
                CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS) {

                console_printf("invalid 'public_tgt_addr' parameter\n");
                help_cmd_byte_stream_exact_length("public_tgt_addr",
                                          BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
                return EINVAL;
            }
            memcpy(public_tgt_addrs[adv_fields.num_public_tgt_addrs],
                   public_tgt_addr, BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
            adv_fields.num_public_tgt_addrs++;
        } else if (rc == ENOENT) {
            break;
        } else {
            console_printf("invalid 'public_tgt_addr' parameter\n");
            help_cmd_byte_stream_exact_length("public_tgt_addr",
                                          BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
            return rc;
        }
    }
    if (adv_fields.num_public_tgt_addrs > 0) {
        adv_fields.public_tgt_addr = (void *)public_tgt_addrs;
    }

    adv_fields.appearance = parse_arg_uint16("appearance", &rc);
    if (rc == 0) {
        adv_fields.appearance_is_present = 1;
    } else if (rc != ENOENT) {
        console_printf("invalid 'appearance' parameter\n");
        help_cmd_uint16("appearance");
        return rc;
    }

    adv_fields.adv_itvl = parse_arg_uint16("adv_itvl", &rc);
    if (rc == 0) {
        adv_fields.adv_itvl_is_present = 1;
    } else if (rc != ENOENT) {
        console_printf("invalid 'adv_itvl' parameter\n");
        help_cmd_uint16("adv_itvl");
        return rc;
    }

    rc = parse_arg_byte_stream("svc_data_uuid32",
                               CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN,
                               svc_data_uuid32, &svc_data_uuid32_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid32 = svc_data_uuid32;
        adv_fields.svc_data_uuid32_len = svc_data_uuid32_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'svc_data_uuid32' parameter\n");
        help_cmd_byte_stream("svc_data_uuid32");
        return rc;
    }

    rc = parse_arg_byte_stream("svc_data_uuid128",
                               CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN,
                               svc_data_uuid128, &svc_data_uuid128_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid128 = svc_data_uuid128;
        adv_fields.svc_data_uuid128_len = svc_data_uuid128_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'svc_data_uuid128' parameter\n");
        help_cmd_byte_stream("svc_data_uuid128");
        return rc;
    }

    rc = parse_arg_byte_stream("uri", CMD_ADV_DATA_URI_MAX_LEN, uri, &uri_len);
    if (rc == 0) {
        adv_fields.uri = uri;
        adv_fields.uri_len = uri_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'uri' parameter\n");
        help_cmd_byte_stream("uri");
        return rc;
    }

    rc = parse_arg_byte_stream("mfg_data", CMD_ADV_DATA_MFG_DATA_MAX_LEN,
                               mfg_data, &mfg_data_len);
    if (rc == 0) {
        adv_fields.mfg_data = mfg_data;
        adv_fields.mfg_data_len = mfg_data_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'mfg_data' parameter\n");
        help_cmd_byte_stream("mfg_data");
        return rc;
    }

    eddystone_url_full = parse_arg_extract("eddystone_url");
    if (eddystone_url_full != NULL) {
        rc = cmd_parse_eddystone_url(eddystone_url_full, &eddystone_url_scheme,
                                     eddystone_url_body,
                                     &eddystone_url_body_len,
                                     &eddystone_url_suffix);
        if (rc != 0) {
            return rc;
        }

        rc = ble_eddystone_set_adv_data_url(&adv_fields, eddystone_url_scheme,
                                            eddystone_url_body,
                                            eddystone_url_body_len,
                                            eddystone_url_suffix);
    } else {
        rc = bletiny_set_adv_data(&adv_fields);
    }
    if (rc != 0) {
        console_printf("error setting advertisement data; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static void
bletiny_set_sm_data_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available set sm_data params: \n");
    help_cmd_bool("oob_flag");
    help_cmd_bool("mitm_flag");
    help_cmd_uint8("io_capabilities");
    help_cmd_uint8("our_key_dist");
    help_cmd_uint8("their_key_dist");
    help_cmd_bool("bonding");
    help_cmd_bool("sc");
}

static int
cmd_set_sm_data(void)
{
    uint8_t tmp;
    int good;
    int rc;

    good = 0;

    tmp = parse_arg_bool("oob_flag", &rc);
    if (rc == 0) {
        ble_hs_cfg.sm_oob_data_flag = tmp;
        good++;
    } else if (rc != ENOENT) {
        console_printf("invalid 'oob_flag' parameter\n");
        help_cmd_bool("oob_flag");
        return rc;
    }

    tmp = parse_arg_bool("mitm_flag", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_mitm = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'mitm_flag' parameter\n");
        help_cmd_bool("mitm_flag");
        return rc;
    }

    tmp = parse_arg_uint8("io_capabilities", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_io_cap = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'io_capabilities' parameter\n");
        help_cmd_uint8("io_capabilities");
        return rc;
    }

    tmp = parse_arg_uint8("our_key_dist", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_our_key_dist = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'our_key_dist' parameter\n");
        help_cmd_uint8("our_key_dist");
        return rc;
    }

    tmp = parse_arg_uint8("their_key_dist", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_their_key_dist = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'their_key_dist' parameter\n");
        help_cmd_uint8("their_key_dist");
        return rc;
    }

    tmp = parse_arg_bool("bonding", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_bonding = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'bonding' parameter\n");
        help_cmd_bool("bonding");
        return rc;
    }

    tmp = parse_arg_bool("sc", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_sc = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'sc' parameter\n");
        help_cmd_bool("sc");
        return rc;
    }

    if (!good) {
        console_printf("Error: no valid settings specified\n");
        return -1;
    }

    return 0;
}

static struct kv_pair cmd_set_addr_types[] = {
    { "public",         BLE_ADDR_PUBLIC },
    { "random",         BLE_ADDR_RANDOM },
    { NULL }
};

static void
bletiny_set_priv_mode_help(void)
{
    console_printf("Available set priv_mode params: \n");
    help_cmd_kv_dflt("addr_type", cmd_set_addr_types, BLE_ADDR_PUBLIC);
    help_cmd_byte_stream_exact_length("addr", 6);
    help_cmd_uint8("mode");
}

static int
cmd_set_priv_mode(void)
{
    ble_addr_t addr;
    uint8_t priv_mode;
    int rc;

    addr.type = parse_arg_kv_default("addr_type", cmd_set_addr_types,
                                     BLE_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'addr_type' parameter\n");
        help_cmd_kv_dflt("addr_type", cmd_set_addr_types, BLE_ADDR_PUBLIC);
        return rc;
    }

    rc = parse_arg_mac("addr", addr.val);
    if (rc != 0) {
        console_printf("invalid 'addr' parameter\n");
        help_cmd_byte_stream_exact_length("addr", 6);
        return rc;
    }

    priv_mode = parse_arg_uint8("mode", &rc);
    if (rc != 0) {
        console_printf("missing mode\n");
        return rc;
    }

    return ble_gap_set_priv_mode(&addr, priv_mode);
}

static void
bletiny_set_addr_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available set addr params: \n");
    help_cmd_kv_dflt("addr_type", cmd_set_addr_types, BLE_ADDR_PUBLIC);
    help_cmd_byte_stream_exact_length("addr", 6);
}

static int
cmd_set_addr(void)
{
    uint8_t addr[6];
    int addr_type;
    int rc;

    addr_type = parse_arg_kv_default("addr_type", cmd_set_addr_types,
                                     BLE_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'addr_type' parameter\n");
        help_cmd_kv_dflt("addr_type", cmd_set_addr_types, BLE_ADDR_PUBLIC);
        return rc;
    }

    rc = parse_arg_mac("addr", addr);
    if (rc != 0) {
        console_printf("invalid 'addr' parameter\n");
        help_cmd_byte_stream_exact_length("addr", 6);
        return rc;
    }

    switch (addr_type) {
    case BLE_ADDR_PUBLIC:
        /* We shouldn't be writing to the controller's address (g_dev_addr).
         * There is no standard way to set the local public address, so this is
         * our only option at the moment.
         */
        memcpy(g_dev_addr, addr, 6);
        ble_hs_id_set_pub(g_dev_addr);
        break;

    case BLE_ADDR_RANDOM:
        rc = ble_hs_id_set_rnd(addr);
        if (rc != 0) {
            return rc;
        }
        break;

    default:
        assert(0);
        return BLE_HS_EUNKNOWN;
    }

    return 0;
}

static void
bletiny_set_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available set commands: \n");
    console_printf("\thelp\n");
    console_printf("\tadv_data\n");
    console_printf("\tsm_data\n");
    console_printf("\taddr\n");
    console_printf("Available set params: \n");
    help_cmd_uint16("mtu");
    help_cmd_byte_stream_exact_length("irk", 16);
}

static int
cmd_set(int argc, char **argv)
{
    uint16_t mtu;
    uint8_t irk[16];
    int good;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_set_help();
        bletiny_set_adv_data_help();
        bletiny_set_sm_data_help();
        bletiny_set_addr_help();
        bletiny_set_priv_mode_help();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "adv_data") == 0) {
        rc = cmd_set_adv_data();
        return rc;
    }

    if (argc > 1 && strcmp(argv[1], "sm_data") == 0) {
        rc = cmd_set_sm_data();
        return rc;
    }

    if (argc > 1 && strcmp(argv[1], "priv_mode") == 0) {
        rc = cmd_set_priv_mode();
        return rc;
    }

    good = 0;

    rc = parse_arg_find_idx("addr");
    if (rc != -1) {
        rc = cmd_set_addr();
        if (rc != 0) {
            return rc;
        }

        good = 1;
    }

    mtu = parse_arg_uint16("mtu", &rc);
    if (rc == 0) {
        rc = ble_att_set_preferred_mtu(mtu);
        if (rc == 0) {
            good = 1;
        }
    } else if (rc != ENOENT) {
        console_printf("invalid 'mtu' parameter\n");
        help_cmd_uint16("mtu");
        return rc;
    }

    rc = parse_arg_byte_stream_exact_length("irk", irk, 16);
    if (rc == 0) {
        good = 1;
        ble_hs_pvcy_set_our_irk(irk);
    } else if (rc != ENOENT) {
        console_printf("invalid 'irk' parameter\n");
        help_cmd_byte_stream_exact_length("irk", 16);
        return rc;
    }

    if (!good) {
        console_printf("Error: no valid settings specified\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * $terminate                                                                *
 *****************************************************************************/

static void
bletiny_term_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available term commands: \n");
    console_printf("\thelp\n");
    console_printf("Available term params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint8_dflt("reason", BLE_ERR_REM_USER_CONN_TERM);
}

static int
cmd_term(int argc, char **argv)
{
    uint16_t conn_handle;
    uint8_t reason;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_term_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    reason = parse_arg_uint8_dflt("reason", BLE_ERR_REM_USER_CONN_TERM, &rc);
    if (rc != 0) {
        console_printf("invalid 'reason' parameter\n");
        help_cmd_uint8_dflt("reason", BLE_ERR_REM_USER_CONN_TERM);
        return rc;
    }

    rc = bletiny_term_conn(conn_handle, reason);
    if (rc != 0) {
        console_printf("error terminating connection; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $update connection parameters                                             *
 *****************************************************************************/

static void
bletiny_update_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available update commands: \n");
    console_printf("\thelp\n");
    console_printf("Available update params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint16_dflt("itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
    help_cmd_uint16_dflt("itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
    help_cmd_uint16_dflt("latency", 0);
    help_cmd_uint16_dflt("timeout", 0x0100);
    help_cmd_uint16_dflt("min_ce_len", 0x0010);
    help_cmd_uint16_dflt("max_ce_len", 0x0300);
}

static int
cmd_update(int argc, char **argv)
{
    struct ble_gap_upd_params params;
    uint16_t conn_handle;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_update_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    params.itvl_min = parse_arg_uint16_dflt(
        "itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_min' parameter\n");
        help_cmd_uint16_dflt("itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN);
        return rc;
    }

    params.itvl_max = parse_arg_uint16_dflt(
        "itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_max' parameter\n");
        help_cmd_uint16_dflt("itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX);
        return rc;
    }

    params.latency = parse_arg_uint16_dflt("latency", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'latency' parameter\n");
        help_cmd_uint16_dflt("latency", 0);
        return rc;
    }

    params.supervision_timeout = parse_arg_uint16_dflt("timeout", 0x0100, &rc);
    if (rc != 0) {
        console_printf("invalid 'timeout' parameter\n");
        help_cmd_uint16_dflt("timeout", 0x0100);
        return rc;
    }

    params.min_ce_len = parse_arg_uint16_dflt("min_ce_len", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'min_ce_len' parameter\n");
        help_cmd_uint16_dflt("min_ce_len", 0x0010);
        return rc;
    }

    params.max_ce_len = parse_arg_uint16_dflt("max_ce_len", 0x0300, &rc);
    if (rc != 0) {
        console_printf("invalid 'max_ce_len' parameter\n");
        help_cmd_uint16_dflt("max_ce_len", 0x0300);
        return rc;
    }

    rc = bletiny_update_conn(conn_handle, &params);
    if (rc != 0) {
        console_printf("error updating connection; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $white list                                                               *
 *****************************************************************************/

#define CMD_WL_MAX_SZ   8

static void
bletiny_wl_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available wl commands: \n");
    console_printf("\thelp\n");
    console_printf("Available wl params: \n");
    console_printf("\tlist of:\n");
    help_cmd_byte_stream_exact_length("addr", 6);
    help_cmd_kv("addr_type", cmd_addr_type);
}

static int
cmd_wl(int argc, char **argv)
{
    static ble_addr_t addrs[CMD_WL_MAX_SZ];
    int addrs_cnt;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_wl_help();
        return 0;
    }

    addrs_cnt = 0;
    while (1) {
        if (addrs_cnt >= CMD_WL_MAX_SZ) {
            return EINVAL;
        }

        rc = parse_arg_mac("addr", addrs[addrs_cnt].val);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            console_printf("invalid 'addr' parameter\n");
            help_cmd_byte_stream_exact_length("addr", 6);
            return rc;
        }

        addrs[addrs_cnt].type = parse_arg_kv("addr_type", cmd_addr_type, &rc);
        if (rc != 0) {
            console_printf("invalid 'addr' parameter\n");
            help_cmd_kv("addr_type", cmd_addr_type);
            return rc;
        }

        addrs_cnt++;
    }

    if (addrs_cnt == 0) {
        return EINVAL;
    }

    bletiny_wl_set(addrs, addrs_cnt);

    return 0;
}

/*****************************************************************************
 * $write                                                                    *
 *****************************************************************************/

static void
bletiny_write_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available write commands: \n");
    console_printf("\thelp\n");
    console_printf("Available write params: \n");
    help_cmd_uint16("conn");
    help_cmd_long("no_rsp");
    help_cmd_long("long");
    console_printf("\tlist of:\n");
    help_cmd_long("attr");
    help_cmd_byte_stream("value");
    help_cmd_uint16("offset");
}

static int
cmd_write(int argc, char **argv)
{
    struct ble_gatt_attr attrs[MYNEWT_VAL(BLE_GATT_WRITE_MAX_ATTRS)] = { { 0 } };
    uint16_t attr_handle;
    uint16_t conn_handle;
    uint16_t offset;
    int total_attr_len;
    int num_attrs;
    int attr_len;
    int is_long;
    int no_rsp;
    int rc;
    int i;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_write_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    no_rsp = parse_arg_long("no_rsp", &rc);
    if (rc == ENOENT) {
        no_rsp = 0;
    } else if (rc != 0) {
        console_printf("invalid 'no_rsp' parameter\n");
        help_cmd_long("no_rsp");
        return rc;
    }

    is_long = parse_arg_long("long", &rc);
    if (rc == ENOENT) {
        is_long = 0;
    } else if (rc != 0) {
        console_printf("invalid 'long' parameter\n");
        help_cmd_long("long");
        return rc;
    }

    total_attr_len = 0;
    num_attrs = 0;
    while (1) {
        attr_handle = parse_arg_long("attr", &rc);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            rc = -rc;
            console_printf("invalid 'attr' parameter\n");
            help_cmd_long("attr");
            goto done;
        }

        rc = parse_arg_byte_stream("value", sizeof cmd_buf - total_attr_len,
                                   cmd_buf + total_attr_len, &attr_len);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            console_printf("invalid 'value' parameter\n");
            help_cmd_byte_stream("value");
            goto done;
        }

        offset = parse_arg_uint16("offset", &rc);
        if (rc == ENOENT) {
            offset = 0;
        } else if (rc != 0) {
            console_printf("invalid 'offset' parameter\n");
            help_cmd_uint16("offset");
            return rc;
        }

        if (num_attrs >= sizeof attrs / sizeof attrs[0]) {
            rc = -EINVAL;
            goto done;
        }

        attrs[num_attrs].handle = attr_handle;
        attrs[num_attrs].offset = offset;
        attrs[num_attrs].om = ble_hs_mbuf_from_flat(cmd_buf + total_attr_len,
                                                    attr_len);
        if (attrs[num_attrs].om == NULL) {
            goto done;
        }

        total_attr_len += attr_len;
        num_attrs++;
    }

    if (no_rsp) {
        if (num_attrs != 1) {
            rc = -EINVAL;
            goto done;
        }
        rc = bletiny_write_no_rsp(conn_handle, attrs[0].handle, attrs[0].om);
        attrs[0].om = NULL;
    } else if (is_long) {
        if (num_attrs != 1) {
            rc = -EINVAL;
            goto done;
        }
        rc = bletiny_write_long(conn_handle, attrs[0].handle,
                                attrs[0].offset, attrs[0].om);
        attrs[0].om = NULL;
    } else if (num_attrs > 1) {
        rc = bletiny_write_reliable(conn_handle, attrs, num_attrs);
    } else if (num_attrs == 1) {
        rc = bletiny_write(conn_handle, attrs[0].handle, attrs[0].om);
        attrs[0].om = NULL;
    } else {
        rc = -EINVAL;
    }

done:
    for (i = 0; i < sizeof attrs / sizeof attrs[0]; i++) {
        os_mbuf_free_chain(attrs[i].om);
    }

    if (rc != 0) {
        console_printf("error writing characteristic; rc=%d\n", rc);
    }

    return rc;
}

/*****************************************************************************
 * store                                                                     *
 *****************************************************************************/

static struct kv_pair cmd_keystore_entry_type[] = {
    { "msec",       BLE_STORE_OBJ_TYPE_PEER_SEC },
    { "ssec",       BLE_STORE_OBJ_TYPE_OUR_SEC },
    { "cccd",       BLE_STORE_OBJ_TYPE_CCCD },
    { NULL }
};

static void
bletiny_keystore_parse_keydata_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available keystore parse keydata params: \n");
    help_cmd_kv("type", cmd_keystore_entry_type);
    help_cmd_kv("addr_type", cmd_addr_type);
    help_cmd_byte_stream_exact_length("addr", 6);
    help_cmd_uint16("ediv");
    help_cmd_uint64("rand");
}

static int
cmd_keystore_parse_keydata(int argc, char **argv, union ble_store_key *out,
                           int *obj_type)
{
    int rc;

    memset(out, 0, sizeof(*out));
    *obj_type = parse_arg_kv("type", cmd_keystore_entry_type, &rc);
    if (rc != 0) {
        console_printf("invalid 'type' parameter\n");
        help_cmd_kv("type", cmd_keystore_entry_type);
        return rc;
    }

    switch (*obj_type) {
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
        out->sec.peer_addr.type = parse_arg_kv("addr_type", cmd_addr_type, &rc);
        if (rc != 0) {
            console_printf("invalid 'addr_type' parameter\n");
            help_cmd_kv("addr_type", cmd_addr_type);
            return rc;
        }

        rc = parse_arg_mac("addr", out->sec.peer_addr.val);
        if (rc != 0) {
            console_printf("invalid 'addr' parameter\n");
            help_cmd_byte_stream_exact_length("addr", 6);
            return rc;
        }

        out->sec.ediv = parse_arg_uint16("ediv", &rc);
        if (rc != 0) {
            console_printf("invalid 'ediv' parameter\n");
            help_cmd_uint16("ediv");
            return rc;
        }

        out->sec.rand_num = parse_arg_uint64("rand", &rc);
        if (rc != 0) {
            console_printf("invalid 'rand' parameter\n");
            help_cmd_uint64("rand");
            return rc;
        }
        return 0;

    default:
        return EINVAL;
    }
}

static void
bletiny_keystore_parse_valuedata_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available keystore parse valuedata params: \n");
    help_cmd_byte_stream_exact_length("ltk", 16);
    help_cmd_byte_stream_exact_length("irk", 16);
    help_cmd_byte_stream_exact_length("csrk", 16);
}

static int
cmd_keystore_parse_valuedata(int argc, char **argv,
                             int obj_type,
                             union ble_store_key *key,
                             union ble_store_value *out)
{
    int rc;
    int valcnt = 0;
    memset(out, 0, sizeof(*out));

    switch (obj_type) {
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
            rc = parse_arg_byte_stream_exact_length("ltk", out->sec.ltk, 16);
            if (rc == 0) {
                out->sec.ltk_present = 1;
                swap_in_place(out->sec.ltk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                console_printf("invalid 'ltk' parameter\n");
                help_cmd_byte_stream_exact_length("ltk", 16);
                return rc;
            }
            rc = parse_arg_byte_stream_exact_length("irk", out->sec.irk, 16);
            if (rc == 0) {
                out->sec.irk_present = 1;
                swap_in_place(out->sec.irk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                console_printf("invalid 'irk' parameter\n");
                help_cmd_byte_stream_exact_length("irk", 16);
                return rc;
            }
            rc = parse_arg_byte_stream_exact_length("csrk", out->sec.csrk, 16);
            if (rc == 0) {
                out->sec.csrk_present = 1;
                swap_in_place(out->sec.csrk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                console_printf("invalid 'csrk' parameter\n");
                help_cmd_byte_stream_exact_length("csrk", 16);
                return rc;
            }
            out->sec.peer_addr = key->sec.peer_addr;
            out->sec.ediv = key->sec.ediv;
            out->sec.rand_num = key->sec.rand_num;
            break;
    }

    if (valcnt) {
        return 0;
    }
    return -1;
}

static void
bletiny_keystore_add_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available keystore add commands: \n");
    console_printf("\thelp\n");
    bletiny_keystore_parse_keydata_help();
    bletiny_keystore_parse_valuedata_help();
}

static int
cmd_keystore_add(int argc, char **argv)
{
    union ble_store_key key;
    union ble_store_value value;
    int obj_type;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_keystore_add_help();
        return 0;
    }

    rc = cmd_keystore_parse_keydata(argc, argv, &key, &obj_type);

    if (rc) {
        return rc;
    }

    rc = cmd_keystore_parse_valuedata(argc, argv, obj_type, &key, &value);

    if (rc) {
        return rc;
    }

    switch(obj_type) {
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
            rc = ble_store_write_peer_sec(&value.sec);
            break;
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
            rc = ble_store_write_our_sec(&value.sec);
            break;
        case BLE_STORE_OBJ_TYPE_CCCD:
            rc = ble_store_write_cccd(&value.cccd);
            break;
        default:
            rc = ble_store_write(obj_type, &value);
    }
    return rc;
}

static void
bletiny_keystore_del_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available keystore del commands: \n");
    console_printf("\thelp\n");
    bletiny_keystore_parse_keydata_help();
}

static int
cmd_keystore_del(int argc, char **argv)
{
    union ble_store_key key;
    int obj_type;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_keystore_del_help();
        return 0;
    }

    rc = cmd_keystore_parse_keydata(argc, argv, &key, &obj_type);

    if (rc) {
        return rc;
    }
    rc = ble_store_delete(obj_type, &key);
    return rc;
}

static int
cmd_keystore_iterator(int obj_type,
                      union ble_store_value *val,
                      void *cookie) {

    switch (obj_type) {
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
            console_printf("Key: ");
            if (ble_addr_cmp(&val->sec.peer_addr, BLE_ADDR_ANY) == 0) {
                console_printf("ediv=%u ", val->sec.ediv);
                console_printf("ediv=%llu ", val->sec.rand_num);
            } else {
                console_printf("addr_type=%u ", val->sec.peer_addr.type);
                print_addr(val->sec.peer_addr.val);
            }
            console_printf("\n");

            if (val->sec.ltk_present) {
                console_printf("    LTK: ");
                print_bytes(val->sec.ltk, 16);
                console_printf("\n");
            }
            if (val->sec.irk_present) {
                console_printf("    IRK: ");
                print_bytes(val->sec.irk, 16);
                console_printf("\n");
            }
            if (val->sec.csrk_present) {
                console_printf("    CSRK: ");
                print_bytes(val->sec.csrk, 16);
                console_printf("\n");
            }
            break;
    }
    return 0;
}

static void
bletiny_keystore_show_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available keystore show commands: \n");
    console_printf("\thelp\n");
    console_printf("Available keystore show params: \n");
    help_cmd_kv("type", cmd_keystore_entry_type);
}

static int
cmd_keystore_show(int argc, char **argv)
{
    int type;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_keystore_show_help();
        return 0;
    }

    type = parse_arg_kv("type", cmd_keystore_entry_type, &rc);
    if (rc != 0) {
        console_printf("invalid 'type' parameter\n");
        help_cmd_kv("type", cmd_keystore_entry_type);
        return rc;
    }

    ble_store_iterate(type, &cmd_keystore_iterator, NULL);
    return 0;
}

static struct cmd_entry cmd_keystore_entries[];

static int
cmd_keystore_help(int argc, char **argv)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return 0;
#endif

    int i;

    console_printf("Available keystore commands:\n");
    for (i = 0; cmd_keystore_entries[i].name != NULL; i++) {
        console_printf("\t%s\n", cmd_keystore_entries[i].name);
    }
    return 0;
}

static struct cmd_entry cmd_keystore_entries[] = {
    { "add", cmd_keystore_add },
    { "del", cmd_keystore_del },
    { "show", cmd_keystore_show },
    { "help", cmd_keystore_help },
    { NULL, NULL }
};

static int
cmd_keystore(int argc, char **argv)
{
    int rc;

    rc = cmd_exec(cmd_keystore_entries, argc, argv);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $passkey                                                                  *
 *****************************************************************************/

static void
bletiny_passkey_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available passkey commands: \n");
    console_printf("\thelp\n");
    console_printf("Available passkey params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint16("action");
    help_cmd_long_bounds("key", 0, 999999);
    help_cmd_byte_stream_exact_length("oob", 16);
    help_cmd_extract("yesno");
}

static int
cmd_passkey(int argc, char **argv)
{
#if !NIMBLE_BLE_SM
    return BLE_HS_ENOTSUP;
#endif

    uint16_t conn_handle;
    struct ble_sm_io pk;
    char *yesno;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_passkey_help();
        return 0;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    pk.action = parse_arg_uint16("action", &rc);
    if (rc != 0) {
        console_printf("invalid 'action' parameter\n");
        help_cmd_uint16("action");
        return rc;
    }

    switch (pk.action) {
        case BLE_SM_IOACT_INPUT:
        case BLE_SM_IOACT_DISP:
           /* passkey is 6 digit number */
           pk.passkey = parse_arg_long_bounds("key", 0, 999999, &rc);
           if (rc != 0) {
               console_printf("invalid 'key' parameter\n");
               help_cmd_long_bounds("key", 0, 999999);
               return rc;
           }
           break;

        case BLE_SM_IOACT_OOB:
            rc = parse_arg_byte_stream_exact_length("oob", pk.oob, 16);
            if (rc != 0) {
                console_printf("invalid 'oob' parameter\n");
                help_cmd_byte_stream_exact_length("oob", 16);
                return rc;
            }
            break;

        case BLE_SM_IOACT_NUMCMP:
            yesno = parse_arg_extract("yesno");
            if (yesno == NULL) {
                console_printf("invalid 'yesno' parameter\n");
                help_cmd_extract("yesno");
                return EINVAL;
            }

            switch (yesno[0]) {
            case 'y':
            case 'Y':
                pk.numcmp_accept = 1;
                break;
            case 'n':
            case 'N':
                pk.numcmp_accept = 0;
                break;

            default:
                console_printf("invalid 'yesno' parameter\n");
                help_cmd_extract("yesno");
                return EINVAL;
            }
            break;

       default:
         console_printf("invalid passkey action action=%d\n", pk.action);
         return EINVAL;
    }

    rc = ble_sm_inject_io(conn_handle, &pk);
    if (rc != 0) {
        console_printf("error providing passkey; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $tx                                                                     *
 *                                                                         *
 * Command to transmit 'num' packets of size 'len' at rate 'r' to
 * handle 'h' Note that length must be <= 251. The rate is in msecs.
 *
 *****************************************************************************/

static void
bletiny_tx_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available tx commands: \n");
    console_printf("\thelp\n");
    console_printf("Available tx params: \n");
    help_cmd_uint16("r");
    help_cmd_uint16("l");
    help_cmd_uint16("n");
    help_cmd_uint16("h");
}

static int
cmd_tx(int argc, char **argv)
{
    int rc;
    uint16_t rate;
    uint16_t len;
    uint16_t handle;
    uint16_t num;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_tx_help();
        return 0;
    }

    rate = parse_arg_uint16("r", &rc);
    if (rc != 0) {
        console_printf("invalid 'r' parameter\n");
        help_cmd_uint16("r");
        return rc;
    }

    len = parse_arg_uint16("l", &rc);
    if (rc != 0) {
        console_printf("invalid 'l' parameter\n");
        help_cmd_uint16("l");
        return rc;
    }
    if ((len > 251) || (len < 4)) {
        console_printf("error: len must be between 4 and 251, inclusive");
    }

    num = parse_arg_uint16("n", &rc);
    if (rc != 0) {
        console_printf("invalid 'n' parameter\n");
        help_cmd_uint16("n");
        return rc;
    }

    handle = parse_arg_uint16("h", &rc);
    if (rc != 0) {
        console_printf("invalid 'h' parameter\n");
        help_cmd_uint16("h");
        return rc;
    }

    rc = bletiny_tx_start(handle, len, rate, num);
    return rc;
}

static struct cmd_entry cmd_b_entries[];

static int
cmd_help(int argc, char **argv)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return 0;
#endif

    int i;

    console_printf("Available commands:\n");
    for (i = 0; cmd_b_entries[i].name != NULL; i++) {
        console_printf("\t%s\n", cmd_b_entries[i].name);
    }
    return 0;
}

/*****************************************************************************
 * $svcch                                                                    *
 *****************************************************************************/

static void
bletiny_svcchg_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available svcchg params: \n");
    help_cmd_uint16("start");
    help_cmd_uint16("end");
}

static int
cmd_svcchg(int argc, char **argv)
{
    uint16_t start;
    uint16_t end;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_svcchg_help();
        return 0;
    }

    start = parse_arg_uint16("start", &rc);
    if (rc != 0) {
        console_printf("invalid 'start' parameter\n");
        help_cmd_uint16("start");
        return rc;
    }

    end = parse_arg_uint16("end", &rc);
    if (rc != 0) {
        console_printf("invalid 'end' parameter\n");
        help_cmd_uint16("end");
        return rc;
    }

    ble_svc_gatt_changed(start, end);

    return 0;
}

/*****************************************************************************
 * $svcvis                                                                   *
 *****************************************************************************/

static void
bletiny_svcvis_help(void)
{
#if !MYNEWT_VAL(BLETINY_HELP)
    bletiny_help_disabled();
    return;
#endif

    console_printf("Available svcvis params: \n");
    help_cmd_uint16("handle");
    help_cmd_bool("vis");
}

static int
cmd_svcvis(int argc, char **argv)
{
    uint16_t handle;
    bool vis;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_svcvis_help();
        return 0;
    }

    handle = parse_arg_uint16("handle", &rc);
    if (rc != 0) {
        console_printf("invalid 'handle' parameter\n");
        help_cmd_uint16("handle");
        return rc;
    }

    vis = parse_arg_bool("vis", &rc);
    if (rc != 0) {
        console_printf("invalid 'vis' parameter\n");
        help_cmd_bool("vis");
        return rc;
    }

    ble_gatts_svc_set_visibility(handle, vis);

    return 0;
}

static const struct cmd_entry cmd_phy_entries[];

static int
cmd_phy_help(int argc, char **argv)
{
    int i;

    console_printf("Available PHY commands:\n");
    for (i = 0; cmd_phy_entries[i].name != NULL; i++) {
        console_printf("\t%s\n", cmd_phy_entries[i].name);
    }
    return 0;
}

static void
bletiny_phy_set_help(void)
{
    console_printf("Available PHY set commands: \n");
    console_printf("\thelp\n");
    console_printf("Available PHY set params: \n");
    help_cmd_uint16("conn");
    help_cmd_uint8("tx_phys_mask");
    help_cmd_uint8("rx_phys_mask");
    help_cmd_uint16("phy_opts");
}

static int
cmd_phy_set(int argc, char **argv)
{
    uint16_t conn;
    uint8_t tx_phys_mask;
    uint8_t rx_phys_mask;
    uint16_t phy_opts;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_phy_set_help();
        return 0;
    }

    conn = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    tx_phys_mask = parse_arg_uint8("tx_phys_mask", &rc);
    if (rc != 0) {
        console_printf("invalid 'tx_phys_mask' parameter\n");
        help_cmd_uint8("tx_phys_mask");
        return rc;
    }

    rx_phys_mask = parse_arg_uint8("rx_phys_mask", &rc);
    if (rc != 0) {
        console_printf("invalid 'rx_phys_mask' parameter\n");
        help_cmd_uint8("rx_phys_mask");
        return rc;
    }

    phy_opts = parse_arg_uint16("phy_opts", &rc);
    if (rc != 0) {
        console_printf("invalid 'phy_opts' parameter\n");
        help_cmd_uint16("phy_opts");
        return rc;
    }

    return ble_gap_set_prefered_le_phy(conn, tx_phys_mask, rx_phys_mask,
                                       phy_opts);
}

static void
bletiny_phy_set_def_help(void)
{
    console_printf("Available PHY set_def commands: \n");
    console_printf("\thelp\n");
    console_printf("Available PHY set_def params: \n");
    help_cmd_uint8("tx_phys_mask");
    help_cmd_uint8("rx_phys_mask");
}

static int
cmd_phy_set_def(int argc, char **argv)
{
    uint8_t tx_phys_mask;
    uint8_t rx_phys_mask;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        bletiny_phy_set_def_help();
        return 0;
    }

    tx_phys_mask = parse_arg_uint8("tx_phys_mask", &rc);
    if (rc != 0) {
        console_printf("invalid 'tx_phys_mask' parameter\n");
        help_cmd_uint8("tx_phys_mask");
        return rc;
    }

    rx_phys_mask = parse_arg_uint8("rx_phys_mask", &rc);
    if (rc != 0) {
        console_printf("invalid 'rx_phys_mask' parameter\n");
        help_cmd_uint8("rx_phys_mask");
        return rc;
    }

    return ble_gap_set_prefered_default_le_phy(tx_phys_mask, rx_phys_mask);
}

static void
bletiny_phy_read_help(void)
{
    console_printf("Available PHY read commands: \n");
    console_printf("\thelp\n");
    console_printf("Available PHY read params: \n");
    help_cmd_uint16("conn");
}

static int
cmd_phy_read(int argc, char **argv)
{

    uint16_t conn = 0;
    uint8_t tx_phy;
    uint8_t rx_phy;
    int rc;

    if (argc > 1 && strcmp(argv[1], "help") == 0) {
            bletiny_phy_read_help();
        return 0;
    }

    conn = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        help_cmd_uint16("conn");
        return rc;
    }

    rc = ble_gap_read_le_phy(conn, &tx_phy, &rx_phy);
    if (rc != 0) {
        console_printf("Could not read PHY error: %d\n", rc);
        return rc;
    }

    console_printf("TX_PHY: %d\n", tx_phy);
    console_printf("RX_PHY: %d\n", tx_phy);

    return 0;
}

static const struct cmd_entry cmd_phy_entries[] = {
    { "read", cmd_phy_read },
    { "set_def", cmd_phy_set_def },
    { "set", cmd_phy_set },
    { "help", cmd_phy_help },
    { NULL, NULL }
};

static int
cmd_phy(int argc, char **argv)
{
    int rc;

    rc = cmd_exec(cmd_phy_entries, argc, argv);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $init                                                                     *
 *****************************************************************************/

static struct cmd_entry cmd_b_entries[] = {
    { "adv",        cmd_adv },
    { "conn",       cmd_conn },
    { "chrup",      cmd_chrup },
    { "datalen",    cmd_datalen },
    { "disc",       cmd_disc },
    { "find",       cmd_find },
    { "help",       cmd_help },
    { "l2cap",      cmd_l2cap },
    { "mtu",        cmd_mtu },
    { "passkey",    cmd_passkey },
    { "read",       cmd_read },
    { "rssi",       cmd_rssi },
    { "scan",       cmd_scan },
    { "show",       cmd_show },
    { "sec",        cmd_sec },
    { "set",        cmd_set },
    { "store",      cmd_keystore },
    { "term",       cmd_term },
    { "update",     cmd_update },
    { "tx",         cmd_tx },
    { "wl",         cmd_wl },
    { "write",      cmd_write },
    { "svcchg",     cmd_svcchg },
    { "phy",        cmd_phy },
    { "svcvis",     cmd_svcvis },
    { NULL, NULL }
};

static int
cmd_b_exec(int argc, char **argv)
{
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        goto done;
    }

    rc = cmd_exec(cmd_b_entries, argc, argv);
    if (rc != 0) {
        console_printf("error; rc=%d\n", rc);
        goto done;
    }

    rc = 0;

done:
    return rc;
}

int
cmd_init(void)
{
    int rc;

    rc = shell_cmd_register(&cmd_b);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
