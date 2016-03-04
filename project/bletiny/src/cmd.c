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
#include "../src/ble_l2cap_priv.h"

#include "bletiny_priv.h"

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
        BLETINY_LOG(ERROR, "Error: unknown %s command: %s\n",
                    argv[0], argv[1]);
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
    BLETINY_LOG(INFO, "            dsc_handle=%d uuid=", dsc->dsc.handle);
    print_uuid(dsc->dsc.uuid128);
    BLETINY_LOG(INFO, "\n");
}

static void
cmd_print_chr(struct bletiny_chr *chr)
{
    struct bletiny_dsc *dsc;

    BLETINY_LOG(INFO, "        def_handle=%d val_handle=%d properties=0x%02x "
                      "uuid=", chr->chr.decl_handle, chr->chr.value_handle,
                chr->chr.properties);
    print_uuid(chr->chr.uuid128);
    BLETINY_LOG(INFO, "\n");

    SLIST_FOREACH(dsc, &chr->dscs, next) {
        cmd_print_dsc(dsc);
    }
}

static void
cmd_print_svc(struct bletiny_svc *svc, int print_chrs)
{
    struct bletiny_chr *chr;

    BLETINY_LOG(INFO, "    start=%d end=%d uuid=", svc->svc.start_handle,
                svc->svc.end_handle);
    print_uuid(svc->svc.uuid128);
    BLETINY_LOG(INFO, "\n");

    if (print_chrs) {
        SLIST_FOREACH(chr, &svc->chrs, next) {
            cmd_print_chr(chr);
        }
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
    struct hci_adv_params params = {
        .adv_itvl_min = 0,
        .adv_itvl_max = 0,
        .adv_type = BLE_HCI_ADV_TYPE_ADV_IND,
        .own_addr_type = BLE_HCI_ADV_OWN_ADDR_PUBLIC,
        .peer_addr_type = BLE_HCI_ADV_PEER_ADDR_PUBLIC,
        .adv_channel_map = BLE_HCI_ADV_CHANMASK_DEF,
        .adv_filter_policy = BLE_HCI_ADV_FILT_DEF,
    };
    uint8_t peer_addr[6];
    uint8_t u8;
    int addr_type;
    int conn;
    int disc;
    int rc;

    if (argc > 1 && strcmp(argv[1], "stop") == 0) {
        rc = bletiny_adv_stop();
        if (rc != 0) {
            BLETINY_LOG(INFO, "advertise stop fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    conn = parse_arg_kv("conn", cmd_adv_conn_modes);
    if (conn == -1) {
        BLETINY_LOG(ERROR, "invalid 'conn' parameter\n");
        return -1;
    }

    disc = parse_arg_kv("disc", cmd_adv_disc_modes);
    if (disc == -1) {
        BLETINY_LOG(ERROR, "missing 'disc' parameter\n");
        return -1;
    }

    if (conn == BLE_GAP_CONN_MODE_DIR) {
        addr_type = parse_arg_kv("addr_type", cmd_adv_addr_types);
        if (addr_type == -1) {
            return -1;
        }

        rc = parse_arg_mac("addr", peer_addr);
        if (rc != 0) {
            return rc;
        }
    } else {
        addr_type = 0;
        memset(peer_addr, 0, sizeof peer_addr);
    }

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

    rc = bletiny_adv_start(disc, conn, peer_addr, addr_type, &params);
    if (rc != 0) {
        BLETINY_LOG(INFO, "advertise fail: %d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $connect                                                                  *
 *****************************************************************************/

static struct kv_pair cmd_conn_addr_types[] = {
    { "public",         BLE_HCI_CONN_PEER_ADDR_PUBLIC },
    { "random",         BLE_HCI_CONN_PEER_ADDR_RANDOM },
    { "public_ident",   BLE_HCI_CONN_PEER_ADDR_PUBLIC_IDENT },
    { "random_ident",   BLE_HCI_CONN_PEER_ADDR_RANDOM_IDENT },
    { "wl",             BLE_GAP_ADDR_TYPE_WL },
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
            BLETINY_LOG(INFO, "connection cancel fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    addr_type = parse_arg_kv("addr_type", cmd_conn_addr_types);
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
 * $connect                                                                  *
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
        BLETINY_LOG(INFO, "error discovering characteristics; rc=%d\n", rc);
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
        BLETINY_LOG(INFO, "error discovering descriptors; rc=%d\n", rc);
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
        BLETINY_LOG(INFO, "error discovering services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static struct cmd_entry cmd_disc_entries[] = {
    { "chr", cmd_disc_chr },
    { "dsc", cmd_disc_dsc },
    { "svc", cmd_disc_svc },
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
        BLETINY_LOG(INFO, "error finding included services; rc=%d\n", rc);
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
        BLETINY_LOG(INFO, "error txing l2cap update; rc=%d\n", rc);
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
        BLETINY_LOG(INFO, "error exchanging mtu; rc=%d\n", rc);
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
        BLETINY_LOG(INFO, "error reading characteristic; rc=%d\n", rc);
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

static int
cmd_scan(int argc, char **argv)
{
    uint32_t dur;
    int disc;
    int type;
    int filt;
    int rc;

    dur = parse_arg_uint16("dur", &rc);
    if (rc != 0) {
        return rc;
    }

    disc = parse_arg_kv("disc", cmd_scan_disc_modes);
    if (disc == -1) {
        return EINVAL;
    }

    type = parse_arg_kv("type", cmd_scan_types);
    if (type == -1) {
        return EINVAL;
    }

    filt = parse_arg_kv("filt", cmd_scan_filt_policies);
    if (disc == -1) {
        return EINVAL;
    }

    rc = bletiny_scan(dur, disc, type, filt);
    if (rc != 0) {
        BLETINY_LOG(INFO, "error scanning; rc=%d\n", rc);
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
    bletiny_lock();

    BLETINY_LOG(INFO, "myaddr=");
    print_addr(g_dev_addr);
    BLETINY_LOG(INFO, "\n");

    bletiny_unlock();

    return 0;
}

static int
cmd_show_chr(int argc, char **argv)
{
    struct bletiny_conn *conn;
    struct bletiny_svc *svc;
    int i;

    bletiny_lock();

    for (i = 0; i < bletiny_num_conns; i++) {
        conn = bletiny_conns + i;

        BLETINY_LOG(INFO, "CONNECTION: handle=%d addr=", conn->handle);
        print_addr(conn->addr);
        BLETINY_LOG(INFO, "\n");

        SLIST_FOREACH(svc, &conn->svcs, next) {
            cmd_print_svc(svc, 1);
        }
    }

    bletiny_unlock();

    return 0;
}

static int
cmd_show_conn(int argc, char **argv)
{
    struct bletiny_conn *conn;
    int i;

    bletiny_lock();

    for (i = 0; i < bletiny_num_conns; i++) {
        conn = bletiny_conns + i;

        BLETINY_LOG(INFO, "handle=%d addr=", conn->handle);
        print_addr(conn->addr);
        BLETINY_LOG(INFO, " addr_type=%d\n", conn->addr_type);
    }

    bletiny_unlock();

    return 0;
}

static int
cmd_show_rssi(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    rc = bletiny_show_rssi(conn_handle);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
cmd_show_svc(int argc, char **argv)
{
    struct bletiny_conn *conn;
    struct bletiny_svc *svc;
    int i;

    bletiny_lock();

    for (i = 0; i < bletiny_num_conns; i++) {
        conn = bletiny_conns + i;

        BLETINY_LOG(INFO, "CONNECTION: handle=%d addr=", conn->handle);
        print_addr(conn->addr);
        BLETINY_LOG(INFO, "\n");

        SLIST_FOREACH(svc, &conn->svcs, next) {
            cmd_print_svc(svc, 0);
        }
    }

    bletiny_unlock();

    return 0;
}

static struct cmd_entry cmd_show_entries[] = {
    { "addr", cmd_show_addr },
    { "chr", cmd_show_chr },
    { "conn", cmd_show_conn },
    { "rssi", cmd_show_rssi },
    { "svc", cmd_show_svc },
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
 * $set                                                                      *
 *****************************************************************************/

#define CMD_ADV_DATA_MAX_UUIDS16                8
#define CMD_ADV_DATA_MAX_UUIDS32                8
#define CMD_ADV_DATA_MAX_UUIDS128               8
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

    rc = bletiny_set_adv_data(&adv_fields);
    if (rc != 0) {
        BLETINY_LOG(INFO, "error setting advertisement data; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static int
cmd_set(int argc, char **argv)
{
    uint16_t mtu;
    uint8_t addr[6];
    int good;
    int rc;

    if (argc > 1 && strcmp(argv[1], "adv_data") == 0) {
        rc = cmd_set_adv_data();
        return rc;
    }

    good = 0;

    rc = parse_arg_mac("addr", addr);
    if (rc == 0) {
        good = 1;
        memcpy(g_dev_addr, addr, 6);
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

    if (!good) {
        BLETINY_LOG(ERROR, "Error: no valid settings specified\n");
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
        BLETINY_LOG(INFO, "error terminating connection; rc=%d\n", rc);
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
        BLETINY_LOG(INFO, "error updating connection; rc=%d\n", rc);
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
        BLETINY_LOG(INFO, "error writing characteristic; rc=%d\n", rc);
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
    { "disc",       cmd_disc },
    { "find",       cmd_find },
    { "l2cap",      cmd_l2cap },
    { "mtu",        cmd_mtu },
    { "read",       cmd_read },
    { "scan",       cmd_scan },
    { "show",       cmd_show },
    { "set",        cmd_set },
    { "term",       cmd_term },
    { "update",     cmd_update },
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
        BLETINY_LOG(ERROR, "error\n");
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
