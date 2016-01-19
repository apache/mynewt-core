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
cmd_disc_svc(int argc, char **argv)
{
    int conn_handle;
    int rc;

    conn_handle = parse_arg_int("conn");
    if (conn_handle == -1) {
        return -1;
    }

    rc = bleshell_disc_svcs(conn_handle);
    if (rc != 0) {
        console_printf("error discovering services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static struct cmd_entry cmd_disc_entries[] = {
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

        STAILQ_FOREACH(svc, &conn->svcs, next) {
            console_printf("    start=%d end=%d uuid=", svc->svc.start_handle,
                           svc->svc.end_handle);
            print_uuid(svc->svc.uuid128);
            console_printf("\n");
        }
    }

    return 0;
}

static struct cmd_entry cmd_show_entries[] = {
    { "addr", cmd_show_addr },
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
    uint8_t addr[6];
    int good;
    int rc;

    good = 0;

    rc = parse_arg_mac("addr", addr);
    if (rc == 0) {
        good = 1;
        memcpy(g_dev_addr, addr, 6);
    }

    if (!good) {
        console_printf("Error: no valid settings specified\n");
        return -1;
    }

    return 0;
}

static struct cmd_entry cmd_b_entries[] = {
    { "adv", cmd_adv },
    { "conn", cmd_conn },
    { "disc", cmd_disc },
    { "show", cmd_show },
    { "set", cmd_set },
    { NULL, NULL }
};

/*****************************************************************************
 * $init                                                                     *
 *****************************************************************************/

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
