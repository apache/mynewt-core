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
fcb_rotate(struct fcb *fcb)
{
    int sector;
    int rc = 0;
    struct flash_sector_range *range;

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB_ERR_ARGS;
    }

    rc = fcb_sector_erase(fcb, fcb->f_oldest_sec);
    if (rc) {
        rc = FCB_ERR_FLASH;
        goto out;
    }
    if (fcb->f_oldest_sec == fcb->f_active.fe_sector) {
        /*
         * Need to create a new active sector, as we're wiping the current.
         */
        sector = fcb_getnext_sector(fcb, fcb->f_oldest_sec);

        rc = fcb_sector_hdr_init(fcb, sector, fcb->f_active_id + 1);
        if (rc) {
            goto out;
        }
        range = fcb_get_sector_range(fcb, sector);
        fcb->f_active.fe_sector = sector;
        fcb->f_active.fe_range = range;
        fcb->f_active.fe_data_off =
            fcb_len_in_flash(range, sizeof(struct fcb_disk_area));
        fcb->f_active.fe_entry_num = 1;
        fcb->f_active.fe_data_len = 0;
        fcb->f_active_id++;
    }
    fcb->f_oldest_sec = fcb_getnext_sector(fcb, fcb->f_oldest_sec);
out:
    os_mutex_release(&fcb->f_mtx);
    return rc;
}
