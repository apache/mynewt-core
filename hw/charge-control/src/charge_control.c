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

#include "os/mynewt.h"
#include "charge-control/charge_control.h"
#include "charge_control_priv.h"

/* =================================================================
 * ====================== LOCAL FUNCTION DECLARATIONS ==============
 * =================================================================
 */

/* ---------------------------- OS -------------------------------- */

static void charge_control_read_ev_cb(struct os_event *ev);

static void charge_control_mgr_wakeup_event(struct os_event *);
static void charge_control_base_ts_update_event(struct os_event *);

/* ---------------------- CHARGE CONTROL --------------------------- */

/**
 * Prevents changes to a charge control device during an operation
 *
 * @param Charge control device to lock
 *
 * @return 0 on success, non-zero error code on failure
 */
static int charge_control_lock(struct charge_control *);

/**
 * Releases mutex lock on charge control device
 *
 * @param Charge control device to release
 */
static void charge_control_unlock(struct charge_control *);

/**
 * Thread-safe operation to change poll rate on charge control device
 */
static void
charge_control_update_poll_rate(struct charge_control *, uint32_t);

static os_time_t
charge_control_calc_nextrun_delta(struct charge_control *, os_time_t);

static struct charge_control *
charge_control_find_min_nextrun(os_time_t, os_time_t *);

static void
charge_control_update_nextrun(struct charge_control *, os_time_t);

static int
charge_control_read_data_func(struct charge_control *, void *, void *,
        charge_control_type_t);

static void
charge_control_up_timestamp(struct charge_control *);

static int
charge_control_insert_type_trait(struct charge_control *,
        struct charge_control_type_traits *);

static int
charge_control_remove_type_trait(struct charge_control *,
        struct charge_control_type_traits *);

/**
 * Search the charge control type traits list for a specific type of charge
 * controller.  Set the charge control pointer to NULL to start from the
 * beginning of the list or to a specific charge control object to begin from
 * that point in the list.
 *
 * @param The charge controller type to search for
 * @param Pointer to a charge controller
 *
 * @return NULL when no charge control type is found, pointer to a
 * charge_control_type_traits structure when found.
 */
static struct charge_control_type_traits *
charge_control_get_type_traits_bytype(charge_control_type_t,
        struct charge_control *);

static struct charge_control *
charge_control_get_type_traits_byname(char *,
        struct charge_control_type_traits **, charge_control_type_t);

static uint8_t
charge_control_type_traits_empty(struct charge_control *);

static void
charge_control_poll_per_type_trait(struct charge_control *, os_time_t,
        os_time_t);

/* ---------------------- CHARGE CONTROL MANAGER ------------------- */

/**
 * Pends on the charge control manager mutex indefinitely.  Must be used prior
 * to operations which iterate through the list of charge controllers.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static int
charge_control_mgr_lock(void);

/** Releases the charge control manager mutex. */
static void
charge_control_mgr_unlock(void);

static int
charge_control_mgr_match_bydevname(struct charge_control *, void *arg);

static void
charge_control_mgr_remove(struct charge_control *);

static void
charge_control_mgr_insert(struct charge_control *);

static void
charge_control_mgr_poll_bytype(struct charge_control *, charge_control_type_t,
        struct charge_control_type_traits *, os_time_t);

static void
charge_control_mgr_evq_set(struct os_eventq *);

static void
charge_control_mgr_init(void);

/* =================================================================
 * ====================== LOCAL STRUCTS/VARIABLES ==================
 * =================================================================
 */

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

/* =================================================================
 * ====================== PKG ======================================
 * =================================================================
 */

void charge_control_pkg_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    charge_control_mgr_init();

#if MYNEWT_VAL(CHARGE_CONTROL_CLI)
    charge_control_shell_register();
#endif
}

/* =================================================================
 * ====================== OS =======================================
 * =================================================================
 */

static void
charge_control_read_ev_cb(struct os_event *ev)
{
    int rc;
    struct charge_control_read_ev_ctx *ccrec;

    ccrec = ev->ev_arg;
    rc = charge_control_read(ccrec->ccrec_charge_control, ccrec->ccrec_type, 
            NULL, NULL, OS_TIMEOUT_NEVER);
    assert(rc == 0);
}

static void
charge_control_mgr_wakeup_event(struct os_event *ev)
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

        if (charge_control_type_traits_empty(cursor)) {

            charge_control_mgr_poll_bytype(cursor, cursor->cc_mask, NULL, now);
        } else {
            charge_control_poll_per_type_trait(cursor, now, next_wakeup);
        }

        charge_control_update_nextrun(cursor, now);

        charge_control_unlock(cursor);
    }

    charge_control_mgr_unlock();

    os_callout_reset(&charge_control_mgr.mgr_wakeup_callout, next_wakeup);
}

static void
charge_control_base_ts_update_event(struct os_event *ev)
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

/* =================================================================
 * ====================== CHARGE CONTROL ===========================
 * =================================================================
 */

static int
charge_control_lock(struct charge_control *cc)
{
    int rc;

    rc = os_mutex_pend(&cc->cc_lock, OS_TIMEOUT_NEVER);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return (rc);
}

static void
charge_control_unlock(struct charge_control *cc)
{
    os_mutex_release(&cc->cc_lock);
}

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

static int
charge_control_insert_type_trait(struct charge_control *cc,
        struct charge_control_type_traits *cctt)
{
    struct charge_control_type_traits *cursor, *prev;
    int rc;

    rc = charge_control_lock(cc);
    if (rc != 0) {
        goto err;
    }

    prev = cursor = NULL;
    SLIST_FOREACH(cursor, &cc->cc_type_traits_list, cctt_next) {
        if (cursor->cctt_poll_n == 0) {
            break;
        }

        if (OS_TIME_TICK_LT(cctt->cctt_poll_n, cursor->cctt_poll_n)) {
            break;
        }

        prev = cursor;
    }

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&cc->cc_type_traits_list, cctt, cctt_next);
    }
    else {
        SLIST_INSERT_AFTER(prev, cctt, cctt_next);
    }

    charge_control_unlock(cc);

    return 0;
err:
    return rc;
}

static int
charge_control_remove_type_trait(struct charge_control *cc,
        struct charge_control_type_traits *cctt)
{
    int rc;

    rc = charge_control_lock(cc);
    if (rc != 0) {
        goto err;
    }

    /* Remove this entry from the list */
    SLIST_REMOVE(&cc->cc_type_traits_list, cctt, charge_control_type_traits,
            cctt_next);

    charge_control_unlock(cc);

    return (0);
err:
    return (rc);
}

static struct charge_control_type_traits *
charge_control_get_type_traits_bytype(charge_control_type_t type,
        struct charge_control * cc)
{
    struct charge_control_type_traits *cctt;

    cctt = NULL;

    charge_control_lock(cc);

    SLIST_FOREACH(cctt, &cc->cc_type_traits_list, cctt_next) {
        if (cctt->cctt_charge_control_type == type) {
            break;
        }
    }

    charge_control_unlock(cc);

    return cctt;
}

static struct charge_control *
charge_control_get_type_traits_byname(char *devname,
        struct charge_control_type_traits **cctt, charge_control_type_t type)
{
    struct charge_control *cc;

    cc = charge_control_mgr_find_next_bydevname(devname, NULL);
    if (!cc) {
        goto err;
    }

    *cctt = charge_control_get_type_traits_bytype(type, cc);

err:
    return cc;
}

static uint8_t
charge_control_type_traits_empty(struct charge_control * cc)
{
    return SLIST_EMPTY(&cc->cc_type_traits_list);
}

static void
charge_control_poll_per_type_trait(struct charge_control *cc, os_time_t now,
        os_time_t next_wakeup)
{
    struct charge_control_type_traits *cctt;

    /* Lock the charge controller */
    charge_control_lock(cc);

    SLIST_FOREACH(cctt, &cc->cc_type_traits_list, cctt_next) {

        /* poll multiple is one if no multiple is specified,
         * as a result, the charge controller would get polled at the
         * poll rate if no multiple is specified
         *
         * If a multiple is specified, the charge controller would get polled
         * at the poll multiple
         */

        charge_control_mgr_poll_bytype(cc, cctt->cctt_charge_control_type, cctt,
                               now);
    }

    /* Unlock the charge controller to allow other access */
    charge_control_unlock(cc);
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

/* =================================================================
 * ====================== CHARGE CONTROL MANAGER ===================
 * =================================================================
 */

static int
charge_control_mgr_lock(void)
{
    int rc;

    rc = os_mutex_pend(&charge_control_mgr.mgr_lock, OS_TIMEOUT_NEVER);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return rc;
}

static void
charge_control_mgr_unlock(void)
{
    (void) os_mutex_release(&charge_control_mgr.mgr_lock);
}

static int
charge_control_mgr_match_bydevname(struct charge_control *cc, void *arg)
{
    char *devname;

    devname = (char *) arg;

    if (!strcmp(cc->cc_dev->od_name, devname)) {
        return (1);
    }

    return (0);
}

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
        charge_control_type_t type, struct charge_control_type_traits *cctt,
        os_time_t now)
{
    if (!cctt || !cctt->cctt_polls_left) {
        /* Charge control read results. Every time a charge controller is read,
         * all of its listeners are called by default. Specify NULL as a
         * callback, because we just want to run all the listeners.
         */

        charge_control_read(cc, type, NULL, NULL, OS_TIMEOUT_NEVER);

        charge_control_lock(cc);

        if (cctt) {
            if (!cctt->cctt_polls_left && cctt->cctt_poll_n) {
                cctt->cctt_polls_left = cctt->cctt_poll_n;
                cctt->cctt_polls_left--;
            }
        }

        /* Unlock the sensor to allow other access */
        charge_control_unlock(cc);

    } else {
        cctt->cctt_polls_left--;
    }
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
charge_control_mgr_register(struct charge_control *cc)
{
    int rc;

    rc = charge_control_mgr_lock();
    if (rc != 0) {
        goto err;
    }

    rc = charge_control_lock(cc);
    if (rc != 0) {
        goto err;
    }

    charge_control_mgr_insert(cc);

    charge_control_unlock(cc);

    charge_control_mgr_unlock();

    return (0);
err:
    return (rc);
}

struct os_eventq *
charge_control_mgr_evq_get(void)
{
    return (charge_control_mgr.mgr_eventq);
}

struct charge_control *
charge_control_mgr_find_next(charge_control_mgr_compare_func_t compare_func,
        void *arg, struct charge_control *prev_cursor)
{
    struct charge_control *cursor;
    int rc;

    cursor = NULL;

    /* Couldn't acquire lock of charge control list, exit */
    rc = charge_control_mgr_lock();
    if (rc != 0) {
        goto done;
    }

    cursor = prev_cursor;
    if (cursor == NULL) {
        cursor = SLIST_FIRST(&charge_control_mgr.mgr_charge_control_list);
    } else {
        cursor = SLIST_NEXT(prev_cursor, cc_next);
    }

    while (cursor != NULL) {
        if (compare_func(cursor, arg)) {
            break;
        }
        cursor = SLIST_NEXT(cursor, cc_next);
    }

    charge_control_mgr_unlock();

done:
    return (cursor);
}

struct charge_control *
charge_control_mgr_find_next_bytype(charge_control_type_t type,
        struct charge_control *prev_cursor)
{
    return (charge_control_mgr_find_next(charge_control_mgr_match_bytype,
            (void *) &type, prev_cursor));
}

struct charge_control *
charge_control_mgr_find_next_bydevname(char *devname,
        struct charge_control *prev_cursor)
{
    return (charge_control_mgr_find_next(charge_control_mgr_match_bydevname,
            devname, prev_cursor));
}

int
charge_control_mgr_match_bytype(struct charge_control *cc, void *arg)
{
    charge_control_type_t *type;

    type = (charge_control_type_t *) arg;

    /* cc_types is a bitmask that contains the supported charge control types
     * for this charge controller, and type is the bitmask we're searching for
     * We also look at the mask as the driver might be configured to work in a
     * mode where only some of the charge control types are supported but not
     * all. Compare the three, and if there is a match, return 1. If it is not
     * supported, return 0.
     */
    return (*type & cc->cc_types & cc->cc_mask) ? 1 : 0;
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

int
charge_control_set_n_poll_rate(char * devname,
        struct charge_control_type_traits *cctt)
{
    struct charge_control *cc;
    struct charge_control_type_traits *cctt_tmp;
    int rc;

    if (!cctt) {
        rc = SYS_EINVAL;
        goto err;
    }

    cctt_tmp = NULL;

    cc = charge_control_get_type_traits_byname(devname, &cctt_tmp,
                                           cctt->cctt_charge_control_type);
    if (!cc) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (!cctt_tmp && cctt) {
        rc = charge_control_insert_type_trait(cc, cctt);
        rc |= charge_control_lock(cc);
        if (rc) {
            goto err;
        }
        cctt_tmp = cctt;
        cctt_tmp->cctt_polls_left = cctt->cctt_poll_n;
        charge_control_unlock(cc);
    } else if (cctt_tmp) {
        rc = charge_control_remove_type_trait(cc, cctt_tmp);
        if (rc) {
            goto err;
        }

        charge_control_lock(cc);

        cctt_tmp->cctt_poll_n = cctt->cctt_poll_n;
        cctt_tmp->cctt_polls_left = cctt->cctt_poll_n;

        charge_control_unlock(cc);

        rc = charge_control_insert_type_trait(cc, cctt_tmp);
        if (rc) {
            goto err;
        }

    } else {
        rc = SYS_EINVAL;
        goto err;
    }

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
