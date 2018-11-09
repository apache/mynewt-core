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

#ifndef H_STATS_PRIV_
#define H_STATS_PRIV_

#ifdef __cplusplus
extern "C" {
#endif

struct stats_hdr;

/**
 * @brief Calculates the total size, in bytes, of all stats within the
 * specified group.
 *
 * The result only includes the statistics; it does not include the stats
 * header.
 *
 * @param hdr                   The stat group to examine.
 *
 * @return                      The aggregate stat group size.
 */
size_t stats_size(const struct stats_hdr *hdr);

/**
 * @brief Retrieves the address of the first stat in the specified group.
 *
 * Stats within a group are contiguous, so the result of a call to this
 * function points to the entire block of stats.
 *
 * @param hdr                   The stat group to pull data from.
 */
void *stats_data(const struct stats_hdr *hdr);

/**
 * @brief Writes the specified stat group to sys/config.
 *
 * If the provided stat group is non-persistent, this function is a no-op.
 *
 * @param hdr                   The stat group to persist.
 */
int stats_conf_save_group(const struct stats_hdr *hdr);

/**
 * @brief Performs a sanity check on the provided persistent stat group.
 *
 * Verifies that the buffer sizes configured in syscfg are sufficient for the
 * specified stat group.  This function asserts that an attempt to persist the
 * stat group would not fail due to buffer exhaustion.
 *
 * @param hdr                   The stat group to verify.
 */
void stats_conf_assert_valid(const struct stats_hdr *hdr);

#ifdef __cplusplus
}
#endif

#endif
