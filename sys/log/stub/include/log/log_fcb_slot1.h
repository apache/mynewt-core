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

#ifndef __SYS_LOG_FCB_SLOT1_H__
#define __SYS_LOG_FCB_SLOT1_H__

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(LOG_FCB_SLOT1)

#include "log/log.h"
#include "fcb/fcb.h"
#include "cbmem/cbmem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (* log_fcb_slot1_reinit_fcb_fn) (struct fcb_log *arg);

struct log_fcb_slot1 {
};

static inline int
log_fcb_slot1_init(struct log_fcb_slot1 *s1, struct fcb_log *fcb_arg,
                   struct cbmem *cbmem_arg,
                   log_fcb_slot1_reinit_fcb_fn fcb_reinit_fn)
{
    return 0;
}

static inline void
log_fcb_slot1_lock(void)
{
}

static inline void
log_fcb_slot1_unlock(void)
{
}

static inline bool
log_fcb_slot1_is_locked(void)
{
    return false;
}

#ifdef __cplusplus
}
#endif

#endif

#endif /* __SYS_LOG_FCB_SLOT1_H__ */
