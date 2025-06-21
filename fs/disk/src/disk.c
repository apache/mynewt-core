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

#include <inttypes.h>
#include <os/mynewt.h>
#include <os/link_tables.h>
#include <disk/disk.h>

#define DISK_EOK          0  /* Success */
#define DISK_EHW          1  /* Error accessing storage medium */
#define DISK_ENOMEM       2  /* Insufficient memory */
#define DISK_ENOENT       3  /* No such file or directory */
#define DISK_EOS          4  /* OS error */
#define DISK_EUNINIT      5  /* File system not initialized */

int
mn_disk_inserted(disk_t *disk)
{
    int count = LINK_TABLE_SIZE(disk_listeners);

    for (int i = 0; i < count; ++i) {
        disk_listener_t *listener = disk_listeners[i];
        listener->ops->disk_added(listener, disk);
    }
    return 0;
}

int
mn_disk_ejected(disk_t *disk)
{
    int count = LINK_TABLE_SIZE(disk_listeners);

    for (int i = 0; i < count; ++i) {
        disk_listener_t *listener = disk_listeners[i];
        listener->ops->disk_removed(listener, disk);
    }
    return 0;
}
