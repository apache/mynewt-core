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

#include <string.h>
#include <stdio.h>

#include <os/os.h>

#include "config/config.h"
#include "config_priv.h"

SLIST_HEAD(, conf_store) conf_load_srcs =
    SLIST_HEAD_INITIALIZER(&conf_load_srcs);

void
conf_src_register(struct conf_store *cs)
{
    struct conf_store *prev, *cur;

    prev = NULL;
    SLIST_FOREACH(cur, &conf_load_srcs, cs_next) {
        prev = cur;
    }
    if (!prev) {
        SLIST_INSERT_HEAD(&conf_load_srcs, cs, cs_next);
    } else {
        SLIST_INSERT_AFTER(prev, cs, cs_next);
    }
}

static void
conf_load_cb(char *name, char *val, void *cb_arg)
{
    conf_set_value(name, val);
}

int
conf_load(void)
{
    struct conf_store *cs;

    /*
     * for every config store
     *    load config
     *    apply config
     *    commit all
     */

    SLIST_FOREACH(cs, &conf_load_srcs, cs_next) {
        cs->cs_itf->csi_load(cs, conf_load_cb, NULL);
    }
    return 0;
}

