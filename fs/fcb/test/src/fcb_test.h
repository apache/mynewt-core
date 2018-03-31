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
#ifndef _FCB_TEST_H
#define _FCB_TEST_H

#include <stdio.h>
#include <string.h>

#include "os/mynewt.h"
#include "testutil/testutil.h"

#include "fcb/fcb.h"
#include "fcb_priv.h"

#ifdef __cplusplus
#extern "C" {
#endif

extern struct fcb test_fcb;

extern struct flash_area test_fcb_area[];

struct append_arg {
    int *elem_cnts;
};

void fcb_test_wipe(void);
int fcb_test_empty_walk_cb(struct fcb_entry *loc, void *arg);
uint8_t fcb_test_append_data(int msg_len, int off);
int fcb_test_data_walk_cb(struct fcb_entry *loc, void *arg);
int fcb_test_cnt_elems_cb(struct fcb_entry *loc, void *arg);

#ifdef __cplusplus
}
#endif
#endif /* _FCB_TEST_H */
