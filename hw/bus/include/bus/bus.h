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

#ifndef HW_BUS_H_
#define HW_BUS_H_

#include <stdint.h>
#include "os/os_dev.h"
#include "os/os_mutex.h"
#include "os/os_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Flags used for bus operations
 */
#define BUS_F_NONE          0
#define BUS_F_NOSTOP        0x0001

/**
 * Read data from node
 *
 * Reads data from node. Bus is locked automatically for the duration of
 * operation.
 *
 * The timeout parameter applies to complete transaction time, including
 * locking the bus.
 *
 * @param node     Node device object
 * @param buf      Buffer to read data into
 * @param length   Length of data to be read
 * @param timeout  Operation timeout
 * @param flags    Flags
 *
 * @return 0 on success, SYS_xxx on error
 */
int
bus_node_read(struct os_dev *node, void *buf, uint16_t length,
              os_time_t timeout, uint16_t flags);

/**
 * Write data to node
 *
 * Writes data to node. Bus is locked automatically for the duration of
 * operation.
 *
 * The timeout parameter applies to complete transaction time, including
 * locking the bus.
 *
 * @param node     Node device object
 * @param buf      Buffer with data to be written
 * @param length   Length of data to be written
 * @param timeout  Operation timeout
 * @param flags    Flags
 *
 * @return 0 on success, SYS_xxx on error
 */
int
bus_node_write(struct os_dev *node, const void *buf, uint16_t length,
               os_time_t timeout, uint16_t flags);

/**
 * Perform write and read transaction on node
 *
 * Writes data to node and automatically reads response afterwards. This is a
 * convenient shortcut for a generic write-then-read operation used to read data
 * from devices which is executed atomically (i.e. with bus lock held during
 * entire transaction).
 *
 * The timeout parameter applies to complete transaction time.
 *
 * @param node     Node device object
 * @param wbuf     Buffer with data to be written
 * @param wlength  Length of data to be written
 * @param rbuf     Buffer to read data into
 * @param rlength  Length of data to be read
 * @param timeout  Operation timeout
 * @param flags    Flags
 *
 * @return 0 on success, SYS_xxx on error
 */
int
bus_node_write_read_transact(struct os_dev *node, const void  *wbuf,
                             uint16_t wlength, void *rbuf, uint16_t rlength,
                             os_time_t timeout, uint16_t flags);

/**
 * Read data from node
 *
 * This is simple version of bus_node_read() with default timeout and no flags.
 *
 * @param node     Node device object
 * @param buf      Buffer to read data into
 * @param length   Length of data to be read
 *
 * @return 0 on success, SYS_xxx on error
 */
static inline int
bus_node_simple_read(struct os_dev *node, void *buf, uint16_t length)
{
    return bus_node_read(node, buf, length, OS_TIMEOUT_NEVER, BUS_F_NONE);
}

/**
 * Write data to node
 *
 * This is simple version of bus_node_write() with default timeout and no flags.
 *
 * @param node     Node device object
 * @param buf      Buffer with data to be written
 * @param length   Length of data to be written
 *
 * @return 0 on success, SYS_xxx on error
 */
static inline int
bus_node_simple_write(struct os_dev *node, const void *buf, uint16_t length)
{
    return bus_node_write(node, buf, length, OS_TIMEOUT_NEVER, BUS_F_NONE);
}

/**
 * Perform write and read transaction on node
 *
 * This is simple version of bus_node_write_read_transact() with default timeout
 * and no flags.
 *
 * @param node     Node device object
 * @param wbuf     Buffer with data to be written
 * @param wlength  Length of data to be written
 * @param rbuf     Buffer to read data into
 * @param rlength  Length of data to be read
 *
 * @return 0 on success, SYS_xxx on error
 */
static inline int
bus_node_simple_write_read_transact(struct os_dev *node, const void *wbuf,
                                    uint16_t wlength, void *rbuf,
                                    uint16_t rlength)
{
    return bus_node_write_read_transact(node, wbuf, wlength, rbuf, rlength,
                                        OS_TIMEOUT_NEVER, BUS_F_NONE);
}

/**
 * Get lock object for bus
 *
 * \deprecated
 * This API is only provided for compatibility with legacy drivers where locking
 * is provided by Sensors interface. To lock bus for compound transactions use
 * bus_dev_lock() and bus_dev_unlock() instead.
 *
 * @param bus    Bus device object
 * @param mutex  Mutex used for bus lock
 *
 * return 0 on success, SYS_xxx on error
 */
int
bus_dev_get_lock(struct os_dev *bus, struct os_mutex **mutex);

/**
 * Lock bus for exclusive access
 *
 * Locks bus for exclusive access. The parent bus of given node (i.e. bus where
 * this node is attached) will be locked. This should be only used for compound
 * transactions where bus shall be locked for the duration of entire transaction.
 * Simple operations like read, write or write-read (i.e. those with dedicated
 * APIs) lock bus automatically.
 *
 * After successful locking, bus is configured to be used with given node.
 *
 * @param node     Node to lock its parent bus
 * @param timeout  Timeout on locking attempt
 *
 * return 0 on success
 *        SYS_ETIMEOUT on lock timeout
 */
int
bus_node_lock(struct os_dev *node, os_time_t timeout);

/**
 * Unlock bus node
 *
 * Unlocks exclusive bus access. This shall be only used when bus was previously
 * locked with bus_node_lock(). API operations which lock bus will also unlock
 * bus automatically.
 *
 * @param node  Node to unlock its parent bus
 *
 * return 0 on success
 *        SYS_EACCESS when bus was not locked by current task
 */
int
bus_node_unlock(struct os_dev *node);

#ifdef __cplusplus
}
#endif

#endif /* HW_BUS_H_ */
