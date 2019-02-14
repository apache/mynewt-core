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

#if !MYNEWT_VAL(FAULT_STUB)

#include "modlog/modlog.h"
#include "fault/fault.h"
#include "fault_priv.h"

struct fault_domain {
    uint8_t success_delta;
    uint8_t failure_delta;

#if MYNEWT_VAL(FAULT_DOMAIN_NAMES)
    const char *name;
#endif
};

uint8_t fault_chronic_counts[MYNEWT_VAL(FAULT_MAX_DOMAINS)];
static struct fault_domain fault_domains[MYNEWT_VAL(FAULT_MAX_DOMAINS)];

static fault_thresh_fn *fault_thresh_cb;

void
fault_configure_cb(fault_thresh_fn *cb)
{
    fault_thresh_cb = cb;
}

static struct fault_domain *
fault_get_domain(int domain_id)
{
    if (domain_id < 0 || domain_id >= MYNEWT_VAL(FAULT_MAX_DOMAINS)) {
        return NULL;
    }

    return &fault_domains[domain_id];
}

bool
fault_domain_is_registered(int domain_id)
{
    const struct fault_domain *dom;

    dom = fault_get_domain(domain_id);
    if (dom == NULL) {
        return false;
    }

    return dom->failure_delta != 0;
}

static struct fault_domain *
fault_get_registered_domain(int domain_id)
{
    struct fault_domain *dom;

    dom = fault_get_domain(domain_id);
    if (dom == NULL) {
        return NULL;
    }

    if (!fault_domain_is_registered(domain_id)) {
        return NULL;
    }

    return dom;
}

static bool
fault_recorder_is_saturated(const struct fault_recorder *recorder)
{
    return debouncer_val(&recorder->deb) == recorder->deb.max;
}

static int
fault_recorder_state(const struct fault_recorder *recorder)
{
    if (fault_recorder_is_saturated(recorder)) {
        return FAULT_STATE_ERROR;
    } else if (debouncer_state(&recorder->deb)) {
        return FAULT_STATE_WARN;
    } else {
        return FAULT_STATE_GOOD;
    }
}

static int
fault_adjust_chronic_count(int domain_id, int delta)
{
    int count;

    /* Increase and persist the chronic failure count if it would not wrap. */
    count = fault_chronic_counts[domain_id] + delta;
    if (count < 0) {
        count = 0;
    } else if (count > UINT8_MAX) {
        count = UINT8_MAX;
    }

    return fault_set_chronic_count(domain_id, count);
}

static int
fault_decrease_chronic_count(int domain_id)
{
    const struct fault_domain *dom;

    dom = fault_get_registered_domain(domain_id);
    assert(dom != NULL);

    return fault_adjust_chronic_count(domain_id, -dom->success_delta);
}

static int
fault_increase_chronic_count(int domain_id)
{
    const struct fault_domain *dom;

    dom = fault_get_registered_domain(domain_id);
    assert(dom != NULL);

    return fault_adjust_chronic_count(domain_id, dom->failure_delta);
}

/**
 * @brief processes the result of a fault-capable operation.
 *
 * @param recorder              The fault recorder associated with the
 *                                  operation.
 * @param success               Whether the operation was successful.
 */
int
fault_process(struct fault_recorder *recorder, bool is_failure)
{
    int prev_state;
    int state;
    int delta;

    prev_state = fault_recorder_state(recorder);

    /* There is nothing to do if we are already at the maximum fault count. */
    if (is_failure && prev_state == FAULT_STATE_ERROR) {
        return FAULT_STATE_ERROR;
    }

    if (is_failure) {
        delta = 1;
    } else {
        delta = -1;
    }

    debouncer_adjust(&recorder->deb, delta);
    state = fault_recorder_state(recorder);

    if (state == FAULT_STATE_GOOD) {
        /* The domain seems to be working; decrease chronic failure count. */
        fault_decrease_chronic_count(recorder->domain_id);
    } else {
        if (state == FAULT_STATE_ERROR) {
            /* Fault detected; trigger a crash in debug builds. */
            DEBUG_PANIC();

            /* Increase chronic fail count and persist. */
            fault_increase_chronic_count(recorder->domain_id);
        }
    }

    /* Notify the application if the fault state changed. */
    if (prev_state != state && fault_thresh_cb != NULL) {
        fault_thresh_cb(recorder->domain_id, prev_state, state, recorder->arg);
    }

    return state;
}

int
fault_success(struct fault_recorder *recorder)
{
    return fault_process(recorder, false);
}

int
fault_failure(struct fault_recorder *recorder)
{
    return fault_process(recorder, true);
}

void
fault_fatal(int domain_id, void *arg, bool is_failure)
{
    struct fault_recorder recorder;
    int rc;

    rc = fault_recorder_init(&recorder, domain_id, 1, 1, arg);
    assert(rc == 0);

    fault_process(&recorder, is_failure);
}

void
fault_fatal_success(int domain_id, void *arg)
{
    fault_fatal(domain_id, arg, false);
}

void
fault_fatal_failure(int domain_id, void *arg)
{
    fault_fatal(domain_id, arg, true);
}

int
fault_get_chronic_count(int domain_id, uint8_t *out_count)
{
    if (fault_get_registered_domain(domain_id) == NULL) {
        return SYS_EINVAL;
    }

    *out_count = fault_chronic_counts[domain_id];
    return 0;
}

int
fault_set_chronic_count(int domain_id, uint8_t count)
{
    int rc;

    if (fault_get_registered_domain(domain_id) == NULL) {
        return SYS_EINVAL;
    }

    if (fault_chronic_counts[domain_id] == count) {
        return 0;
    }

    fault_chronic_counts[domain_id] = count;

    rc = fault_conf_persist_chronic_counts();
    DEBUG_ASSERT(rc == 0);

    return rc;
}

int
fault_recorder_init(struct fault_recorder *recorder,
                    int domain_id,
                    uint16_t warn_thresh,
                    uint16_t error_thresh,
                    void *arg)
{
    int rc;

    if (fault_get_registered_domain(domain_id) == NULL) {
        return SYS_EINVAL;
    }

    if (warn_thresh > error_thresh) {
        return SYS_EINVAL;
    }

    rc = debouncer_init(&recorder->deb, 0, warn_thresh, error_thresh);
    if (rc != 0) {
        return rc;
    }

    /* If this fault recorder is associated with a chronically-failing domain,
     * start the recorder with an initial acute failure count.  For recorders
     * with high failure thresholds, this ensures that at least a few successes
     * are required before the domain is considered stable.
     */
    if (fault_chronic_counts[domain_id] > 0) {
        debouncer_set(&recorder->deb, warn_thresh / 2);
    }

    recorder->domain_id = domain_id;
    recorder->arg = arg;

    return 0;
}

const char *
fault_domain_name(int domain_id)
{
#if MYNEWT_VAL(FAULT_DOMAIN_NAMES)
    const struct fault_domain *domain;

    domain = fault_get_registered_domain(domain_id);
    if (domain != NULL) {
        return domain->name;
    }
#endif

    return NULL;
}

int
fault_register_domain_priv(int domain_id, uint8_t success_delta,
                           uint8_t failure_delta, const char *name)
{
    struct fault_domain *dom;

    if (failure_delta == 0) {
        return SYS_EINVAL;
    }

    dom = fault_get_domain(domain_id);
    if (dom == NULL) {
        return SYS_EINVAL;
    }

    if (fault_domain_is_registered(domain_id)) {
        return SYS_EALREADY;
    }

    *dom = (struct fault_domain) {
        .success_delta = success_delta,
        .failure_delta = failure_delta,
#if MYNEWT_VAL(FAULT_DOMAIN_NAMES)
        .name = name,
#endif
    };

    return 0;
}

void
fault_init(void)
{
    int rc;

    rc = fault_conf_init();
    SYSINIT_PANIC_ASSERT(rc == 0);
}

#endif /* !MYNEWT_VAL(FAULT_STUB) */
