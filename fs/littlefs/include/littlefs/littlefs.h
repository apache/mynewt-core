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

#ifndef _LITTLEFS_LITTLEFS_H
#define _LITTLEFS_LITTLEFS_H

/**
 * Formats littlefs filesystem
 *
 * @return FS_EOK on success
 *         FS_* on error (translated from lfs error)
 */
int littlefs_format(void);

/**
 * Mounts littlefs filesystem
 *
 * This shall be called from application if auto-mount is disabled before
 * file system can be accessed.
 *
 * @return FS_EOK on success
 *         FS_* on error (translated from lfs error)
 */
int littlefs_mount(void);

#endif /* _LITTLEFS_LITTLEFS_H */
