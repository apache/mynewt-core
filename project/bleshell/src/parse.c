#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include "console/console.h"
#include "host/ble_uuid.h"
#include "bleshell_priv.h"

#define CMD_MAX_ARGS        16

static char *cmd_args[CMD_MAX_ARGS][2];
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
            /* Erase parameter. */
            cmd_args[i][0][0] = '\0';

            return cmd_args[i][1];
        }
    }

    return NULL;
}

long
parse_arg_long_bounds(char *name, long min, long max, int *out_status)
{
    char *endptr;
    char *sval;
    long lval;

    sval = parse_arg_find(name);
    if (sval == NULL) {
        *out_status = ENOENT;
        return 0;
    }

    lval = strtol(sval, &endptr, 0);
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

uint16_t
parse_arg_uint16(char *name, int *out_status)
{
    return parse_arg_long_bounds(name, 0, UINT16_MAX, out_status);
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

int
parse_arg_kv(char *name, struct kv_pair *kvs)
{
    struct kv_pair *kv;
    char *sval;

    sval = parse_arg_find(name);
    if (sval == NULL) {
        return ENOENT;
    }

    kv = parse_kv_find(kvs, sval);
    if (kv == NULL) {
        return EINVAL;
    }

    return kv->val;
}

static int
parse_arg_byte_stream_no_delim(char *sval, int max_len, uint8_t *dst,
                               int *out_len)
{
    unsigned long ul;
    char *endptr;
    char buf[3];
    int i;

    buf[2] = '\0';
    i = 0;
    while (1) {
        if (sval[i * 2] == '\0') {
            *out_len = i;
            return 0;
        }

        if (i >= max_len) {
            return EINVAL;
        }

        buf[0] = sval[i * 2 + 0];
        buf[1] = sval[i * 2 + 1];

        ul = strtoul(buf, &endptr, 16);
        if (*sval == '\0' || *endptr != '\0') {
            return EINVAL;
        }

        assert(ul <= UINT8_MAX);
        dst[i] = ul;

        i++;
    }
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
    int total_len;
    char *sval;

    sval = parse_arg_find(name);
    if (sval == NULL) {
        return ENOENT;
    }

    total_len = strlen(sval);
    if (strcspn(sval, ":-") == total_len) {
        return parse_arg_byte_stream_no_delim(sval, max_len, dst, out_len);
    } else {
        return parse_arg_byte_stream_delim(sval, ":-", max_len, dst, out_len);
    }
}

static int
parse_arg_byte_stream_exact_length(char *name, uint8_t *dst, int len)
{
    int actual_len;
    int rc;

    rc = parse_arg_byte_stream(name, 6, dst, &actual_len);
    if (rc != 0) {
        return rc;
    }

    if (actual_len != len) {
        return EINVAL;
    }

    return 0;
}
int
parse_arg_mac(char *name, uint8_t *dst)
{
    return parse_arg_byte_stream_exact_length(name, dst, 6);
}

int
parse_arg_uuid(char *name, uint8_t *dst_uuid128)
{
    uint16_t uuid16;
    int rc;

    uuid16 = parse_arg_uint16(name, &rc);
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
        rc = parse_arg_byte_stream_exact_length(name, dst_uuid128, 16);
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

