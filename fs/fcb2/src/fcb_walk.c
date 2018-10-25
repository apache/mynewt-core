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

#include "fcb/fcb.h"
#include "fcb_priv.h"

int
fcb_get_sector_loc(const struct fcb *fcb, int sector, struct fcb_entry *entry)
{
    struct fcb_sector_info info;
    int rc = 0;

    rc = fcb_get_sector_info(fcb, sector, &info);
    if (rc == 0) {
        entry->fe_range = info.si_range;
        entry->fe_sector = info.si_sector_in_range +
            info.si_range->fsr_first_sector;
    }

    return rc;
}

/*
 * Call 'cb' for every element in flash circular buffer. If fap is specified,
 * only elements with that flash_area are reported.
 */
int
fcb_walk(struct fcb *fcb, int sector, fcb_walk_cb cb, void *cb_arg)
{
    struct fcb_entry loc;
    int rc;

    fcb_get_sector_loc(fcb, sector, &loc);
    loc.fe_data_off = 0;
    loc.fe_entry_num = 0;

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB_ERR_ARGS;
    }
    while ((rc = fcb_getnext_nolock(fcb, &loc)) != FCB_ERR_NOVAR) {
        os_mutex_release(&fcb->f_mtx);
        if (sector != FCB_SECTOR_OLDEST && loc.fe_sector != sector) {
            return 0;
        }
        rc = cb(&loc, cb_arg);
        if (rc) {
            return rc;
        }
        os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    }
    os_mutex_release(&fcb->f_mtx);
    return 0;
}
