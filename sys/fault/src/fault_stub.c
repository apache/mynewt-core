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

#include "os/mynewt.h"

#if MYNEWT_VAL(FAULT_STUB)

#include "fault/fault.h"

void
fault_configure_cb(fault_thresh_fn *cb)
{ }

int
fault_process(struct fault_recorder *recorder, bool is_failure)
{
    return 0;
}

int
fault_success(struct fault_recorder *recorder)
{
    return 0;
}

int
fault_failure(struct fault_recorder *recorder)
{
    return 0;
}

void
fault_fatal(int domain_id, void *arg, bool is_failure)
{ }

void
fault_fatal_success(int domain_id, void *arg)
{ }

void
fault_fatal_failure(int domain_id, void *arg)
{ }

int
fault_get_chronic_count(int domain_id, uint8_t *out_count)
{
    *out_count = 0;
    return 0;
}

int
fault_set_chronic_count(int domain_id, uint8_t count)
{
    return 0;
}

int
fault_recorder_init(struct fault_recorder *recorder,
                    int domain_id,
                    uint16_t warn_thresh,
                    uint16_t error_thresh,
                    void *arg)
{
    return 0;
}

const char *
fault_domain_name(int domain_id)
{
    return NULL;
}

int
fault_register_domain_priv(int domain_id, uint8_t success_delta,
                           uint8_t failure_delta, const char *name)
{
    return 0;
}

void
fault_init(void)
{ }

#endif /* MYNEWT_VAL(FAULT_STUB) */
