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

#ifndef H_MODLOG_TEST_UTIL_
#define H_MODLOG_TEST_UTIL_

#define MLTU_LOG_ENTRY_MAX_LEN      256
#define MLTU_LOG_ARG_MAX_ENTRIES    32

struct mltu_log_entry {
    struct log_entry_hdr hdr;
    int len;
    uint8_t body[MLTU_LOG_ENTRY_MAX_LEN];
};

struct mltu_log_arg {
    struct mltu_log_entry entries[MLTU_LOG_ARG_MAX_ENTRIES];
    int num_entries;
};

void mltu_register_log(struct log *lg, struct mltu_log_arg *arg,
                       const char *name, uint8_t level);
void mltu_append(uint8_t module, uint8_t level, uint8_t etype,
                 void *data, int len, bool mbuf);

#endif
