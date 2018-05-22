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

#ifndef __CONFIG_PRIV_H_
#define __CONFIG_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

int conf_cli_register(void);
int conf_nmgr_register(void);

struct mgmt_cbuf;
int conf_line_parse(char *buf, char **namep, char **valp);
int conf_line_make(char *dst, int dlen, const char *name, const char *val);
int conf_line_make2(char *dst, int dlen, const char *name, const char *value);
struct conf_handler *conf_parse_and_lookup(char *name, int *name_argc,
                                           char *name_argv[]);

/*
 * API for config storage.
 */
typedef void (*load_cb)(char *name, char *val, void *cb_arg);
struct conf_store_itf {
    int (*csi_load)(struct conf_store *cs, load_cb cb, void *cb_arg);
    int (*csi_save_start)(struct conf_store *cs);
    int (*csi_save)(struct conf_store *cs, const char *name, const char *value);
    int (*csi_save_end)(struct conf_store *cs);
};

void conf_src_register(struct conf_store *cs);
void conf_dst_register(struct conf_store *cs);

SLIST_HEAD(conf_store_head, conf_store);
extern struct conf_store_head conf_load_srcs;
SLIST_HEAD(conf_handler_head, conf_handler);
extern struct conf_handler_head conf_handlers;
extern struct conf_store *conf_save_dst;

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_PRIV_H_ */
