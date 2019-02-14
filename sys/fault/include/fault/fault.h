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

#ifndef H_FAULT_
#define H_FAULT_

/**
 * FAULT
 * This package tracks, logs, and recovers from errors.  
 *
 * TERMS
 * Domain:
 *     Identifies the part of the system that failed (e.g., BLE, file system,
 *     etc.).
 *
 * Recorder:
 *     Tracks successes and failures of a particular operation (e.g., BLE
 *     advertisement attempts).  Each recorder has an associated acute failure
 *     count.
 *
 * Acute failure count:
 *     The number of times a particular operation has failed.  Each failure
 *     increments the count; each success decrements it.
 *
 * Chronic failure count:
 *     Indicates the long-term stability of a particular domain.  The set of
 *     chronic failure counts (one per domain) is persisted to flash.
 *
 * Warn threshold:
 *     When a recorder's acute failure count count increases to its warn
 *     threshold, the recorder enters the warn state.  The application is
 *     notified of the state change.
 *
 * Error threshold:
 *     When a recorder's fault count increases to its error threshold, the
 *     recorder enters the error state.  When this happens, the domain's
 *     chronic failure count is increased and persisted.  In addition, the
 *     application is notified of the state change.  The application should
 *     perform either of the following actions when the error threshold is
 *     reached:
 *         o Reboot the system, or
 *         o Disable the domain entirely.
 *
 *     It may make sense to choose which action to take depending on the
 *     domain's chronic fail count.
 *
 * Fatal fault:
 *     A fatal fault is any operation with an error threshold of 1.  That is, a
 *     fatal failure always triggers a transition to the error state.
 */

#include "debounce/debounce.h"

#define FAULT_STATE_GOOD        0
#define FAULT_STATE_WARN        1
#define FAULT_STATE_ERROR       2

/**
 * Fault recorder: Tracks the occurrence of a specific fault type.
 *
 * All members should be considered private.
 */
struct fault_recorder {
    /* Config. */
    int domain_id;
    void *arg;

    /* State. */
    struct debouncer deb;
};

typedef void fault_thresh_fn(int domain_id, int prev_state, int state,
                             void *arg);

/**
 * @brief Configures a global callback to be executed when a fault state change
 * occurs.
 *
 * @param cb                    The callback to confiure.
 */
void fault_configure_cb(fault_thresh_fn *cb);

/**
 * @brief Indicates whether an ID corresponds to a registered domain.
 *
 * @param domain_id             The domain ID to check.
 *
 * @return                      true if the domain has been registered;
 *                              false otherwise.
 */
bool fault_domain_is_registered(int domain_id);

/**
 * @brief processes the result of a fault-capable operation.
 *
 * See `fault_success()` and `fault_failure()` for details.  All else being
 * equal, `fault_success()` and `fault_failure()` should be preferred over this
 * function for reasons of clarity.
 *
 * @param recorder              The fault recorder associated with the
 *                                  operation.
 * @param is_failure            Whether the operation failed.
 */
int fault_process(struct fault_recorder *recorder, bool is_failure);

/**
 * @brief Records a successful operation for the provided fault recorder.
 *
 * Each successful operation decrements the acute failure count of the provided
 * fault recorder, or has no effect if the failure count is already 0.  If this
 * function causes the acute failure count to reach 0, the chronic failure
 * count of the relevant faults is decreased and persisted to flash (if it is
 * currently greated than 0).
 *
 * @param recorder              The fault recorder to register a success with.
 *
 * @return                      The recorder's current state.
 */
int fault_success(struct fault_recorder *recorder);

/**
 * @brief Records a failed operation for the provided fault recorder.
 *
 * Each failed operation increments the failure count of the provided fault
 * recorder.  If this function causes the failure count to reach the provided
 * recorder's warn threshold or error threshold, the application is notified
 * via the global "fault thresh" callback.
 *
 * @param recorder              The fault recorder to register a failure with.
 *
 * @return                      The recorder's current state.
 */
int fault_failure(struct fault_recorder *recorder);

/**
 * @brief processes the result of a fatal operation.
 *
 * See `fault_fatal_success()` and `fault_fatal_failure()` for details.  All
 * else being equal, `fault_fatal_success()` and `fault_fatal_failure()` should
 * be preferred over this function for reasons of clarity.
 *
 * @param fault_domain          The domain associated with the operation.
 * @param arg                   Fault-specific argument to pass to the global
 *                                  calback.
 * @param is_failure            Whether the operation failed.
 */
void fault_fatal(int domain_id, void *arg, bool is_failure);

/**
 * @brief Records a success for a fatal operation.
 *
 * This function decreases the specified fault type's chronic failure count,
 * or has no effect if the count is already 0.
 *
 * @param domain_id             The domain associated with the successful
 *                                  operation.
 * @param arg                   Fault-specific argument to pass to the global
 *                                  calback.
 */
void fault_fatal_success(int domain_id, void *arg);

/**
 * @brief Records a failure for a fatal operation.
 *
 * This function triggers an error for the specified domain.  That is, it
 * increases the specified domain's chronic failure count, and notifies the
 * application via the "global thresh" callback.
 *
 * @param fault_domain          The fault type associated with the failed
 *                                  operation.
 * @param arg                   Fault-specific argument to pass to the global
 *                                  calback.
 */
void fault_fatal_failure(int domain_id, void *arg);

/**
 * @brief Retrieves the chronic failure associated with the specified domian.
 *
 * @param domain_id             The domain to query.
 * @param out_count             On success, the requested count gets written
 *                                  here.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL if the specified ID does not
 *                                  correspond to a registered domain.
 */
int fault_get_chronic_count(int domain_id, uint8_t *out_count);

/**
 * @brief Sets the chronic failure associated with the specified domian.
 *
 * @param domain_id             The domain whose chronic failure count should
 *                                  be set.
 * @param count                 The chronic failure count to set.
 *
 *                              SYS_EINVAL if the specified ID does not
 *                                  correspond to a registered domain.
 */
int fault_set_chronic_count(int domain_id, uint8_t count);

/**
 * @brief Retrieves the name of the specified fault domain.
 *
 * Note: if the FAULT_DOMAIN_NAMES setting is disabled, this function always
 * returns NULL.
 *
 * @param domain                The domain to query.
 *
 * @return                      The name of the specified domain, or NULL if
 *                                  the specified ID does not correspond to a
 *                                  registered domain.
 */
const char *fault_domain_name(int domain_id);

/**
 * @brief Constructs a new fault recorder for tracking errors.
 *
 * @param recorder              The fault recorder to initialize.
 * @param fault_domain          The domain of faults to track.
 * @param warn_thresh           The application is notified when the recorder's
 *                                  acute failure count reaches this value.
 * @param error_thresh          The domain's chronic failure
 *                                  count is increased, and the application is
 *                                  notified, when the recorder's acute failure
 *                                  count reaches this
 *                                  value.
 * @param arg                   Fault-specific argument.
 *
 * @return                      0 on success; SYS_E[...] on error.
 */
int fault_recorder_init(struct fault_recorder *recorder,
                        int domain_id,
                        uint16_t warn_thresh,
                        uint16_t error_thresh,
                        void *arg);


/**
 * @brief Private function; use `fault_register_domain` instead.
 */
int fault_register_domain_priv(int domain_id, uint8_t success_delta,
                               uint8_t fail_delta, const char *name);

/**
 * @brief Registers a fault domain.
 *
 * @param domain_id             The unique ID of the domain to register.
 * @param success_delta         The amount the domain's chronic failure count
 *                                  decreases when a recorder's acute failure
 *                                  count reaches 0.
 * @param failure_delta         The amount the domain's chronic failure count
 *                                  increases when a recorder's acute failure
 *                                  count reaches the error threshold.
 * @param name                  The name of the domain; ignored if
 *                                  FAULT_DOMAI_NAMES is disabled.
 *
 * @return                      0 on success; SYS_E[...] on error.
 */ 
static inline int
fault_register_domain(int domain_id, uint8_t success_delta,
                      uint8_t failure_delta, const char *name)
{
#if !MYNEWT_VAL(FAULT_DOMAIN_NAMES)
    /* Allow hardcoded strings to be optimized out. */
    name = NULL;
#endif

    return fault_register_domain_priv(domain_id, success_delta, failure_delta,
                                      name);
}

#endif
