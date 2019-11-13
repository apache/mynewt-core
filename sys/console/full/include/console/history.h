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
#ifndef __SYS_CONSOLE_FULL_HISTORY_H__
#define __SYS_CONSOLE_FULL_HISTORY_H__

/**
 * History search types for console_history_find()
 */
typedef enum history_find_type {
    /* Find previous entry */
    HFT_PREV                = 0,
    /* Find next entry */
    HFT_NEXT                = 1,
    /* Find oldest entry */
    HFT_LAST                = 2,
    HFT_MATCH               = 4,
    /* Find previous entry matching string pointed by arg */
    HFT_MATCH_PREV          = HFT_MATCH | HFT_PREV,
    /* Find next entry matching string pointed by arg */
    HFT_MATCH_NEXT          = HFT_MATCH | HFT_NEXT,
} history_find_type_t;

typedef intptr_t history_handle_t;

/**
 * Function adds line to history.
 *
 * Implemented by history provider.
 *
 * @param line text to add to history
 *
 * @return handle to new entry added to history
 *         SYS_EINVAL - line was empty or null
 *         SYS_EALREADY - line was already at the end of history
 */
history_handle_t console_history_add(const char *line);

/**
 * Finds element in history.
 *
 * Implemented by history provider.
 *
 * @param start starting point for search 0 start from the beginning
 * @param search_type search method
 * @param arg for HFT_PREV, HFT_NEXT can be a pointer to int with
 *            requested move size (default 1)
 *            for HFT_MATCH_PREV, HFT_MATCH_NEXT points to the null
 *            terminated string to match history line.
 * @return handle to entry that matched or 0 if entry can't be found.
 */
history_handle_t console_history_find(history_handle_t start,
                                      history_find_type_t search_type,
                                      void *arg);

/**
 * Get data from history element found by console_history_find()
 *
 * Implemented by history provider.
 *
 * @param handle  handle to entry to get data from
 * @param offset  offset of data in element to get data from
 * @param buf     buffer to fill with data from element
 * @param buf_size size of buf
 *
 * @return number of characters written to buf, or SYS_Exxx if
 *                there is an error.
 */
int console_history_get(history_handle_t handle, size_t offset, char *buf,
                        size_t buf_size);

#endif /* __SYS_CONSOLE_FULL_HISTORY_H__ */
