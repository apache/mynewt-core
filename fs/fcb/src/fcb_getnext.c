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
#include <stddef.h>

#include "fcb/fcb.h"
#include "fcb_priv.h"

int
fcb_getnext_in_area(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;

    rc = fcb_elem_info(fcb, loc);
    if (rc == 0 || rc == FCB_ERR_CRC) {
        do {
            loc->fe_elem_off = loc->fe_data_off +
              fcb_len_in_flash(fcb, loc->fe_data_len) +
              fcb_len_in_flash(fcb, FCB_CRC_SZ);
            rc = fcb_elem_info(fcb, loc);
            if (rc != FCB_ERR_CRC) {
                break;
            }
        } while (rc == FCB_ERR_CRC);
    }
    return rc;
}

struct flash_area *
fcb_getnext_area(struct fcb *fcb, struct flash_area *fap)
{
    fap++;
    if (fap >= &fcb->f_sectors[fcb->f_sector_cnt]) {
        fap = &fcb->f_sectors[0];
    }
    return fap;
}

int
fcb_getnext_nolock(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;

    if (loc->fe_area == NULL) {
        /*
         * Find the first one we have in flash.
         */
        loc->fe_area = fcb->f_oldest;
    }
    if (loc->fe_elem_off == 0) {
        /*
         * If offset is zero, we serve the first entry from the area.
         */
        loc->fe_elem_off = sizeof(struct fcb_disk_area);
        rc = fcb_elem_info(fcb, loc);
    } else {
        rc = fcb_getnext_in_area(fcb, loc);
    }
    switch (rc) {
    case 0:
        return 0;
    case FCB_ERR_CRC:
        break;
    default:
        goto next_sector;
    }
    while (rc == FCB_ERR_CRC) {
        rc = fcb_getnext_in_area(fcb, loc);
        if (rc == 0) {
            return 0;
        }

        if (rc != FCB_ERR_CRC) {
            /*
             * Moving to next sector.
             */
next_sector:
            if (loc->fe_area == fcb->f_active.fe_area) {
                return FCB_ERR_NOVAR;
            }
            loc->fe_area = fcb_getnext_area(fcb, loc->fe_area);
            loc->fe_elem_off = sizeof(struct fcb_disk_area);
            rc = fcb_elem_info(fcb, loc);
            switch (rc) {
            case 0:
                return 0;
            case FCB_ERR_CRC:
                break;
            default:
                goto next_sector;
            }
        }
    }

    return 0;
}

int
fcb_getnext(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB_ERR_ARGS;
    }
    rc = fcb_getnext_nolock(fcb, loc);
    os_mutex_release(&fcb->f_mtx);

    return rc;
}
