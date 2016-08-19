/**
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
    struct flash_area *fap;
    int rc = 0;

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB_ERR_ARGS;
    }

    rc = flash_area_erase(fcb->f_oldest, 0, fcb->f_oldest->fa_size);
    if (rc) {
        rc = FCB_ERR_FLASH;
        goto out;
    }
    if (fcb->f_oldest == fcb->f_active.fe_area) {
        /*
         * Need to create a new active area, as we're wiping the current.
         */
        fap = fcb_getnext_area(fcb, fcb->f_oldest);
        rc = fcb_sector_hdr_init(fcb, fap, fcb->f_active_id + 1);
        if (rc) {
            goto out;
        }
        fcb->f_active.fe_area = fap;
        fcb->f_active.fe_elem_off = sizeof(struct fcb_disk_area);
        fcb->f_active_id++;
    }
    fcb->f_oldest = fcb_getnext_area(fcb, fcb->f_oldest);
out:
    os_mutex_release(&fcb->f_mtx);
    return rc;
}
