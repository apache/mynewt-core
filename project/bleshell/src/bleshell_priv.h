#ifndef H_BLESHELL_PRIV_
#define H_BLESHELL_PRIV_

#include <inttypes.h>
#include "os/queue.h"

#include "host/ble_gatt.h"

#define BLESHELL_MAX_CONNS              8

typedef int cmd_fn(int argc, char **argv);
struct cmd_entry {
    char *name;
    cmd_fn *cb;
};

struct kv_pair {
    char *key;
    int val;
};

struct bleshell_chr {
    STAILQ_ENTRY(bleshell_chr) next;
    struct ble_gatt_chr chr;
};

STAILQ_HEAD(bleshell_chr_list, bleshell_chr);

struct bleshell_svc {
    STAILQ_ENTRY(bleshell_svc) next;
    struct ble_gatt_service svc;

    struct bleshell_chr_list chrs;
};

STAILQ_HEAD(bleshell_svc_list, bleshell_svc);

struct bleshell_conn {
    uint16_t handle;
    uint8_t addr_type;
    uint8_t addr[6];

    struct bleshell_svc_list svcs;
};

extern struct bleshell_conn bleshell_conns[BLESHELL_MAX_CONNS];
extern int bleshell_num_conns;

void print_addr(void *addr);
void print_uuid(void *uuid128);
struct cmd_entry *parse_cmd_find(struct cmd_entry *cmds, char *name);
struct kv_pair *parse_kv_find(struct kv_pair *kvs, char *name);
char *parse_arg_find(char *key);
int parse_arg_int_bounds(char *name, int min, int max);
int parse_arg_int(char *name);
uint16_t parse_arg_uint16(char *name);
int parse_arg_kv(char *name, struct kv_pair *kvs);
int parse_arg_mac(char *name, uint8_t *dst);
int parse_err_too_few_args(char *cmd_name);
int parse_arg_all(int argc, char **argv);
int cmd_init(void);
void periph_init(void);
int bleshell_disc_svcs(uint16_t conn_handle);
int bleshell_disc_all_chrs(uint16_t conn_handle, uint16_t start_handle,
                           uint16_t end_handle);
int bleshell_adv_start(int disc, int conn, uint8_t *peer_addr, int addr_type);
int bleshell_adv_stop(void);
int bleshell_conn_initiate(int addr_type, uint8_t *peer_addr);

#endif
