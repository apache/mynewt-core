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

#include <assert.h>
#include <string.h>
#include "defs/error.h"
#include "bus/bus.h"
#include "bus/bus_debug.h"
#include "bus/bus_driver.h"
#if MYNEWT_VAL(BUS_STATS)
#include "stats/stats.h"
#endif

static os_time_t g_bus_node_lock_timeout;

#if MYNEWT_VAL(BUS_STATS)
STATS_NAME_START(bus_stats_section)
    STATS_NAME(bus_stats_section, lock_timeouts)
    STATS_NAME(bus_stats_section, read_ops)
    STATS_NAME(bus_stats_section, read_errors)
    STATS_NAME(bus_stats_section, write_ops)
    STATS_NAME(bus_stats_section, write_errors)
STATS_NAME_END(bus_stats_section)

#if MYNEWT_VAL(BUS_STATS_PER_NODE)
#define BUS_STATS_INC(_bdev, _bnode, _var)  \
    do {                                    \
        STATS_INC((_bdev)->stats, _var);    \
        STATS_INC((_bnode)->stats, _var);   \
    } while (0)
#else
#define BUS_STATS_INC(_bdev, _bnode, _var)  \
    do {                                    \
        STATS_INC((_bdev)->stats, _var);    \
    } while (0)
#endif
#else
#define BUS_STATS_INC(_bdev, _bnode, _var)  \
    do {                                    \
    } while (0)
#endif

static int
bus_node_open_func(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct bus_node *bnode = (struct bus_node *)odev;

    BUS_DEBUG_VERIFY_NODE(bnode);

    if (!bnode->callbacks.open) {
        return 0;
    }

    /*
     * XXX current os_dev implementation is prone to races since reference
     * counting is done without any locking, we'll need to fix it there
     */

    /* Call open callback if opening first ref */
    if (odev->od_open_ref == 0) {
        bnode->callbacks.open(bnode);
    }

    return 0;
}

static int
bus_node_close_func(struct os_dev *odev)
{
    struct bus_node *bnode = (struct bus_node *)odev;

    BUS_DEBUG_VERIFY_NODE(bnode);

    if (!bnode->callbacks.close) {
        return 0;
    }

    /*
     * XXX current os_dev implementation is prone to races since reference
     * counting is done without any locking, we'll need to fix it there
     */

    /* Call close callback if closing last ref */
    if (odev->od_open_ref == 1) {
        bnode->callbacks.close(bnode);
    }

    return 0;
}

void
bus_node_set_callbacks(struct os_dev *node, struct bus_node_callbacks *cbs)
{
    struct bus_node *bnode = (struct bus_node *)node;

    /* This should be done only once so all callbacks should be NULL here */
    assert(bnode->callbacks.init == NULL);
    assert(bnode->callbacks.open == NULL);
    assert(bnode->callbacks.close == NULL);

    bnode->callbacks = *cbs;
}

int
bus_dev_init_func(struct os_dev *odev, void *arg)
{
    struct bus_dev *bdev = (struct bus_dev *)odev;
    struct bus_dev_ops *ops = arg;
#if MYNEWT_VAL(BUS_STATS)
    char *stats_name;
#endif

    BUS_DEBUG_POISON_DEV(bdev);

    bdev->dops = ops;
    bdev->configured_for = NULL;

    os_mutex_init(&bdev->lock);

#if MYNEWT_VAL(BUS_STATS)
    asprintf(&stats_name, "bd_%s", odev->od_name);
    /* XXX should we assert or return error on failure? */
    stats_init_and_reg(STATS_HDR(bdev->stats),
                       STATS_SIZE_INIT_PARMS(bdev->stats, STATS_SIZE_32),
                       STATS_NAME_INIT_PARMS(bus_stats_section),
                       stats_name);
#endif

    if (bdev->dops->enable) {
        bdev->dops->enable(bdev);
    }

    return 0;
}

int
bus_node_init_func(struct os_dev *odev, void *arg)
{
    struct bus_node *bnode = (struct bus_node *)odev;
    struct bus_node_cfg *node_cfg = arg;
    struct os_dev *parent_odev;
    struct bus_dev *bdev;
    void *init_arg;
#if MYNEWT_VAL(BUS_STATS_PER_NODE)
    char *stats_name;
#endif
    int rc;

    parent_odev = os_dev_lookup(node_cfg->bus_name);
    if (!parent_odev) {
        return OS_EINVAL;
    }

    BUS_DEBUG_POISON_NODE(bnode);

    /* We need to save init_arg here since it will be overwritten by parent_bus */
    init_arg = bnode->init_arg;
    bnode->parent_bus = (struct bus_dev *)parent_odev;

    bdev = (struct bus_dev *)parent_odev;
    rc = bdev->dops->init_node(bdev, bnode, arg);
    if (rc) {
        return rc;
    }

    if (node_cfg->lock_timeout_ms) {
        bnode->lock_timeout = os_time_ms_to_ticks32(node_cfg->lock_timeout_ms);
    } else {
        /* Use default */
        bnode->lock_timeout = 0;
    }

    odev->od_handlers.od_open = bus_node_open_func;
    odev->od_handlers.od_close = bus_node_close_func;

#if MYNEWT_VAL(BUS_STATS_PER_NODE)
    asprintf(&stats_name, "bn_%s", odev->od_name);
    /* XXX should we assert or return error on failure? */
    stats_init_and_reg(STATS_HDR(bnode->stats),
                       STATS_SIZE_INIT_PARMS(bnode->stats, STATS_SIZE_32),
                       STATS_NAME_INIT_PARMS(bus_stats_section),
                       stats_name);
#endif

    if (bnode->callbacks.init) {
        bnode->callbacks.init(bnode, init_arg);
    }

    return 0;
}

int
bus_node_read(struct os_dev *node, void *buf, uint16_t length,
              os_time_t timeout, uint16_t flags)
{
    struct bus_node *bnode = (struct bus_node *)node;
    struct bus_dev *bdev = bnode->parent_bus;
    int rc;

    BUS_DEBUG_VERIFY_DEV(bdev);
    BUS_DEBUG_VERIFY_NODE(bnode);

    if (!bdev->dops->read) {
        return SYS_ENOTSUP;
    }

    rc = bus_node_lock(node, bus_node_get_lock_timeout(node));
    if (rc) {
        return rc;
    }

    BUS_STATS_INC(bdev, bnode, read_ops);
    rc = bdev->dops->read(bdev, bnode, buf, length, timeout, flags);
    if (rc) {
        BUS_STATS_INC(bdev, bnode, read_errors);
    }

    (void)bus_node_unlock(node);

    return rc;
}

int
bus_node_write(struct os_dev *node, const void *buf, uint16_t length,
               os_time_t timeout, uint16_t flags)
{
    struct bus_node *bnode = (struct bus_node *)node;
    struct bus_dev *bdev = bnode->parent_bus;
    int rc;

    BUS_DEBUG_VERIFY_DEV(bdev);
    BUS_DEBUG_VERIFY_NODE(bnode);

    if (!bdev->dops->write) {
        return SYS_ENOTSUP;
    }

    rc = bus_node_lock(node, bus_node_get_lock_timeout(node));
    if (rc) {
        return rc;
    }

    BUS_STATS_INC(bdev, bnode, write_ops);
    rc = bdev->dops->write(bdev, bnode, buf, length, timeout, flags);
    if (rc) {
        BUS_STATS_INC(bdev, bnode, write_errors);
    }

    (void)bus_node_unlock(node);

    return rc;
}

int
bus_node_write_read_transact(struct os_dev *node, const void *wbuf,
                             uint16_t wlength, void *rbuf, uint16_t rlength,
                             os_time_t timeout, uint16_t flags)
{
    struct bus_node *bnode = (struct bus_node *)node;
    struct bus_dev *bdev = bnode->parent_bus;
    int rc;

    BUS_DEBUG_VERIFY_DEV(bdev);
    BUS_DEBUG_VERIFY_NODE(bnode);

    if (!bdev->dops->write || !bdev->dops->read) {
        return SYS_ENOTSUP;
    }

    rc = bus_node_lock(node, bus_node_get_lock_timeout(node));
    if (rc) {
        return rc;
    }

    /*
     * XXX we probably should pass flags here but with some of them stripped,
     * e.g. BUS_F_NOSTOP should not be present here, but since we do not have
     * too many flags now (like we literally have only one flag) let's just pass
     * no flags for now
     */
    BUS_STATS_INC(bdev, bnode, write_ops);
    rc = bdev->dops->write(bdev, bnode, wbuf, wlength, timeout, BUS_F_NOSTOP);
    if (rc) {
        BUS_STATS_INC(bdev, bnode, write_errors);
        goto done;
    }

    BUS_STATS_INC(bdev, bnode, read_ops);
    rc = bdev->dops->read(bdev, bnode, rbuf, rlength, timeout, flags);
    if (rc) {
        BUS_STATS_INC(bdev, bnode, read_errors);
        goto done;
    }

done:
    (void)bus_node_unlock(node);

    return rc;
}


int
bus_node_lock(struct os_dev *node, os_time_t timeout)
{
    struct bus_node *bnode = (struct bus_node *)node;
    struct bus_dev *bdev = bnode->parent_bus;
    os_error_t err;
    int rc;

    BUS_DEBUG_VERIFY_DEV(bdev);
    BUS_DEBUG_VERIFY_NODE(bnode);

    if (timeout == BUS_NODE_LOCK_DEFAULT_TIMEOUT) {
        timeout = g_bus_node_lock_timeout;
    }

    err = os_mutex_pend(&bdev->lock, timeout);
    if (err == OS_TIMEOUT) {
        BUS_STATS_INC(bdev, bnode, lock_timeouts);
        return SYS_ETIMEOUT;
    }

    assert(err == OS_OK || err == OS_NOT_STARTED);

    /* No need to configure if already configured for the same node */
    if (bdev->configured_for == bnode) {
        return 0;
    }

    /*
     * Configuration is done on 1st lock so in case we need to configure device
     * on nested lock it means that most likely bus device was locked for one
     * node and then access is done on another node which is not correct.
     */
    if (os_mutex_get_level(&bdev->lock) != 1) {
        (void)bus_node_unlock(node);
        return SYS_EACCES;
    }

    rc = bdev->dops->configure(bdev, bnode);
    if (rc) {
        bdev->configured_for = NULL;
        (void)bus_node_unlock(node);
    } else {
        bdev->configured_for = bnode;
    }

    return rc;
}

int
bus_node_unlock(struct os_dev *node)
{
    struct bus_node *bnode = (struct bus_node *)node;
    struct bus_dev *bdev = bnode->parent_bus;
    os_error_t err;

    BUS_DEBUG_VERIFY_DEV(bdev);
    BUS_DEBUG_VERIFY_NODE(bnode);

    err = os_mutex_release(&bdev->lock);

    /*
     * Probably no one cares about return value from unlock, so for debugging
     * purposes let's assert on anything that is not a success. This includes
     * OS_INVALID_PARM (we basically can't pass invalid mutex here unless our
     * structs are broken) and OS_BAD_MUTEX (unlock shall be only done by the
     * same task which locked it).
     */
    assert(err == OS_OK || err == OS_NOT_STARTED);

    return 0;
}

os_time_t
bus_node_get_lock_timeout(struct os_dev *node)
{
    struct bus_node *bnode = (struct bus_node *)node;

    return bnode->lock_timeout ? bnode->lock_timeout : g_bus_node_lock_timeout;
}

void
bus_pkg_init(void)
{
    uint32_t lock_timeout_ms;

    lock_timeout_ms = MYNEWT_VAL(BUS_DEFAULT_LOCK_TIMEOUT_MS);

    g_bus_node_lock_timeout = os_time_ms_to_ticks32(lock_timeout_ms);
}
