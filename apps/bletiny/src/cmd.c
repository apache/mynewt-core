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

#include <errno.h>
#include <string.h>
#include "bsp/bsp.h"
#include "console/console.h"
#include "shell/shell.h"

#include "nimble/ble.h"
#include "nimble/hci_common.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "host/ble_eddystone.h"
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
    print_uuid(dsc->dsc.uuid128);
    console_printf("\n");
}

static void
cmd_print_chr(struct bletiny_chr *chr)
{
    struct bletiny_dsc *dsc;

    console_printf("        def_handle=%d val_handle=%d properties=0x%02x "
                   "uuid=", chr->chr.def_handle, chr->chr.val_handle,
                   chr->chr.properties);
    print_uuid(chr->chr.uuid128);
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
    print_uuid(svc->svc.uuid128);
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

static struct kv_pair cmd_adv_addr_types[] = {
    { "public", BLE_ADDR_TYPE_PUBLIC },
    { "random", BLE_ADDR_TYPE_RANDOM },
    { "rpa_pub", BLE_ADDR_TYPE_RPA_PUB_DEFAULT },
    { "rpa_rnd", BLE_ADDR_TYPE_RPA_RND_DEFAULT },
    { NULL }
};

static struct kv_pair cmd_adv_filt_types[] = {
    { "none", BLE_HCI_ADV_FILT_NONE },
    { "scan", BLE_HCI_ADV_FILT_SCAN },
    { "conn", BLE_HCI_ADV_FILT_CONN },
    { "both", BLE_HCI_ADV_FILT_BOTH },
};

static int
cmd_adv(int argc, char **argv)
{
    struct ble_gap_adv_params params = {
        .adv_itvl_min = 0,
        .adv_itvl_max = 0,
        .adv_type = BLE_HCI_ADV_TYPE_ADV_IND,
        .own_addr_type = BLE_HCI_ADV_OWN_ADDR_PUBLIC,
        .adv_channel_map = BLE_HCI_ADV_CHANMASK_DEF,
        .adv_filter_policy = BLE_HCI_ADV_FILT_DEF,
    };
    uint8_t u8;
    uint8_t peer_addr_type;
    uint8_t peer_addr[8];
    int addr_type;
    int conn;
    int disc;
    int rc;

    if (argc > 1 && strcmp(argv[1], "stop") == 0) {
        rc = bletiny_adv_stop();
        if (rc != 0) {
            console_printf("advertise stop fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    conn = parse_arg_kv("conn", cmd_adv_conn_modes);
    if (conn <  0) {
        console_printf("invalid 'conn' parameter\n");
        return -1;
    }

    disc = parse_arg_kv("disc", cmd_adv_disc_modes);
    if (conn <  0) {
        console_printf("missing 'disc' parameter\n");
        return -1;
    }

    addr_type = parse_arg_kv_default
            ("peer_addr_type", cmd_adv_addr_types, BLE_ADDR_TYPE_PUBLIC);
    if (addr_type == -1) {
        return -1;
    }

    rc = parse_arg_mac("peer_addr", peer_addr);
    if (rc == ENOENT) {
        memset(peer_addr, 0, sizeof peer_addr);
    } else if (rc != 0) {
        return rc;
    }

    peer_addr_type = addr_type;

    rc = parse_arg_kv_default
                ("own_addr_type", cmd_adv_addr_types, BLE_ADDR_TYPE_PUBLIC);
    if (rc == -1) {
        return rc;
    }

    params.own_addr_type = rc;

    u8 = parse_arg_long_bounds("chan_map", 0, 0xff, &rc);
    if (rc == 0) {
        params.adv_channel_map = u8;
    } else if (rc != ENOENT) {
        return rc;
    }

    if (parse_arg_find("filt") != NULL) {
        params.adv_filter_policy = parse_arg_kv("filt", cmd_adv_filt_types);
        if (params.adv_filter_policy == -1) {
            return EINVAL;
        }
    }

    rc = bletiny_adv_start(disc, conn, peer_addr, peer_addr_type, &params);
    if (rc != 0) {
        console_printf("advertise fail: %d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $connect                                                                  *
 *****************************************************************************/

static struct kv_pair cmd_conn_peer_addr_types[] = {
    { "public",         BLE_HCI_CONN_PEER_ADDR_PUBLIC },
    { "random",         BLE_HCI_CONN_PEER_ADDR_RANDOM },
    { "public_ident",   BLE_HCI_CONN_PEER_ADDR_PUBLIC_IDENT },
    { "random_ident",   BLE_HCI_CONN_PEER_ADDR_RANDOM_IDENT },
    { "wl",             BLE_GAP_ADDR_TYPE_WL },
    { NULL }
};

static struct kv_pair cmd_conn_own_addr_types[] = {
    { "public", BLE_ADDR_TYPE_PUBLIC },
    { "random", BLE_ADDR_TYPE_RANDOM },
    { "rpa_pub", BLE_ADDR_TYPE_RPA_PUB_DEFAULT },
    { "rpa_rnd", BLE_ADDR_TYPE_RPA_RND_DEFAULT },
    { NULL }
};

static int
cmd_conn(int argc, char **argv)
{
    struct ble_gap_crt_params params;
    uint8_t peer_addr[6];
    int addr_type;
    int rc;

    if (argc > 1 && strcmp(argv[1], "cancel") == 0) {
        rc = bletiny_conn_cancel();
        if (rc != 0) {
            console_printf("connection cancel fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    addr_type = parse_arg_kv("peer_addr_type", cmd_conn_peer_addr_types);
    if (addr_type == -1) {
        return -1;
    }

    if (addr_type != BLE_GAP_ADDR_TYPE_WL) {
        rc = parse_arg_mac("addr", peer_addr);
        if (rc != 0) {
            return rc;
        }
    } else {
        memset(peer_addr, 0, sizeof peer_addr);
    }

    rc = parse_arg_kv_default("own_addr_type",
                        cmd_conn_own_addr_types, BLE_ADDR_TYPE_PUBLIC);
    if (rc < 0) {
        return rc;
    }
    params.our_addr_type = rc;

    params.scan_itvl = parse_arg_uint16_dflt("scan_itvl", 0x0010, &rc);
    if (rc != 0) {
        return rc;
    }

    params.scan_window = parse_arg_uint16_dflt("scan_window", 0x0010, &rc);
    if (rc != 0) {
        return rc;
    }

    params.itvl_min = parse_arg_uint16_dflt(
        "itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN, &rc);
    if (rc != 0) {
        return rc;
    }

    params.itvl_max = parse_arg_uint16_dflt(
        "itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        return rc;
    }

    params.latency = parse_arg_uint16_dflt("latency", 0, &rc);
    if (rc != 0) {
        return rc;
    }

    params.supervision_timeout = parse_arg_uint16_dflt("timeout", 0x0100, &rc);
    if (rc != 0) {
        return rc;
    }

    params.min_ce_len = parse_arg_uint16_dflt("min_ce_len", 0x0010, &rc);
    if (rc != 0) {
        return rc;
    }

    params.max_ce_len = parse_arg_uint16_dflt("max_ce_len", 0x0300, &rc);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_conn_initiate(addr_type, peer_addr, &params);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $chrup                                                                    *
 *****************************************************************************/

static int
cmd_chrup(int argc, char **argv)
{
    uint16_t attr_handle;
    int rc;

    attr_handle = parse_arg_long("attr", &rc);
    if (rc != 0) {
        return rc;
    }

    bletiny_chrup(attr_handle);

    return 0;
}

/*****************************************************************************
 * $datalen                                                                  *
 *****************************************************************************/

static int
cmd_datalen(int argc, char **argv)
{
    uint16_t conn_handle;
    uint16_t tx_octets;
    uint16_t tx_time;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    tx_octets = parse_arg_long("octets", &rc);
    if (rc != 0) {
        return rc;
    }

    tx_time = parse_arg_long("time", &rc);
    if (rc != 0) {
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

static int
cmd_disc_chr(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    uint8_t uuid128[16];
    int rc;

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        return rc;
    }

    rc = parse_arg_uuid("uuid", uuid128);
    if (rc == 0) {
        rc = bletiny_disc_chrs_by_uuid(conn_handle, start_handle, end_handle,
                                        uuid128);
    } else if (rc == ENOENT) {
        rc = bletiny_disc_all_chrs(conn_handle, start_handle, end_handle);
    } else  {
        return rc;
    }
    if (rc != 0) {
        console_printf("error discovering characteristics; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static int
cmd_disc_dsc(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    int rc;

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_disc_all_dscs(conn_handle, start_handle, end_handle);
    if (rc != 0) {
        console_printf("error discovering descriptors; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static int
cmd_disc_svc(int argc, char **argv)
{
    uint8_t uuid128[16];
    int conn_handle;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    rc = parse_arg_uuid("uuid", uuid128);
    if (rc == 0) {
        rc = bletiny_disc_svc_by_uuid(conn_handle, uuid128);
    } else if (rc == ENOENT) {
        rc = bletiny_disc_svcs(conn_handle);
    } else  {
        return rc;
    }

    if (rc != 0) {
        console_printf("error discovering services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static int
cmd_disc_full(int argc, char **argv)
{
    int conn_handle;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_disc_full(conn_handle);
    if (rc != 0) {
        console_printf("error discovering all; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static struct cmd_entry cmd_disc_entries[] = {
    { "chr", cmd_disc_chr },
    { "dsc", cmd_disc_dsc },
    { "svc", cmd_disc_svc },
    { "full", cmd_disc_full },
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

static int
cmd_find_inc_svcs(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    int rc;

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_find_inc_svcs(conn_handle, start_handle, end_handle);
    if (rc != 0) {
        console_printf("error finding included services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct cmd_entry cmd_find_entries[] = {
    { "inc_svcs", cmd_find_inc_svcs },
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

static int
cmd_l2cap_update(int argc, char **argv)
{
    struct ble_l2cap_sig_update_params params;
    uint16_t conn_handle;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    params.itvl_min = parse_arg_uint16_dflt(
        "itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN, &rc);
    if (rc != 0) {
        return rc;
    }

    params.itvl_max = parse_arg_uint16_dflt(
        "itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        return rc;
    }

    params.slave_latency = parse_arg_uint16_dflt("latency", 0, &rc);
    if (rc != 0) {
        return rc;
    }

    params.timeout_multiplier = parse_arg_uint16_dflt("timeout", 0x0100, &rc);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_l2cap_update(conn_handle, &params);
    if (rc != 0) {
        console_printf("error txing l2cap update; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct cmd_entry cmd_l2cap_entries[] = {
    { "update", cmd_l2cap_update },
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

static int
cmd_mtu(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
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

static int
cmd_read(int argc, char **argv)
{
    static uint16_t attr_handles[CMD_READ_MAX_ATTRS];
    uint16_t conn_handle;
    uint16_t start;
    uint16_t end;
    uint8_t uuid128[16];
    uint8_t num_attr_handles;
    int is_uuid;
    int is_long;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    is_long = parse_arg_long("long", &rc);
    if (rc == ENOENT) {
        is_long = 0;
    } else if (rc != 0) {
        return rc;
    }

    for (num_attr_handles = 0;
         num_attr_handles < CMD_READ_MAX_ATTRS;
         num_attr_handles++) {

        attr_handles[num_attr_handles] = parse_arg_uint16("attr", &rc);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            return rc;
        }
    }

    rc = parse_arg_uuid("uuid", uuid128);
    if (rc == ENOENT) {
        is_uuid = 0;
    } else if (rc == 0) {
        is_uuid = 1;
    } else {
        return rc;
    }

    start = parse_arg_uint16("start", &rc);
    if (rc == ENOENT) {
        start = 0;
    } else if (rc != 0) {
        return rc;
    }

    end = parse_arg_uint16("end", &rc);
    if (rc == ENOENT) {
        end = 0;
    } else if (rc != 0) {
        return rc;
    }

    if (num_attr_handles == 1) {
        if (is_long) {
            rc = bletiny_read_long(conn_handle, attr_handles[0]);
        } else {
            rc = bletiny_read(conn_handle, attr_handles[0]);
        }
    } else if (num_attr_handles > 1) {
        rc = bletiny_read_mult(conn_handle, attr_handles, num_attr_handles);
    } else if (is_uuid) {
        if (start == 0 || end == 0) {
            rc = EINVAL;
        } else {
            rc = bletiny_read_by_uuid(conn_handle, start, end, uuid128);
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
 * $scan                                                                     *
 *****************************************************************************/

static struct kv_pair cmd_scan_disc_modes[] = {
    { "ltd", BLE_GAP_DISC_MODE_LTD },
    { "gen", BLE_GAP_DISC_MODE_GEN },
    { NULL }
};

static struct kv_pair cmd_scan_types[] = {
    { "passive", BLE_HCI_SCAN_TYPE_PASSIVE },
    { "active", BLE_HCI_SCAN_TYPE_ACTIVE },
    { NULL }
};

static struct kv_pair cmd_scan_filt_policies[] = {
    { "no_wl", BLE_HCI_SCAN_FILT_NO_WL },
    { "use_wl", BLE_HCI_SCAN_FILT_USE_WL },
    { "no_wl_inita", BLE_HCI_SCAN_FILT_NO_WL_INITA },
    { "use_wl_inita", BLE_HCI_SCAN_FILT_USE_WL_INITA },
    { NULL }
};

static struct kv_pair cmd_scan_addr_types[] = {
    { "public",  BLE_ADDR_TYPE_PUBLIC },
    { "random",  BLE_ADDR_TYPE_RANDOM },
    { "rpa_pub", BLE_ADDR_TYPE_RPA_PUB_DEFAULT },
    { "rpa_rnd", BLE_ADDR_TYPE_RPA_RND_DEFAULT },
    { NULL }
};

static int
cmd_scan(int argc, char **argv)
{
    uint32_t dur;
    int disc;
    int type;
    int filt;
    int rc;
    int addr_mode;

    dur = parse_arg_uint16("dur", &rc);
    if (rc != 0) {
        return rc;
    }

    disc = parse_arg_kv("disc", cmd_scan_disc_modes);
    if (disc < 0) {
        return EINVAL;
    }

    type = parse_arg_kv("type", cmd_scan_types);
    if (type < 0) {
        return EINVAL;
    }

    filt = parse_arg_kv("filt", cmd_scan_filt_policies);
    if (filt < 0) {
        return EINVAL;
    }

    addr_mode = parse_arg_kv_default("addr_mode",
                                    cmd_scan_addr_types, BLE_ADDR_TYPE_PUBLIC);
    if (addr_mode == -1) {
        return EINVAL;
    }

    rc = bletiny_scan(dur, disc, type, filt, addr_mode);
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
    uint8_t *id_addr;
    uint8_t id_addr_type;

    id_addr = bls_hs_priv_get_local_identity_addr(&id_addr_type);

    console_printf("id_addr_type=%d id_addr=", id_addr_type);
    print_addr(id_addr);
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

        rc = ble_gap_find_conn(conn->handle, &conn_desc);
        if (rc == 0) {
            print_conn_desc(&conn_desc);
        }
    }

    return 0;
}

static struct cmd_entry cmd_show_entries[] = {
    { "addr", cmd_show_addr },
    { "chr", cmd_show_chr },
    { "conn", cmd_show_conn },
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

static int
cmd_sec_start(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_sec_start(conn_handle);
    if (rc != 0) {
        console_printf("error starting security; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static int
cmd_sec_restart(int argc, char **argv)
{
    uint16_t conn_handle;
    uint16_t ediv;
    uint64_t rand_val;
    uint8_t ltk[16];
    int rc;
    int auth;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    ediv = parse_arg_uint16("ediv", &rc);
    if (rc == ENOENT) {
        rc = bletiny_sec_restart(conn_handle, NULL, 0, 0, 0);
    } else {
        rand_val = parse_arg_uint64("rand", &rc);
        if (rc != 0) {
            return rc;
        }

        auth = parse_arg_bool("auth", &rc);
        if (rc != 0) {
            return rc;
        }

        rc = parse_arg_byte_stream_exact_length("ltk", ltk, 16);
        if (rc != 0) {
            return rc;
        }

        rc = bletiny_sec_restart(conn_handle, ltk, ediv, rand_val, auth);
    }

    if (rc != 0) {
        console_printf("error starting encryption; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static struct cmd_entry cmd_sec_entries[] = {
    { "start", cmd_sec_start },
    { "restart", cmd_sec_restart },
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
#define CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN    32
#define CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN    32
#define CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN   32
#define CMD_ADV_DATA_URI_MAX_LEN                32
#define CMD_ADV_DATA_MFG_DATA_MAX_LEN           32

static int
cmd_set_adv_data(void)
{
    static bssnz_t uint16_t uuids16[CMD_ADV_DATA_MAX_UUIDS16];
    static bssnz_t uint32_t uuids32[CMD_ADV_DATA_MAX_UUIDS32];
    static bssnz_t uint8_t uuids128[CMD_ADV_DATA_MAX_UUIDS128][16];
    static bssnz_t uint8_t
        public_tgt_addrs[CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS]
                        [BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN];
    static bssnz_t uint8_t device_class[BLE_HS_ADV_DEVICE_CLASS_LEN];
    static bssnz_t uint8_t slave_itvl_range[BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN];
    static bssnz_t uint8_t
        svc_data_uuid16[CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN];
    static bssnz_t uint8_t
        svc_data_uuid32[CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN];
    static bssnz_t uint8_t
        svc_data_uuid128[CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN];
    static bssnz_t uint8_t le_addr[BLE_HS_ADV_LE_ADDR_LEN];
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

    while (1) {
        uuid16 = parse_arg_uint16("uuid16", &rc);
        if (rc == 0) {
            if (adv_fields.num_uuids16 >= CMD_ADV_DATA_MAX_UUIDS16) {
                return EINVAL;
            }
            uuids16[adv_fields.num_uuids16] = uuid16;
            adv_fields.num_uuids16++;
        } else if (rc == ENOENT) {
            break;
        } else {
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
        return rc;
    }

    while (1) {
        uuid32 = parse_arg_uint32("uuid32", &rc);
        if (rc == 0) {
            if (adv_fields.num_uuids32 >= CMD_ADV_DATA_MAX_UUIDS32) {
                return EINVAL;
            }
            uuids32[adv_fields.num_uuids32] = uuid32;
            adv_fields.num_uuids32++;
        } else if (rc == ENOENT) {
            break;
        } else {
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
        return rc;
    }

    while (1) {
        rc = parse_arg_byte_stream_exact_length("uuid128", uuid128, 16);
        if (rc == 0) {
            if (adv_fields.num_uuids128 >= CMD_ADV_DATA_MAX_UUIDS128) {
                return EINVAL;
            }
            memcpy(uuids128[adv_fields.num_uuids128], uuid128, 16);
            adv_fields.num_uuids128++;
        } else if (rc == ENOENT) {
            break;
        } else {
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
        return rc;
    }

    adv_fields.name = (uint8_t *)parse_arg_find("name");
    if (adv_fields.name != NULL) {
        adv_fields.name_len = strlen((char *)adv_fields.name);
    }

    tmp = parse_arg_long_bounds("tx_pwr_lvl", 0, 0xff, &rc);
    if (rc == 0) {
        adv_fields.tx_pwr_lvl = tmp;
        adv_fields.tx_pwr_lvl_is_present = 1;
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream_exact_length("device_class", device_class,
                                            BLE_HS_ADV_DEVICE_CLASS_LEN);
    if (rc == 0) {
        adv_fields.device_class = device_class;
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream_exact_length("slave_itvl_range",
                                            slave_itvl_range,
                                            BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
    if (rc == 0) {
        adv_fields.slave_itvl_range = slave_itvl_range;
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream("svc_data_uuid16",
                               CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN,
                               svc_data_uuid16, &svc_data_uuid16_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid16 = svc_data_uuid16;
        adv_fields.svc_data_uuid16_len = svc_data_uuid16_len;
    } else if (rc != ENOENT) {
        return rc;
    }

    while (1) {
        rc = parse_arg_byte_stream_exact_length(
            "public_tgt_addr", public_tgt_addr,
            BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
        if (rc == 0) {
            if (adv_fields.num_public_tgt_addrs >=
                CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS) {

                return EINVAL;
            }
            memcpy(public_tgt_addrs[adv_fields.num_public_tgt_addrs],
                   public_tgt_addr, BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
            adv_fields.num_public_tgt_addrs++;
        } else if (rc == ENOENT) {
            break;
        } else {
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
        return rc;
    }

    adv_fields.adv_itvl = parse_arg_uint16("adv_itvl", &rc);
    if (rc == 0) {
        adv_fields.adv_itvl_is_present = 1;
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream_exact_length("le_addr", le_addr,
                                            BLE_HS_ADV_LE_ADDR_LEN);
    if (rc == 0) {
        adv_fields.le_addr = le_addr;
    } else if (rc != ENOENT) {
        return rc;
    }

    adv_fields.le_role = parse_arg_long_bounds("le_role", 0, 0xff, &rc);
    if (rc == 0) {
        adv_fields.le_role_is_present = 1;
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream("svc_data_uuid32",
                               CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN,
                               svc_data_uuid32, &svc_data_uuid32_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid32 = svc_data_uuid32;
        adv_fields.svc_data_uuid32_len = svc_data_uuid32_len;
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream("svc_data_uuid128",
                               CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN,
                               svc_data_uuid128, &svc_data_uuid128_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid128 = svc_data_uuid128;
        adv_fields.svc_data_uuid128_len = svc_data_uuid128_len;
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream("uri", CMD_ADV_DATA_URI_MAX_LEN, uri, &uri_len);
    if (rc == 0) {
        adv_fields.uri = uri;
        adv_fields.uri_len = uri_len;
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream("mfg_data", CMD_ADV_DATA_MFG_DATA_MAX_LEN,
                               mfg_data, &mfg_data_len);
    if (rc == 0) {
        adv_fields.mfg_data = mfg_data;
        adv_fields.mfg_data_len = mfg_data_len;
    } else if (rc != ENOENT) {
        return rc;
    }

    eddystone_url_full = parse_arg_find("eddystone_url");
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
        return rc;
    }

    tmp = parse_arg_bool("mitm_flag", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_mitm = tmp;
    } else if (rc != ENOENT) {
        return rc;
    }

    tmp = parse_arg_uint8("io_capabilities", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_io_cap = tmp;
    } else if (rc != ENOENT) {
        return rc;
    }

    tmp = parse_arg_uint8("our_key_dist", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_our_key_dist = tmp;
    } else if (rc != ENOENT) {
        return rc;
    }

    tmp = parse_arg_uint8("their_key_dist", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_their_key_dist = tmp;
    } else if (rc != ENOENT) {
        return rc;
    }

    tmp = parse_arg_bool("bonding", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_bonding = tmp;
    } else if (rc != ENOENT) {
        return rc;
    }

    tmp = parse_arg_bool("sc", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_sc = tmp;
    } else if (rc != ENOENT) {
        return rc;
    }

    if (!good) {
        console_printf("Error: no valid settings specified\n");
        return -1;
    }

    return 0;
}

static int
cmd_set(int argc, char **argv)
{
    uint16_t mtu;
    uint8_t addr[6];
    uint8_t irk[16];
    int good;
    int rc;

    if (argc > 1 && strcmp(argv[1], "adv_data") == 0) {
        rc = cmd_set_adv_data();
        return rc;
    }

    if (argc > 1 && strcmp(argv[1], "sm_data") == 0) {
        rc = cmd_set_sm_data();
        return rc;
    }

    good = 0;

    rc = parse_arg_mac("addr", addr);
    if (rc == 0) {
        good = 1;
        /* XXX: There are a lot of problems with this.  This command probably
         * needs to be removed.
         */
        memcpy(g_dev_addr, addr, 6);
        ble_gap_init_identity_addr(g_dev_addr);
    } else if (rc != ENOENT) {
        return rc;
    }

    mtu = parse_arg_uint16("mtu", &rc);
    if (rc == 0) {
        rc = ble_att_set_preferred_mtu(mtu);
        if (rc == 0) {
            good = 1;
        }
    } else if (rc != ENOENT) {
        return rc;
    }

    rc = parse_arg_byte_stream_exact_length("irk", irk, 16);
    if (rc == 0) {
        good = 1;
        ble_hs_priv_update_irk(irk);
    } else if (rc != ENOENT) {
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

static int
cmd_term(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_term_conn(conn_handle);
    if (rc != 0) {
        console_printf("error terminating connection; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $update connection parameters                                             *
 *****************************************************************************/

static int
cmd_update(int argc, char **argv)
{
    struct ble_gap_upd_params params;
    uint16_t conn_handle;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    params.itvl_min = parse_arg_uint16_dflt(
        "itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN, &rc);
    if (rc != 0) {
        return rc;
    }

    params.itvl_max = parse_arg_uint16_dflt(
        "itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        return rc;
    }

    params.latency = parse_arg_uint16_dflt("latency", 0, &rc);
    if (rc != 0) {
        return rc;
    }

    params.supervision_timeout = parse_arg_uint16_dflt("timeout", 0x0100, &rc);
    if (rc != 0) {
        return rc;
    }

    params.min_ce_len = parse_arg_uint16_dflt("min_ce_len", 0x0010, &rc);
    if (rc != 0) {
        return rc;
    }

    params.max_ce_len = parse_arg_uint16_dflt("max_ce_len", 0x0300, &rc);
    if (rc != 0) {
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

static struct kv_pair cmd_wl_addr_types[] = {
    { "public",         BLE_HCI_CONN_PEER_ADDR_PUBLIC },
    { "random",         BLE_HCI_CONN_PEER_ADDR_RANDOM },
    { NULL }
};

#define CMD_WL_MAX_SZ   8

static int
cmd_wl(int argc, char **argv)
{
    static struct ble_gap_white_entry white_list[CMD_WL_MAX_SZ];
    uint8_t addr_type;
    uint8_t addr[6];
    int wl_cnt;
    int rc;

    wl_cnt = 0;
    while (1) {
        if (wl_cnt >= CMD_WL_MAX_SZ) {
            return EINVAL;
        }

        rc = parse_arg_mac("addr", addr);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            return rc;
        }

        addr_type = parse_arg_kv("addr_type", cmd_wl_addr_types);
        if (addr_type == -1) {
            return EINVAL;
        }

        memcpy(white_list[wl_cnt].addr, addr, 6);
        white_list[wl_cnt].addr_type = addr_type;
        wl_cnt++;
    }

    if (wl_cnt == 0) {
        return EINVAL;
    }

    bletiny_wl_set(white_list, wl_cnt);

    return 0;
}

/*****************************************************************************
 * $write                                                                    *
 *****************************************************************************/

#define CMD_WRITE_MAX_ATTRS 16

static int
cmd_write(int argc, char **argv)
{
    static struct ble_gatt_attr attrs[CMD_WRITE_MAX_ATTRS];
    uint16_t attr_handle;
    uint16_t conn_handle;
    int total_attr_len;
    int num_attrs;
    int attr_len;
    int is_long;
    int no_rsp;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    no_rsp = parse_arg_long("no_rsp", &rc);
    if (rc == ENOENT) {
        no_rsp = 0;
    } else if (rc != 0) {
        return rc;
    }

    is_long = parse_arg_long("long", &rc);
    if (rc == ENOENT) {
        is_long = 0;
    } else if (rc != 0) {
        return rc;
    }

    total_attr_len = 0;
    num_attrs = 0;
    while (1) {
        attr_handle = parse_arg_long("attr", &rc);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            return rc;
        }

        rc = parse_arg_byte_stream("value", sizeof cmd_buf - total_attr_len,
                                   cmd_buf + total_attr_len, &attr_len);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            return rc;
        }

        if (num_attrs >= CMD_WRITE_MAX_ATTRS) {
            return EINVAL;
        }

        attrs[num_attrs].handle = attr_handle;
        attrs[num_attrs].offset = 0;
        attrs[num_attrs].value_len = attr_len;
        attrs[num_attrs].value = cmd_buf + total_attr_len;

        total_attr_len += attr_len;
        num_attrs++;
    }

    if (no_rsp) {
        if (num_attrs != 1) {
            return EINVAL;
        }
        rc = bletiny_write_no_rsp(conn_handle, attrs[0].handle,
                                   attrs[0].value, attrs[0].value_len);
    } else if (is_long) {
        if (num_attrs != 1) {
            return EINVAL;
        }
        rc = bletiny_write_long(conn_handle, attrs[0].handle,
                                attrs[0].value, attrs[0].value_len);
    } else if (num_attrs > 1) {
        rc = bletiny_write_reliable(conn_handle, attrs, num_attrs);
    } else if (num_attrs == 1) {
        rc = bletiny_write(conn_handle, attrs[0].handle,
                           attrs[0].value, attrs[0].value_len);
    } else {
        return EINVAL;
    }

    if (rc != 0) {
        console_printf("error writing characteristic; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * store                                                                     *
 *****************************************************************************/

static struct kv_pair cmd_keystore_entry_type[] = {
    { "msec",       BLE_STORE_OBJ_TYPE_MST_SEC },
    { "ssec",       BLE_STORE_OBJ_TYPE_SLV_SEC },
    { "cccd",       BLE_STORE_OBJ_TYPE_CCCD },
    { NULL }
};

static struct kv_pair cmd_keystore_addr_type[] = {
    { "public",     BLE_ADDR_TYPE_PUBLIC },
    { "random",     BLE_ADDR_TYPE_RANDOM },
    { NULL }
};

static int
cmd_keystore_parse_keydata(int argc, char **argv, union ble_store_key *out,
                           int *obj_type)
{
    int rc;

    memset(out, 0, sizeof(*out));
    *obj_type = parse_arg_kv("type", cmd_keystore_entry_type);

    switch (*obj_type) {
    case BLE_STORE_OBJ_TYPE_MST_SEC:
    case BLE_STORE_OBJ_TYPE_SLV_SEC:
        rc = parse_arg_kv("addr_type", cmd_keystore_addr_type);
        if (rc < 0) {
            return EINVAL;
        }

        rc = parse_arg_mac("addr", out->sec.peer_addr);
        if (rc != 0) {
            return rc;
        }

        out->sec.ediv = parse_arg_uint16("ediv", &rc);
        if (rc != 0) {
            return rc;
        }

        out->sec.rand_num = parse_arg_uint64("rand", &rc);
        if (rc != 0) {
            return rc;
        }
        return 0;

    default:
        return -1;
    }
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
        case BLE_STORE_OBJ_TYPE_MST_SEC:
        case BLE_STORE_OBJ_TYPE_SLV_SEC:
            rc = parse_arg_byte_stream_exact_length("ltk", out->sec.ltk, 16);
            if (rc == 0) {
                out->sec.ltk_present = 1;
                swap_in_place(out->sec.ltk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                return rc;
            }
            rc = parse_arg_byte_stream_exact_length("irk", out->sec.irk, 16);
            if (rc == 0) {
                out->sec.irk_present = 1;
                swap_in_place(out->sec.irk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                return rc;
            }
            rc = parse_arg_byte_stream_exact_length("csrk", out->sec.csrk, 16);
            if (rc == 0) {
                out->sec.csrk_present = 1;
                swap_in_place(out->sec.csrk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                return rc;
            }
            out->sec.peer_addr_type = key->sec.peer_addr_type;
            memcpy(out->sec.peer_addr, key->sec.peer_addr, 6);
            out->sec.ediv = key->sec.ediv;
            out->sec.rand_num = key->sec.rand_num;
            break;
    }

    if (valcnt) {
        return 0;
    }
    return -1;
}

static int
cmd_keystore_add(int argc, char **argv)
{
    union ble_store_key key;
    union ble_store_value value;
    int obj_type;
    int rc;

    rc = cmd_keystore_parse_keydata(argc, argv, &key, &obj_type);

    if (rc) {
        return rc;
    }

    rc = cmd_keystore_parse_valuedata(argc, argv, obj_type, &key, &value);

    if (rc) {
        return rc;
    }

    switch(obj_type) {
        case BLE_STORE_OBJ_TYPE_MST_SEC:
            rc = ble_store_write_mst_sec(&value.sec);
            break;
        case BLE_STORE_OBJ_TYPE_SLV_SEC:
            rc = ble_store_write_slv_sec(&value.sec);
            break;
        case BLE_STORE_OBJ_TYPE_CCCD:
            rc = ble_store_write_cccd(&value.cccd);
            break;
        default:
            rc = ble_store_write(obj_type, &value);
    }
    return rc;
}

static int
cmd_keystore_del(int argc, char **argv)
{
    union ble_store_key key;
    int obj_type;
    int rc;

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
        case BLE_STORE_OBJ_TYPE_MST_SEC:
        case BLE_STORE_OBJ_TYPE_SLV_SEC:
            console_printf("Key: ");
            if (val->sec.peer_addr_type == BLE_STORE_ADDR_TYPE_NONE) {
                console_printf("ediv=%u ", val->sec.ediv);
                console_printf("ediv=%llu ", val->sec.rand_num);
            } else {
                console_printf("addr_type=%u ", val->sec.peer_addr_type);
                print_addr(val->sec.peer_addr);
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

static int
cmd_keystore_show(int argc, char **argv)
{
    int type;

    type = parse_arg_kv("type", cmd_keystore_entry_type);

    if (type < 0) {
        return type;
    }

    ble_store_iterate(type, &cmd_keystore_iterator, NULL);
    return 0;
}

static struct cmd_entry cmd_keystore_entries[] = {
    { "add", cmd_keystore_add },
    { "del", cmd_keystore_del },
    { "show", cmd_keystore_show },
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

static int
cmd_passkey(int argc, char **argv)
{
#if !NIMBLE_OPT(SM)
    return BLE_HS_ENOTSUP;
#endif

    uint16_t conn_handle;
    struct ble_sm_io pk;
    char *yesno;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    pk.action = parse_arg_uint16("action", &rc);
    if (rc != 0) {
        return rc;
    }

    switch (pk.action) {
        case BLE_SM_IOACT_INPUT:
        case BLE_SM_IOACT_DISP:
           /* passkey is 6 digit number */
           pk.passkey = parse_arg_long_bounds("key", 0, 999999, &rc);
           if (rc != 0) {
               return rc;
           }
           break;

        case BLE_SM_IOACT_OOB:
            rc = parse_arg_byte_stream_exact_length("oob", pk.oob, 16);
            if (rc != 0) {
                return rc;
            }
            break;

        case BLE_SM_IOACT_NUMCMP:
            yesno = parse_arg_find("yesno");
            if (yesno == NULL) {
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
static int
cmd_tx(int argc, char **argv)
{
    int rc;
    uint16_t rate;
    uint16_t len;
    uint16_t handle;
    uint16_t num;

    rate = parse_arg_uint16("r", &rc);
    if (rc != 0) {
        return rc;
    }

    len = parse_arg_uint16("l", &rc);
    if (rc != 0) {
        return rc;
    }
    if ((len > 251) || (len < 4)) {
        console_printf("error: len must be between 4 and 251, inclusive");
    }

    num = parse_arg_uint16("n", &rc);
    if (rc != 0) {
        return rc;
    }

    handle = parse_arg_uint16("h", &rc);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_tx_start(handle, len, rate, num);
    return rc;
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
    { "l2cap",      cmd_l2cap },
    { "mtu",        cmd_mtu },
    { "passkey",    cmd_passkey },
    { "read",       cmd_read },
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
