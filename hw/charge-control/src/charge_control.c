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

#include <string.h>

#include "os/os.h"
#include "os/os_dev.h"
#include "defs/error.h"
#include "syscfg/syscfg.h"
#include "sysinit/sysinit.h"
#include "charge-control/charge_control.h"
#include "charge_control_priv.h"

// =================================================================
// ====================== LOCAL FUNCTION DECLARATIONS ==============
// =================================================================

// ---------------------- OS ---------------------------------------

static void charge_control_read_ev_cb(struct os_event *ev);

static void charge_control_mgr_wakeup_event(struct os_event *);
static void charge_control_base_ts_update_event(struct os_event *);

// ---------------------- CHARGE CONTROL ---------------------------

/**
 *
 */
static void 
charge_control_update_poll_rate(struct charge_control *, uint32_t);

/**
 *
 */
static os_time_t 
charge_control_calc_nextrun_delta(struct charge_control *, os_time_t);

/**
 *
 */
static struct charge_control *
charge_control_find_min_nextrun(os_time_t, os_time_t *);

/**
 *
 */
static void
charge_control_update_nextrun(struct charge_control *, os_time_t);

/**
 *
 */
static int
charge_control_read_data_func(struct charge_control *, void *, void *,
        charge_control_type_t);

/**
 *
 */
static void charge_control_up_timestamp(struct charge_control *);

// ---------------------- CHARGE CONTROL MANAGER -------------------

/**
 *
 */
static void charge_control_mgr_remove(struct charge_control *);

/**
 *
 */
static void charge_control_mgr_insert(struct charge_control *);

/**
 *
 */
static void
charge_control_mgr_poll_bytype(struct charge_control *, charge_control_type_t,
        os_time_t);

/**
 * 
 */
static void charge_control_mgr_evq_set(struct os_eventq *);

/**
 * 
 */
static void charge_control_mgr_init(void);

// =================================================================
// ====================== LOCAL STRUCTS/VARIABLES ==================
// =================================================================

/**
 * Declaration and definition of manager for the list of charge control devices
 */
struct {
    /** Locks list for manipulation/search operations */
    struct os_mutex mgr_lock;

    /** Manages poll rates of the charge control devices */
    struct os_callout mgr_wakeup_callout;

    /** Event queue to which wakeup events are added */
    struct os_eventq *mgr_eventq;

    /** Charge control device list */
    SLIST_HEAD(, charge_control) mgr_charge_control_list;
} charge_control_mgr;

struct charge_control_read_ctx {
    charge_control_data_func_t user_func;
    void *user_arg;
};

struct charge_control_timestamp charge_control_base_ts;

/** OS Callout to update charge control timestamp */
struct os_callout cct_up_osco;

/** Event for performing a charge control read */
static struct os_event charge_control_read_event = {
    .ev_cb = charge_control_read_ev_cb,
};

// =================================================================
// ====================== PKG ======================================
// =================================================================

void charge_control_pkg_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    charge_control_mgr_init();

#if MYNEWT_VAL(CHARGE_CONTROL_CLI)
    charge_control_shell_register();
#endif
}

// =================================================================
// ====================== OS =======================================
// =================================================================

static void charge_control_read_ev_cb(struct os_event *ev)
{
    int rc;
    struct charge_control_read_ev_ctx *ccrec;

    ccrec = ev->ev_arg;
    rc = charge_control_read(ccrec->ccrec_charge_control, ccrec->ccrec_type, 
            NULL, NULL, OS_TIMEOUT_NEVER);
    assert(rc == 0);
}

static void charge_control_mgr_wakeup_event(struct os_event *ev)
{
    struct charge_control *cursor;
    os_time_t now;
    os_time_t next_wakeup;

    now = os_time_get();

#if MYNEWT_VAL(SENSOR_POLL_TEST_LOG)
    ccmgr_wakeup[ccmgr_wakeup_idx++%500] = now;
#endif

    charge_control_mgr_lock();

    cursor = NULL;
    while (1) {

        cursor = charge_control_find_min_nextrun(now, &next_wakeup);

        charge_control_lock(cursor);
        /* Charge controllers that are not periodic are inserted at the end of 
         * the charge_control list.
         */
        if (!cursor->cc_poll_rate) {
            charge_control_unlock(cursor);
            charge_control_mgr_unlock();
            return;
        }

        /* List is sorted by what runs first.  If we reached the first element 
         * that doesn't run, break out.
         */
        if (next_wakeup > 0) {
            break;
        }

        charge_control_mgr_poll_bytype(cursor, cursor->cc_mask, now);

        charge_control_update_nextrun(cursor, now);

        charge_control_unlock(cursor);
    }

    charge_control_mgr_unlock();

    os_callout_reset(&charge_control_mgr.mgr_wakeup_callout, next_wakeup);
}

static void charge_control_base_ts_update_event(struct os_event *ev)
{
    os_time_t ticks;
    int rc;
    struct os_timeval ostv;
    struct os_timezone ostz;

    ticks = os_time_get();

    rc = os_gettimeofday(&ostv, &ostz);
    if (rc) {
        /* There is nothing we can do here
         * just reset the timer frequently if we
         * fail to get time, till then we will keep using
         * old timestamp values.
         */
        ticks += OS_TICKS_PER_SEC * 600;
        goto done;
    }

    /* CPU time gets wrapped in 4295 seconds since it is uint32_t,
     * hence the hardcoded value of 3600 seconds, We want to make
     * sure that the cputime never gets wrapped more than once.
     * os_timeval usecs value gets wrapped in 2147 secs since it is int32_t.
     * Hence, we take 2000 secs so that we update before it gets wrapped
     * without cutting it too close.
     */
    ticks += OS_TICKS_PER_SEC * 2000;

    charge_control_base_ts.cct_ostv = ostv;
    charge_control_base_ts.cct_ostz = ostz;
    charge_control_base_ts.cct_cputime = os_cputime_get32();

done:
    os_callout_reset(&cct_up_osco, ticks);
}

// =================================================================
// ====================== CHARGE CONTROL ===========================
// =================================================================

static void 
charge_control_update_poll_rate(struct charge_control * cc, uint32_t poll_rate)
{
    charge_control_lock(cc);

    cc->cc_poll_rate = poll_rate;

    charge_control_unlock(cc);
}

static os_time_t 
charge_control_calc_nextrun_delta(struct charge_control * cc, os_time_t now)
{
    os_time_t charge_control_ticks;
    int delta;

    charge_control_lock(cc);

    delta = (int32_t)(cc->cc_next_run - now);
    if (delta < 0) {
        /* This fires the callout right away */
        charge_control_ticks = 0;
    } else {
        charge_control_ticks = delta;
    }

    charge_control_unlock(cc);

    return charge_control_ticks;
}

static struct charge_control *
charge_control_find_min_nextrun(os_time_t now, os_time_t *min_nextrun)
{
    struct charge_control *head;

    head = NULL;

    charge_control_mgr_lock();

    head = SLIST_FIRST(&charge_control_mgr.mgr_charge_control_list);

    *min_nextrun = charge_control_calc_nextrun_delta(head, now);

    charge_control_mgr_unlock();

    return head;
}

static void
charge_control_update_nextrun(struct charge_control *cc, os_time_t now)
{
    os_time_t charge_control_ticks;

    os_time_ms_to_ticks(cc->cc_poll_rate, &charge_control_ticks);

    charge_control_lock(cc);

    /* Remove the charge_control from the charge_control list for insert. */
    charge_control_mgr_remove(cc);

    /* Set next wakeup, and insert the charge_control back into the
     * list.
     */
    cc->cc_next_run = charge_control_ticks + now;

    /* Re-insert the charge_control manager, with the new wakeup time. */
    charge_control_mgr_insert(cc);

    charge_control_unlock(cc);
}

static int
charge_control_read_data_func(struct charge_control *cc, void *arg, 
        void *data, charge_control_type_t type)
{
    struct charge_control_listener *listener;
    struct charge_control_read_ctx *ctx;

    ctx = (struct charge_control_read_ctx *) arg;

    if ((uint8_t)(uintptr_t)(ctx->user_arg) != CHARGE_CONTROL_IGN_LISTENER) {
        /* Notify all listeners first */
        SLIST_FOREACH(listener, &cc->cc_listener_list, ccl_next) {
            if (listener->ccl_type & type) {
                listener->ccl_func(cc, listener->ccl_arg, data, type);
            }
        }
    }

    /* Call data function */
    if (ctx->user_func != NULL) {
        return (ctx->user_func(cc, ctx->user_arg, data, type));
    } else {
        return (0);
    }
}

static void charge_control_up_timestamp(struct charge_control *cc)
{
    uint32_t curr_ts_ticks;
    uint32_t ts;

    curr_ts_ticks = os_cputime_get32();

    ts = os_cputime_ticks_to_usecs(curr_ts_ticks -
             charge_control_base_ts.cct_cputime);

    /* Updating cputime */
    charge_control_base_ts.cct_cputime = cc->cc_sts.cct_cputime = curr_ts_ticks;

    /* Updating seconds */
    charge_control_base_ts.cct_ostv.tv_sec  = 
        charge_control_base_ts.cct_ostv.tv_sec 
        + (ts + charge_control_base_ts.cct_ostv.tv_usec)/1000000;
    cc->cc_sts.cct_ostv.tv_sec = charge_control_base_ts.cct_ostv.tv_sec;

    /* Updating Micro seconds */
    charge_control_base_ts.cct_ostv.tv_usec  =
        (charge_control_base_ts.cct_ostv.tv_usec + ts)%1000000;
    cc->cc_sts.cct_ostv.tv_usec = charge_control_base_ts.cct_ostv.tv_usec;
}

int
charge_control_init(struct charge_control *cc, struct os_dev *dev)
{
    int rc;

    memset(cc, 0, sizeof(*cc));

    rc = os_mutex_init(&cc->cc_lock);
    if (rc != 0) {
        goto err;
    }
    cc->cc_dev = dev;

    return (0);
err:
    return (rc);
}

int
charge_control_lock(struct charge_control *cc)
{
    int rc;

    rc = os_mutex_pend(&cc->cc_lock, OS_TIMEOUT_NEVER);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return (rc);
}

void
charge_control_unlock(struct charge_control *cc)
{
    os_mutex_release(&cc->cc_lock);
}

int
charge_control_register_listener(struct charge_control *cc,
        struct charge_control_listener *listener)
{
    int rc;

    rc = charge_control_lock(cc);
    if (rc != 0) {
        goto err;
    }

    SLIST_INSERT_HEAD(&cc->cc_listener_list, listener, ccl_next);

    charge_control_unlock(cc);

    return (0);
err:
    return (rc);
}

int
charge_control_unregister_listener(struct charge_control *cc,
        struct charge_control_listener *listener)
{
    int rc;

    rc = charge_control_lock(cc);
    if (rc != 0) {
        goto err;
    }

    /* Remove this entry from the list */
    SLIST_REMOVE(&cc->cc_listener_list, listener, charge_control_listener,
            ccl_next);

    charge_control_unlock(cc);

    return (0);
err:
    return (rc);
}

int
charge_control_read(struct charge_control *cc, charge_control_type_t type, 
        charge_control_data_func_t data_func, void *arg, uint32_t timeout)
{
    struct charge_control_read_ctx ccrc;
    int rc;

    rc = charge_control_lock(cc);
    if (rc) {
        goto err;
    }

    ccrc.user_func = data_func;
    ccrc.user_arg = arg;

    if (!charge_control_mgr_match_bytype(cc, (void *)&type)) {
        rc = SYS_ENOENT;
        goto err;
    }

    charge_control_up_timestamp(cc);

    rc = cc->cc_funcs->ccd_read(cc, type, charge_control_read_data_func, &ccrc,
            timeout);
    if (rc) {
        goto err;
    }

err:
    charge_control_unlock(cc);
    return (rc);
}

// =================================================================
// ====================== CHARGE CONTROL MANAGER ===================
// =================================================================

static void
charge_control_mgr_remove(struct charge_control *cc)
{
    SLIST_REMOVE(&charge_control_mgr.mgr_charge_control_list, cc, 
            charge_control, cc_next);
}

static void
charge_control_mgr_insert(struct charge_control *cc)
{
    struct charge_control *cursor, *prev;

    prev = cursor = NULL;
    if (!cc->cc_poll_rate) {
        SLIST_FOREACH(cursor, &charge_control_mgr.mgr_charge_control_list, cc_next) {
            prev = cursor;
        }
        goto insert;
    }

    prev = cursor = NULL;
    SLIST_FOREACH(cursor, &charge_control_mgr.mgr_charge_control_list, cc_next) {
        if (!cursor->cc_poll_rate) {
            break;
        }

        if (OS_TIME_TICK_LT(cc->cc_next_run, cursor->cc_next_run)) {
            break;
        }

        prev = cursor;
    }

insert:
    if (prev == NULL) {
        SLIST_INSERT_HEAD(&charge_control_mgr.mgr_charge_control_list, cc, 
                cc_next);
    } else {
        SLIST_INSERT_AFTER(prev, cc, cc_next);
    }
}

static void
charge_control_mgr_poll_bytype(struct charge_control *cc,
        charge_control_type_t type, os_time_t now)
{

}

static void
charge_control_mgr_evq_set(struct os_eventq *evq)
{
    assert(evq != NULL);
    charge_control_mgr.mgr_eventq = evq;
}

static void
charge_control_mgr_init(void)
{
    struct os_timeval ostv;
    struct os_timezone ostz;

#ifdef MYNEWT_VAL_CHARGE_CONTROL_MGR_EVQ
    charge_control_mgr_evq_set(MYNEWT_VAL(CHARGE_CONTROL_MGR_EVQ));
#else
    charge_control_mgr_evq_set(os_eventq_dflt_get());
#endif

    /**
     * Initialize charge control polling callout and set it to fire on boot.
     */
    os_callout_init(&charge_control_mgr.mgr_wakeup_callout, 
            charge_control_mgr_evq_get(), charge_control_mgr_wakeup_event, 
            NULL);

    /* Initialize sensor cputime update callout and set it to fire after an
     * hour, CPU time gets wrapped in 4295 seconds,
     * hence the hardcoded value of 3600 seconds, We make sure that the
     * cputime never gets wrapped more than once.
     */
    os_gettimeofday(&ostv, &ostz);

    charge_control_base_ts.cct_ostv = ostv;
    charge_control_base_ts.cct_ostz = ostz;
    charge_control_base_ts.cct_cputime = os_cputime_get32();

    os_callout_init(&cct_up_osco, charge_control_mgr_evq_get(),
            charge_control_base_ts_update_event, NULL);
    os_callout_reset(&cct_up_osco, OS_TICKS_PER_SEC);

    os_mutex_init(&charge_control_mgr.mgr_lock);
}

int
charge_control_mgr_lock(void)
{
    int rc;

    rc = os_mutex_pend(&charge_control_mgr.mgr_lock, OS_TIMEOUT_NEVER);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return rc;
}

void
charge_control_mgr_unlock(void)
{
    (void) os_mutex_release(&charge_control_mgr.mgr_lock);
}

int
charge_control_mgr_register(struct charge_control *cc)
{
    return 0;
}

struct os_eventq *
charge_control_mgr_evq_get(void)
{
    return charge_control_mgr.mgr_eventq;
}

struct charge_control *
charge_control_mgr_find_next(charge_control_mgr_compare_func_t compare_func, 
        void *arg, struct charge_control *cc)
{
    return NULL;
}

struct charge_control *
charge_control_mgr_find_next_bytype(charge_control_type_t type, 
        struct charge_control *cc)
{
    return NULL;
}

struct charge_control *
charge_control_mgr_find_next_bydevname(char *name, struct charge_control *cc)
{
    return NULL;
}

int
charge_control_mgr_match_bytype(struct charge_control *cc, void *type)
{
    return 0;
}

int
charge_control_set_poll_rate_ms(char *devname, uint32_t poll_rate)
{
    struct charge_control *charge_control;
    os_time_t next_wakeup;
    os_time_t now;
    int rc;

    os_callout_stop(&charge_control_mgr.mgr_wakeup_callout);

    charge_control = charge_control_mgr_find_next_bydevname(devname, NULL);
    if (!charge_control) {
        rc = SYS_EINVAL;
        goto err;
    }

    charge_control_lock(charge_control);

    now = os_time_get();

    os_time_ms_to_ticks(poll_rate, &next_wakeup);

    charge_control_update_poll_rate(charge_control, poll_rate);

    charge_control_update_nextrun(charge_control, now);

    charge_control_unlock(charge_control);

    charge_control = charge_control_find_min_nextrun(now, &next_wakeup);

    os_callout_reset(&charge_control_mgr.mgr_wakeup_callout, next_wakeup);

    return 0;
err:
    return rc;
}

void
charge_control_mgr_put_read_evt(void *arg)
{
    charge_control_read_event.ev_arg = arg;
    os_eventq_put(charge_control_mgr_evq_get(), &charge_control_read_event);
}

#if MYNEWT_VAL(CHARGE_CONTROL_CLI)
char*
charge_control_ftostr(float f, char *c, int k)
{

}
#endif
