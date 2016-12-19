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

#include <limits.h>
#include "sysinit/sysinit.h"
#include "sysinit_priv.h"

uint8_t sysinit_active;

void
sysinit_init_pkgs(void)
{
    const struct sysinit_entry *entry;
    const struct sysinit_entry *start;
    const struct sysinit_entry *end;
    struct sysinit_init_ctxt ctxt;
    int next_stage;
    int cur_stage;

    sysinit_active = 1;

    /* Determine the start and end of the sysinit linker section. */
    sysinit_section_bounds(&start, &end);

    cur_stage = 0;
    do {
        /* Assume this is the final stage. */
        next_stage = INT_MAX;

        /* Execute all init functions corresponding to the current stage. */
        for (entry = start; entry != end; entry++) {
            if (cur_stage == entry->stage) {
                ctxt.entry = entry;
                ctxt.cur_stage = cur_stage;
                entry->init_fn(&ctxt);
            } else if (cur_stage < entry->stage && next_stage > entry->stage) {
                /* Found a stage to execute next. */
                next_stage = entry->stage;
            }
        }
        cur_stage = next_stage;
    } while (cur_stage != INT_MAX);

    sysinit_active = 0;
}
