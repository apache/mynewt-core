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
#include <os/mynewt.h>

#include <config/config.h>
#include <config/config_store.h>

#include "conf_mmap/conf_mmap.h"

static int conf_mmap_load(struct conf_store *cs, conf_store_load_cb cb,
                          void *cb_arg);

static struct conf_store_itf conf_mmap_itf = {
    .csi_load = conf_mmap_load,
};

static int
conf_mmap_load(struct conf_store *cs, conf_store_load_cb cb, void *cb_arg)
{
    struct conf_mmap *cm = (struct conf_mmap *)cs;
    const struct conf_mmap_kv *cmk;
    char name[CONF_MAX_NAME_LEN + 1];
    char val[CONF_MAX_VAL_LEN + 1];
    int len;
    int i;

    for (i = 0; i < cm->cm_kv_cnt; i++) {
        cmk = &cm->cm_kv[i];

        strncpy(name, cmk->cmk_key, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        len = cmk->cmk_maxlen;
        if (len > CONF_MAX_VAL_LEN) {
            len = CONF_MAX_VAL_LEN;
        }
        strncpy(val, (void *)(cm->cm_base + cmk->cmk_off), len);
        cb(name, val, cb_arg);
    }
    return OS_OK;
}

int
conf_mmap_src(struct conf_mmap *cm)
{
    /* XXX probably should check for magic number or something where
       cm_base is */
    cm->cm_store.cs_itf = &conf_mmap_itf;
    conf_src_register(&cm->cm_store);

    return OS_OK;
}
