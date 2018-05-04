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

/**
 * @file charge_control.h
 * @brief Hardware-agnostic interface for battery charge control ICs
 *
 * The Charge Control interface provides a hardware-agnostic layer for driving
 * battery charge controller ICs.
 */

#ifndef __CHARGE_CONTROL_H__
#define __CHARGE_CONTROL_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Package init function.  Remove when we have post-kernel init stages.
 */
void charge_control_pkg_init(void);


/* Forward declaration of charge_control structure. */
struct charge_control;

/* Comments above intentionally not in Doxygen format */

/* =================================================================
 * ====================== DEFINES/MACROS ===========================
 * =================================================================
 */

/**
 * Charge control listener constants
 */
#define CHARGE_CONTROL_IGN_LISTENER     1

/**
 * @{ Charge Control API
 */

/**
 * Return the OS device structure corresponding to this charge controller
 */
#define CHARGE_CONTROL_GET_DEVICE(__c) ((__c)->cc_dev)

/**
 * Return the interface for this charge controller
 */
#define CHARGE_CONTROL_GET_ITF(__c) (&((__c)->cc_itf))

// ---------------------- TYPE -------------------------------------

/** 
 * Charge controller supported functionality
 */
typedef enum {
    /** No type, used for queries */
    CHARGE_CONTROL_TYPE_NONE            = (1 << 0),
    /** Charging status reporting supported */
    CHARGE_CONTROL_TYPE_STATUS          = (1 << 1),
    /** Fault reporting supported */
    CHARGE_CONTROL_TYPE_FAULT           = (1 << 2),
} charge_control_type_t;

/** 
 * Possible charge controller states
 */
typedef enum {
    /** Charge controller is disabled (if enable/disable function exists) */
    CHARGE_CONTROL_STATUS_DISABLED      = 0,
    /** No charge source is present at the charge controller input */
    CHARGE_CONTROL_STATUS_NO_SOURCE,
    /** Charge controller is charging a battery */
    CHARGE_CONTROL_STATUS_CHARGING,
    /** Charge controller has completed its charging cycle */
    CHARGE_CONTROL_STATUS_CHARGE_COMPLETE,
    /** Charging is temporarily suspended */
    CHARGE_CONTROL_STATUS_SUSPEND,
    /** Charge controller has detected a fault condition */
    CHARGE_CONTROL_STATUS_FAULT,
    /** Unspecified status; User must understand how to interpret */
    CHARGE_CONTROL_STATUS_OTHER,
} charge_control_status_t;

/**
 * Possible fault conditions for the charge controller
 */
typedef enum {
    /** No fault detected */
    CHARGE_CONTROL_FAULT_NONE           = 0,
    /** Charge controller input voltage exceeds threshold */
    CHARGE_CONTROL_FAULT_OV             = (1 << 0),
    /** Charge controller input voltage below required operating level */
    CHARGE_CONTROL_FAULT_UV             = (1 << 1),
    /** Charge controller is not running at its programmed charging current */
    CHARGE_CONTROL_FAULT_ILIM           = (1 << 2),
    /** Charge controller detected an over-temperature condition */
    CHARGE_CONTROL_FAULT_THERM          = (1 << 3),
    /** Unspecified fault; User must understand how to interpret */
    CHARGE_CONTROL_FAULT_OTHER          = (1 << 4),
} charge_control_fault_t;

/**
 * Charge controller type traits list.  Allows a certain type of charge control
 * data to be polled at n times the normal poll rate.
 */
struct charge_control_type_traits {
    /** The type of charge control data to which the traits apply */
    charge_control_type_t cctt_charge_control_type;

    /** Poll rate multiple */
    uint16_t cctt_poll_n;

    /** Polls left until the charge control type is polled */
    uint16_t cctt_polls_left;

    /** Next item in the traits list. The head of this list is contained
     * within the charge control object
     */
    SLIST_ENTRY(charge_control_type_traits) cctt_next;
};

// ---------------------- CONFIG -----------------------------------

/**
 * Configuration structure, describing a specific charge controller type off of
 * an existing charge controller.
 */
struct charge_control_cfg {
    /** Reserved for future use */
    uint8_t _reserved[4];
};

// ---------------------- INTERFACE --------------------------------

/**
 * Charge control serial interface types
 */
enum charge_control_itf_type {
    CHARGE_CONTROL_ITF_SPI      =   0,
    CHARGE_CONTROL_ITF_I2C      =   1,
    CHARGE_CONTROL_ITF_UART     =   2,
};

/**
 * Specifies a serial interface to use for communication with the charge
 * controller (if applicable)
 */
struct charge_control_itf {
    /* Charge control interface type */
    enum charge_control_itf_type cci_type;

    /* Charge control interface number (i.e. 0 for I2C0, 1 for I2C1) */
    uint8_t cci_num;

    /* Charge control CS pin (for SPI interface type) */
    uint8_t cci_cs_pin;

    /* Charge control interface address (for I2C interface type) */
    uint16_t cci_addr;
};

// ---------------------- LISTENER ---------------------------------

/**
 * Callback for handling charge controller status.
 *
 * @param The charge control object for which data is being returned
 * @param The argument provided to charge_control_read() function.
 * @param A single charge control reading for that listener
 * @param The type of charge control reading for the data function
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*charge_control_data_func_t)(struct charge_control *, void *,
        void *, charge_control_type_t);

/**
 * Listener structure which may be registered to a charge controller. The
 * listener will call the supplied charge_control_data_func_t every time it
 * reads new data of the specified type.
 */
struct charge_control_listener {
    /* The type of charge control data to listen for, this is interpreted as a
     * mask, and this listener is called for all charge control types that
     * match the mask.
     */
    charge_control_type_t ccl_type;

    /* Charge control data handler function, called when has data */
    charge_control_data_func_t ccl_func;

    /* Argument for the charge control listener */
    void *ccl_arg;

    /* Next item in the charge control listener list.  The head of this list is
     * contained within the charge control object.
     */
    SLIST_ENTRY(charge_control_listener) ccl_next;
};

/* ---------------------- DRIVER FUNCTIONS ------------------------- */

/**
 * Read from a charge controller, given a specific type of information to read
 * (e.g. CHARGE_CONTROL_TYPE_STATUS).
 *
 * @param The charge controller to read from
 * @param The type(s) of charge control value(s) to read, interpreted as a mask
 * @param The function to call with each value read.  If NULL, it calls all
 *        charge control listeners associated with this function.
 * @param The argument to pass to the read callback.
 * @param Timeout.  If block until result, specify OS_TIMEOUT_NEVER, 0 returns
 *        immediately (no wait.)
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*charge_control_read_func_t)(struct charge_control *,
        charge_control_type_t, charge_control_data_func_t, void *, uint32_t);

/**
 * Get the configuration of the charge controller.  This includes the value
 * type of the charge controller.
 *
 * @param The desired charge controller
 * @param The type of charge control value to get configuration for
 * @param A pointer to the charge control value to place the returned result
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*charge_control_get_config_func_t)(struct charge_control *,
        charge_control_type_t, struct charge_control_cfg *);

/** 
 * Reconfigure the settings of a charge controller.
 *
 * @param ptr to the charge controller-specific stucture
 * @param ptr to the charge controller-specific configuration structure
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*charge_control_set_config_func_t)(struct charge_control *,
        void *);

/**
 * Read the status of a charge controller.
 *
 * @param ptr to the charge controller-specific structure
 * @param ptr to the location where the charge control status will be stored
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*charge_control_get_status_func_t)(struct charge_control *, int *);

/**
 * Read the fault status of a charge controller.
 *
 * @param ptr to the charge controller-specific structure
 * @param ptr to the location where the fault status will be stored
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*charge_control_get_fault_func_t)(struct charge_control *,
        charge_control_fault_t *);

/**
 * Enable a charge controller
 *
 * @param The charge controller to enable
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*charge_control_enable_func_t)(struct charge_control *);

/**
 * Disable a charge controller
 *
 * @param The charge controller to disable
 *
 * @return 0 on success, non-zero error code on failure
 */
typedef int (*charge_control_disable_func_t)(struct charge_control *);

/** 
 * Pointers to charge controller-specific driver functions.  
 */
struct charge_control_driver {
    charge_control_read_func_t          ccd_read;
    charge_control_get_config_func_t    ccd_get_config;
    charge_control_set_config_func_t    ccd_set_config;
    charge_control_get_status_func_t    ccd_get_status;
    charge_control_get_fault_func_t     ccd_get_fault;
    charge_control_enable_func_t        ccd_enable;
    charge_control_disable_func_t       ccd_disable;
};

// ---------------------- POLL RATE CONTROL ------------------------

struct charge_control_timestamp {
    struct os_timeval cct_ostv;
    struct os_timezone cct_ostz;
    uint32_t cct_cputime;
};

// ---------------------- CHARGE CONTROL OBJECT --------------------

struct charge_control {
    /* The OS device this charge controller inherits from, this is typically a
     * charge controller specific driver.
     */
    struct os_dev *cc_dev;

    /* The lock for this charge controller object */
    struct os_mutex cc_lock;


    /* A bit mask describing information types available from this charge
     * controller.  If the bit corresponding to the charge_control_type_t is
     * set, then this charge controller supports that type of information.
     */
    charge_control_type_t cc_types;

    /* Charge control information type mask.  A driver for a charge controller
     * or an implementation of the controller may be written to support some or
     * all of the types of information */
    charge_control_type_t cc_mask;

    /**
     * Poll rate in MS for this charge controller.
     */
    uint32_t cc_poll_rate;

    /* The next time at which we will poll data from this charge controller */
    os_time_t cc_next_run;

    /* Charge controller driver specific functions, created by the device
     * registering the charge controller.
     */
    struct charge_control_driver *cc_funcs;

    /* Charge controller last reading timestamp */
    struct charge_control_timestamp cc_sts;

    /* Charge controller interface structure */
    struct charge_control_itf cc_itf;

    /* A list of listeners that are registered to receive data from this
     * charge controller
     */
    SLIST_HEAD(, charge_control_listener) cc_listener_list;

    /** A list of traits registered to the charge control data types */
    SLIST_HEAD(, charge_control_type_traits) cc_type_traits_list;

    /* The next charge controller in the global charge controller list. */
    SLIST_ENTRY(charge_control) cc_next;
};

/* =================================================================
 * ====================== CHARGE CONTROL ===========================
 * =================================================================
 */

/**
 * Initialize charge control structure data and mutex and associate it with an
 * os_dev.
 *
 * @param Charge control struct to initialize
 * @param os_dev to associate with the charge control struct
 *
 * @return 0 on success, non-zero error code on failure.
 */
int charge_control_init(struct charge_control *, struct os_dev *);

/**
 * Register a charge control listener. This allows a calling application to
 * receive callbacks for data from a given charge control object.
 *
 * For more information on the type of callbacks available, see the documentation
 * for the charge control listener structure.
 *
 * @param The charge controller to register a listener on
 * @param The listener to register onto the charge controller
 *
 * @return 0 on success, non-zero error code on failure.
 */
int charge_control_register_listener(struct charge_control *,
        struct charge_control_listener *);

/**
 * Un-register a charge control listener. This allows a calling application to
 * unset callbacks for a given charge control object.
 *
 * @param The charge control object
 * @param The listener to remove from the charge control listener list
 *
 * @return 0 on success, non-zero error code on failure.
 */
int charge_control_unregister_listener(struct charge_control *,
        struct charge_control_listener *);


/**
 * Read from a charge controller, given a specific type of information to read
 * (e.g. CHARGE_CONTROL_TYPE_STATUS).
 *
 * @param The charge controller to read from
 * @param The type(s) of charge control value(s) to read, interpreted as a mask
 * @param The function to call with each value read.  If NULL, it calls all
 *        charge control listeners associated with this function.
 * @param The argument to pass to the read callback.
 * @param Timeout.  If block until result, specify OS_TIMEOUT_NEVER, 0 returns
 *        immediately (no wait.)
 *
 * @return 0 on success, non-zero error code on failure.
 */
int charge_control_read(struct charge_control *, charge_control_type_t, 
        charge_control_data_func_t, void *, uint32_t);

/**
 * Set the charge controller poll rate
 *
 * @param The name of the charge controller
 * @param The poll rate in milli seconds
 */
int
charge_control_set_poll_rate_ms(char *, uint32_t);

/**
 * Set a charge control type to poll at some multiple of the poll rate
 *
 * @param The name of the charge controller
 * @param The charge control type
 * @param The multiple of the poll rate
 */
int 
charge_control_set_n_poll_rate(char *, struct charge_control_type_traits *);

/**
 * Set the driver functions for this charge controller, along with the type of
 * data available for the given charge controller.
 *
 * @param The charge controller to set the driver information for
 * @param The types of charge control data available
 * @param The driver functions for this charge controller
 *
 * @return 0 on success, non-zero error code on failure
 */
static inline int
charge_control_set_driver(struct charge_control *cc, charge_control_type_t type,
        struct charge_control_driver *driver)
{
    cc->cc_funcs = driver;
    cc->cc_types = type;

    return (0);
}

/**
 * Set the charge control driver mask so that the developer who configures the
 * charge controller tells the charge control framework which data types to 
 * send back to the user
 *
 * @param The charge controller to set the mask for
 * @param The mask
 */
static inline int
charge_control_set_type_mask(struct charge_control *cc,
        charge_control_type_t mask)
{
    cc->cc_mask = mask;

    return (0);
}

/**
 * Check if the given type is supported by the charge control device
 *
 * @param charge control object
 * @param Type to be checked
 *
 * @return type bitfield, if supported, 0 if not supported
 */
static inline charge_control_type_t
charge_control_check_type(struct charge_control *cc,
        charge_control_type_t type)
{
    return (cc->cc_types & cc->cc_mask & type);
}

/**
 * Set interface type and number
 *
 * @param The charge control object to set the interface for
 * @param The interface type to set
 * @param The interface number to set
 */
static inline int
charge_control_set_interface(struct charge_control *cc,
        struct charge_control_itf *itf)
{
    cc->cc_itf.cci_type = itf->cci_type;
    cc->cc_itf.cci_num = itf->cci_num;
    cc->cc_itf.cci_cs_pin = itf->cci_cs_pin;
    cc->cc_itf.cci_addr = itf->cci_addr;

    return (0);
}

/**
 * Read the configuration for the charge control type "type," and return the
 * configuration into "cfg."
 *
 * @param The charge control to read configuration for
 * @param The type of charge control configuration to read
 * @param The configuration structure to point to.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static inline int
charge_control_get_config(struct charge_control *cc,
        charge_control_type_t type, struct charge_control_cfg *cfg)
{
    return (cc->cc_funcs->ccd_get_config(cc, type, cfg));
}

// =================================================================
// ====================== CHARGE CONTROL MANAGER ===================
// =================================================================

/**
 * @{ Charge Control Manager API
 */

/**
 * Registers a charge controller with the charge control manager list.  This
 * makes the charge controller externally searchable.
 *
 * @param The charge controller to register
 *
 * @return 0 on success, non-zero error code on failure.
 */
int charge_control_mgr_register(struct charge_control *);

/**
 * Return the charge control event queue. 
 */
struct os_eventq *charge_control_mgr_evq_get(void);


typedef int (*charge_control_mgr_compare_func_t)(struct charge_control *,
        void *);

/**
 * The charge control manager contains a list of charge controllers, this
 * function returns the next charge controller in that list, for which
 * compare_func() returns successful (one).  If prev_cursor is provided, the
 * function starts at that point in the charge controller list.
 *
 * @warn This function MUST be locked by charge_control_mgr_lock/unlock() if
 * the goal is to iterate through charge controllers (as opposed to just
 * finding one.)  As the "prev_cursor" may be resorted in the charge control
 * list, in between calls.
 *
 * @param The comparison function to use against charge controllers in the list.
 * @param The argument to provide to that comparison function
 * @param The previous charge controller in the manager list, in case of
 *        iteration.  If desire is to find first matching charge controller,
 *        provide a NULL value.
 *
 * @return A pointer to the first charge controller found from prev_cursor, or
 *         NULL, if none found.
 *
 */
struct charge_control *charge_control_mgr_find_next(
        charge_control_mgr_compare_func_t, void *, struct charge_control *);

/**
 * Find the "next" charge controller available for a giventype.
 *
 * If the charge control parameter is present find the next entry from that
 * parameter.  Otherwise, return the first match.
 *
 * @param The type of charge controller to search for
 * @param The cursor to search from, or NULL to start from the beginning.
 *
 * @return A pointer to the charge control object matching that type, or NULL if
 *         none found.
 */
struct charge_control *charge_control_mgr_find_next_bytype(
        charge_control_type_t, struct charge_control *);

/**
 * Search the charge control list and find the next charge controller that 
 * corresponds to a given device name.
 *
 * @param The device name to search for
 * @param The previous charge controller found with this device name
 *
 * @return 0 on success, non-zero error code on failure
 */
struct charge_control *charge_control_mgr_find_next_bydevname(char *, 
        struct charge_control *);

/**
 * Check if charge control type matches
 *
 * @param The charge control object
 * @param The type to check
 *
 * @return 1 if matches, 0 if it doesn't match.
 */
int charge_control_mgr_match_bytype(struct charge_control *, void *);

/**
 * Puts read event on the charge control manager evq
 *
 * @param arg
 */
void
charge_control_mgr_put_read_evt(void *);

#if MYNEWT_VAL(CHARGE_CONTROL_CLI)
char*
charge_control_ftostr(float, char *, int);
#endif

/**
 * }@
 */

/* End Charge Control Manager API */


/**
 * @}
 */

/* End Charge Control API */

#ifdef __cplusplus
}
#endif

#endif /* __CHARGE_CONTROL_H__ */
