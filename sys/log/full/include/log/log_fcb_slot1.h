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

/*
 * Argument for log_fcb_slot1_handler
 *
 * log_fcb_slot1_init() shall be used to initialize this structure.
 */
struct log_fcb_slot1 {
    struct os_mutex mutex;

    struct log *l_current;
    struct log l_fcb;
    struct log l_cbmem;

    log_fcb_slot1_reinit_fcb_fn fcb_reinit_fn;
};

/*
 * Initialize log data for log_fcb_slot1 handler
 *
 * fcb_arg and cbmem_arg are the same as for log_fcb and log_cbmem respectively.
 * Both FCB and CBMEM handlers are switched internally depending on slot1 lock
 * state. If no cbmem_arg is given, logging when slot1 is locked will return an
 * error.
 *
 * Each time slot1 is unlocked, fcb_reinit_fn callback is called and FCB should
 * be reinitialized to allow proper logging there. This callback will be also
 * called by log_fcb_slot1_init() function thus it is not required to initialize
 * FCB prior to calling this function.
 *
 * @param s1             Log data structure to initialize
 * @param fcb_arg        Log data for log_fcb
 * @param cbmem_arg      Log data for log_cbmem
 * @param fcb_reinit_fn  Callback called to reinitialize FCB
 *
 */
int log_fcb_slot1_init(struct log_fcb_slot1 *s1, struct fcb_log *fcb_arg,
                       struct cbmem *cbmem_arg,
                       log_fcb_slot1_reinit_fcb_fn fcb_reinit_fn);

/*
 * Lock logging to slot1
 *
 * This will switch internal log handler to CBMEM (if available) instead of FCB.
 * The existing data in FCB are not touched and should be read by application
 * prior to locking slot1 if necessary.
 */
void log_fcb_slot1_lock(void);

/*
 * Unlock logging to slot1
 *
 * This will switch internal log handler to FCB. A fcb_reinit_fn callback will
 * be called to let application reinitialize FCB. If CBMEM is available, all
 * entries will be copied to FCB automatically.
 */
void log_fcb_slot1_unlock(void);

/*
 * Check slot1 lock state
 *
 * @return  slot1 lock state
 */
bool log_fcb_slot1_is_locked(void);

#ifdef __cplusplus
}
#endif

#endif

#endif /* __SYS_LOG_FCB_SLOT1_H__ */
