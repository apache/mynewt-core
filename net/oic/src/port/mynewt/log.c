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
#include <log/log.h>

/* logging data for this module. TODO, the application should
 * define the logging strategy for this module */
#define MAX_CBMEM_BUF   (600)
static uint32_t *cbmem_buf;
static struct cbmem cbmem;
struct log oc_log;

int
oc_log_init(void) {

    log_init();

    cbmem_buf = malloc(sizeof(uint32_t) * MAX_CBMEM_BUF);
    if (cbmem_buf == NULL) {
        return -1;
    }

    cbmem_init(&cbmem, cbmem_buf, MAX_CBMEM_BUF);
    log_register("iot", &oc_log, &log_cbmem_handler, &cbmem, LOG_SYSLEVEL);

    LOG_INFO(&oc_log, LOG_MODULE_IOTIVITY, "OC Init");
    return 0;
}
