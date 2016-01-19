#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "console/console.h"
#include "host/ble_uuid.h"
#include "bleshell_priv.h"

#define CMD_MAX_ARG_LEN     32
#define CMD_MAX_ARGS        16

static char cmd_args[CMD_MAX_ARGS][2][CMD_MAX_ARG_LEN];
static int cmd_num_args;

void
print_addr(void *addr)
{
    uint8_t *u8p;

    u8p = addr;
    console_printf("%02x:%02x:%02x:%02x:%02x:%02x",
                   u8p[0], u8p[1], u8p[2], u8p[3], u8p[4], u8p[5]);
}

void
print_uuid(void *uuid128)
{
    uint16_t uuid16;
    uint8_t *u8p;

    uuid16 = ble_uuid_128_to_16(uuid128);
    if (uuid16 != 0) {
        console_printf("0x%04x", uuid16);
        return;
    }

    u8p = uuid128;

    /* 00001101-0000-1000-8000-00805f9b34fb */
    console_printf("%02x%02x%02x%02x-", u8p[15], u8p[14], u8p[13], u8p[12]);
    console_printf("%02x%02x-%02x%02x-", u8p[11], u8p[10], u8p[9], u8p[8]);
    console_printf("%02x%02x%02x%02x%02x%02x%02x%02x",
                   u8p[7], u8p[6], u8p[5], u8p[4],
                   u8p[3], u8p[2], u8p[1], u8p[0]);
}

int
parse_err_too_few_args(char *cmd_name)
{
    console_printf("Error: too few arguments for command \"%s\"\n", cmd_name);
    return -1;
}

struct cmd_entry *
parse_cmd_find(struct cmd_entry *cmds, char *name)
{
    struct cmd_entry *cmd;
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

char *
parse_arg_find(char *key)
{
    int i;

    for (i = 0; i < cmd_num_args; i++) {
        if (strcmp(cmd_args[i][0], key) == 0) {
            return cmd_args[i][1];
        }
    }

    return NULL;
}

int
parse_arg_int(char *name)
{
    char *endptr;
    char *sval;
    long lval;

    sval = parse_arg_find(name);
    if (sval == NULL) {
        return -1;
    }

    lval = strtol(sval, &endptr, 0);
    if (sval[0] != '\0' && *endptr == '\0' &&
        lval >= INT_MIN && lval <= INT_MAX) {

        return lval;
    }

    return -1;
}

int
parse_arg_kv(char *name, struct kv_pair *kvs)
{
    struct kv_pair *kv;
    char *sval;

    sval = parse_arg_find(name);
    if (sval == NULL) {
        return -1;
    }

    kv = parse_kv_find(kvs, sval);
    if (kv == NULL) {
        return -1;
    }

    return kv->val;
}

int
parse_arg_mac(char *name, uint8_t *dst)
{
    unsigned long ul;
    char *endptr;
    char *token;
    char *sval;
    int i;

    sval = parse_arg_find(name);
    if (sval == NULL) {
        return -1;
    }

    token = strtok(sval, ":");
    for (i = 0; i < 6; i++) {
        if (token == NULL) {
            return -1;
        }

        ul = strtoul(token, &endptr, 16);
        if (sval[0] == '\0' || *endptr != '\0' || ul > UINT8_MAX) {
            return -1;
        }

        dst[i] = ul;

        token = strtok(NULL, ":");
    }

    if (token != NULL) {
        return -1;
    }

    return 0;
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
            if (strlen(key) == 0 || strlen(key) >= CMD_MAX_ARG_LEN ||
                strlen(val) >= CMD_MAX_ARG_LEN) {

                console_printf("Error: invalid argument: %s\n", argv[i]);
                return -1;
            }

            if (cmd_num_args >= CMD_MAX_ARGS) {
                console_printf("Error: too many arguments");
                return -1;
            }

            strcpy(cmd_args[cmd_num_args][0], key);
            strcpy(cmd_args[cmd_num_args][1], val);
            cmd_num_args++;
        }
    }

    return 0;
}

