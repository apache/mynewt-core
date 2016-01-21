#include <errno.h>
#include <string.h>
#include "console/console.h"
#include "shell/shell.h"

#include "nimble/ble.h"
#include "nimble/hci_common.h"
#include "host/ble_gap.h"

#include "bleshell_priv.h"

static struct shell_cmd cmd_b;

/*****************************************************************************
 * $misc                                                                     *
 *****************************************************************************/

static int
cmd_exec(struct cmd_entry *cmds, int argc, char **argv)
{
    struct cmd_entry *cmd;
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
cmd_print_dsc(struct bleshell_dsc *dsc)
{
    console_printf("            dsc_handle=%d uuid=", dsc->dsc.handle);
    print_uuid(dsc->dsc.uuid128);
    console_printf("\n");
}

static void
cmd_print_chr(struct bleshell_chr *chr)
{
    struct bleshell_dsc *dsc;

    console_printf("        def_handle=%d val_handle=%d properties=0x%02x "
                   "uuid=",
                   chr->chr.decl_handle, chr->chr.value_handle,
                   chr->chr.properties);
    print_uuid(chr->chr.uuid128);
    console_printf("\n");

    SLIST_FOREACH(dsc, &chr->dscs, next) {
        cmd_print_dsc(dsc);
    }
}

static void
cmd_print_svc(struct bleshell_svc *svc, int print_chrs)
{
    struct bleshell_chr *chr;

    console_printf("    start=%d end=%d uuid=", svc->svc.start_handle,
                   svc->svc.end_handle);
    print_uuid(svc->svc.uuid128);
    console_printf("\n");

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

static int
cmd_adv(int argc, char **argv)
{
    uint8_t peer_addr[6];
    int addr_type;
    int conn;
    int disc;
    int rc;

    if (argc > 1 && strcmp(argv[1], "stop") == 0) {
        rc = bleshell_adv_stop();
        if (rc != 0) {
            console_printf("advertise stop fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    conn = parse_arg_kv("conn", cmd_adv_conn_modes);
    if (conn == -1) {
        console_printf("invalid 'conn' parameter\n");
        return -1;
    }
    

    disc = parse_arg_kv("disc", cmd_adv_disc_modes);
    if (disc == -1) {
        console_printf("missing 'disc' parameter\n");
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

    rc = bleshell_adv_start(disc, conn, peer_addr, addr_type);
    if (rc != 0) {
        console_printf("advertise fail: %d\n", rc);
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
    uint8_t peer_addr[6];
    int addr_type;
    int rc;

    if (argc > 1 && strcmp(argv[1], "cancel") == 0) {
        rc = bleshell_conn_cancel();
        if (rc != 0) {
            console_printf("connection cancel fail: %d\n", rc);
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

    rc = bleshell_conn_initiate(addr_type, peer_addr);
    if (rc != 0) {
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
        rc = bleshell_disc_chrs_by_uuid(conn_handle, start_handle, end_handle,
                                        uuid128);
    } else if (rc == ENOENT) {
        rc = bleshell_disc_all_chrs(conn_handle, start_handle, end_handle);
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

    rc = bleshell_disc_all_dscs(conn_handle, start_handle, end_handle);
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
        rc = bleshell_disc_svc_by_uuid(conn_handle, uuid128);
    } else if (rc == ENOENT) {
        rc = bleshell_disc_svcs(conn_handle);
    } else  {
        return rc;
    }

    if (rc != 0) {
        console_printf("error discovering services; rc=%d\n", rc);
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

    rc = bleshell_find_inc_svcs(conn_handle, start_handle, end_handle);
    if (rc != 0) {
        console_printf("error finding included services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static struct cmd_entry cmd_find_entries[] = {
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

    rc = bleshell_exchange_mtu(conn_handle);
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
    uint16_t attr_handles[CMD_READ_MAX_ATTRS];
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
            rc = bleshell_read_long(conn_handle, attr_handles[0]);
        } else {
            rc = bleshell_read(conn_handle, attr_handles[0]);
        }
    } else if (num_attr_handles > 1) {
        rc = bleshell_read_mult(conn_handle, attr_handles, num_attr_handles);
    } else if (is_uuid) {
        if (start == 0 || end == 0) {
            rc = EINVAL;
        } else {
            rc = bleshell_read_by_uuid(conn_handle, start, end, uuid128);
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
 * $show                                                                     *
 *****************************************************************************/

static int
cmd_show_addr(int argc, char **argv)
{
    console_printf("myaddr=");
    print_addr(g_dev_addr);
    console_printf("\n");

    return 0;
}

static int
cmd_show_chr(int argc, char **argv)
{
    struct bleshell_conn *conn;
    struct bleshell_svc *svc;
    int i;

    for (i = 0; i < bleshell_num_conns; i++) {
        conn = bleshell_conns + i;

        console_printf("CONNECTION: handle=%d addr=", conn->handle);
        print_addr(conn->addr);
        console_printf("\n");

        SLIST_FOREACH(svc, &conn->svcs, next) {
            cmd_print_svc(svc, 1);
        }
    }

    return 0;
}

static int
cmd_show_conn(int argc, char **argv)
{
    struct bleshell_conn *conn;
    int i;

    for (i = 0; i < bleshell_num_conns; i++) {
        conn = bleshell_conns + i;

        console_printf("handle=%d addr=", conn->handle);
        print_addr(conn->addr);
        console_printf(" addr_type=%d\n", conn->addr_type);
    }

    return 0;
}

static int
cmd_show_svc(int argc, char **argv)
{
    struct bleshell_conn *conn;
    struct bleshell_svc *svc;
    int i;

    for (i = 0; i < bleshell_num_conns; i++) {
        conn = bleshell_conns + i;

        console_printf("CONNECTION: handle=%d addr=", conn->handle);
        print_addr(conn->addr);
        console_printf("\n");

        SLIST_FOREACH(svc, &conn->svcs, next) {
            cmd_print_svc(svc, 0);
        }
    }

    return 0;
}

static struct cmd_entry cmd_show_entries[] = {
    { "addr", cmd_show_addr },
    { "chr", cmd_show_chr },
    { "conn", cmd_show_conn },
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

static int
cmd_set(int argc, char **argv)
{
    uint16_t mtu;
    uint8_t addr[6];
    int good;
    int rc;

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
    }
    if (rc != ENOENT) {
        return rc;
    }

    if (!good) {
        console_printf("Error: no valid settings specified\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * $write                                                                    *
 *****************************************************************************/

static int
cmd_write(int argc, char **argv)
{
    static uint8_t attr_val[BLE_ATT_ATTR_MAX_LEN];
    static int attr_len;

    uint16_t attr_handle;
    uint16_t conn_handle;
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

    attr_handle = parse_arg_long("attr", &rc);
    if (rc != 0) {
        return rc;
    }

    rc = parse_arg_byte_stream("value", sizeof attr_val, attr_val, &attr_len);
    if (rc != 0) {
        return rc;
    }

    if (no_rsp) {
        rc = bleshell_write_no_rsp(conn_handle, attr_handle, attr_val,
                                   attr_len);
    } else if (is_long) {
        rc = bleshell_write_long(conn_handle, attr_handle, attr_val, attr_len);
    } else {
        rc = bleshell_write(conn_handle, attr_handle, attr_val, attr_len);
    }
    if (rc != 0) {
        console_printf("error writing characteristic; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $init                                                                     *
 *****************************************************************************/

static struct cmd_entry cmd_b_entries[] = {
    { "adv", cmd_adv },
    { "conn", cmd_conn },
    { "disc", cmd_disc },
    { "find", cmd_find },
    { "mtu", cmd_mtu },
    { "read", cmd_read },
    { "show", cmd_show },
    { "set", cmd_set },
    { "write", cmd_write },
    { NULL, NULL }
};

static int
cmd_b_exec(int argc, char **argv)
{
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    rc = cmd_exec(cmd_b_entries, argc, argv);
    if (rc != 0) {
        console_printf("error\n");
        return rc;
    }

    return 0;
}

int
cmd_init(void)
{
    int rc;

    rc = shell_cmd_register(&cmd_b, "b", cmd_b_exec);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
