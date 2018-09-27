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

#ifndef _LOG_ASYNC_H
#define _LOG_ASYNC_H

#include <inttypes.h>
#include <syscfg/syscfg.h>
#include <log/log.h>

/*
 * Initialize log data for log_async handler
 *
 * Given log structure with synchronous handler, redirects handler pointer
 * to asynchronous one.
 * Functions creates mempool, and task for handling logs in the background.
 *
 * @param log  Already initialized log with synchronous handler
 *
 */
int log_switch_to_async(struct log *log);

#endif /* _LOG_ASYNC_H */
