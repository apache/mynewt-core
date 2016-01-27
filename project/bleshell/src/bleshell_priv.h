#ifndef H_BLESHELL_PRIV_
#define H_BLESHELL_PRIV_

#include <inttypes.h>
#include "os/queue.h"

#include "host/ble_gatt.h"
struct ble_gap_white_entry;
struct ble_hs_adv_fields;
struct ble_gap_conn_upd_params;
struct ble_gap_conn_crt_params;
struct hci_adv_params;

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

struct bleshell_dsc {
    SLIST_ENTRY(bleshell_dsc) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(bleshell_dsc_list, bleshell_dsc);

struct bleshell_chr {
    SLIST_ENTRY(bleshell_chr) next;
    struct ble_gatt_chr chr;

    struct bleshell_dsc_list dscs;
};
SLIST_HEAD(bleshell_chr_list, bleshell_chr);

struct bleshell_svc {
    SLIST_ENTRY(bleshell_svc) next;
    struct ble_gatt_service svc;

    struct bleshell_chr_list chrs;
};

SLIST_HEAD(bleshell_svc_list, bleshell_svc);

struct bleshell_conn {
    uint16_t handle;
    uint8_t addr_type;
    uint8_t addr[6];

    struct bleshell_svc_list svcs;
};

extern struct bleshell_conn bleshell_conns[BLESHELL_MAX_CONNS];
extern int bleshell_num_conns;

extern const char *bleshell_device_name;
extern const uint16_t bleshell_appearance;
extern const uint8_t bleshell_privacy_flag;
extern uint8_t bleshell_reconnect_addr[6];
extern uint8_t bleshell_pref_conn_params[8];
uint8_t bleshell_gatt_service_changed[4];

void bleshell_printf(const char *fmt, ...);
void print_addr(void *addr);
void print_uuid(void *uuid128);
struct cmd_entry *parse_cmd_find(struct cmd_entry *cmds, char *name);
struct kv_pair *parse_kv_find(struct kv_pair *kvs, char *name);
char *parse_arg_find(char *key);
long parse_arg_long_bounds(char *name, long min, long max, int *out_status);
long parse_arg_long(char *name, int *staus);
uint16_t parse_arg_uint16(char *name, int *status);
uint16_t parse_arg_uint16_dflt(char *name, uint16_t dflt, int *out_status);
uint32_t parse_arg_uint32(char *name, int *out_status);
int parse_arg_kv(char *name, struct kv_pair *kvs);
int parse_arg_byte_stream(char *name, int max_len, uint8_t *dst, int *out_len);
int parse_arg_byte_stream_exact_length(char *name, uint8_t *dst, int len);
int parse_arg_mac(char *name, uint8_t *dst);
int parse_arg_uuid(char *name, uint8_t *dst_uuid128);
int parse_err_too_few_args(char *cmd_name);
int parse_arg_all(int argc, char **argv);
int cmd_init(void);
void periph_init(void);
void bleshell_lock(void);
void bleshell_unlock(void);
int bleshell_exchange_mtu(uint16_t conn_handle);
int bleshell_disc_svcs(uint16_t conn_handle);
int bleshell_disc_svc_by_uuid(uint16_t conn_handle, uint8_t *uuid128);
int bleshell_disc_all_chrs(uint16_t conn_handle, uint16_t start_handle,
                           uint16_t end_handle);
int bleshell_disc_chrs_by_uuid(uint16_t conn_handle, uint16_t start_handle,
                               uint16_t end_handle, uint8_t *uuid128);
int bleshell_disc_all_dscs(uint16_t conn_handle, uint16_t chr_val_handle,
                           uint16_t chr_end_handle);
int bleshell_find_inc_svcs(uint16_t conn_handle, uint16_t start_handle,
                           uint16_t end_handle);
int bleshell_read(uint16_t conn_handle, uint16_t attr_handle);
int bleshell_read_long(uint16_t conn_handle, uint16_t attr_handle);
int bleshell_read_by_uuid(uint16_t conn_handle, uint16_t start_handle,
                          uint16_t end_handle, uint8_t *uuid128);
int bleshell_read_mult(uint16_t conn_handle, uint16_t *attr_handles,
                       int num_attr_handles);
int bleshell_write(uint16_t conn_handle, uint16_t attr_handle, void *value,
                   uint16_t value_len);
int bleshell_write_no_rsp(uint16_t conn_handle, uint16_t attr_handle,
                          void *value, uint16_t value_len);
int bleshell_write_long(uint16_t conn_handle, uint16_t attr_handle,
                        void *value, uint16_t value_len);
int bleshell_write_reliable(uint16_t conn_handle, struct ble_gatt_attr *attrs,
                            int num_attrs);
int bleshell_adv_start(int disc, int conn, uint8_t *peer_addr, int addr_type,
                       struct hci_adv_params *params);
int bleshell_adv_stop(void);
int bleshell_conn_initiate(int addr_type, uint8_t *peer_addr,
                           struct ble_gap_conn_crt_params *params);
int bleshell_conn_cancel(void);
int bleshell_term_conn(uint16_t conn_handle);
int bleshell_wl_set(struct ble_gap_white_entry *white_list,
                    int white_list_count);
int bleshell_scan(uint32_t dur_ms, uint8_t disc_mode, uint8_t scan_type,
                  uint8_t filter_policy);
int bleshell_set_adv_data(struct ble_hs_adv_fields *adv_fields);
int bleshell_update_conn(uint16_t conn_handle,
                         struct ble_gap_conn_upd_params *params);
void bleshell_chrup(uint16_t attr_handle);

#endif
