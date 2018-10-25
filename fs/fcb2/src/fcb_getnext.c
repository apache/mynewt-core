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
    int rc = FCB_ERR_CRC;
    int off;
    int len;

    while (rc == FCB_ERR_CRC) {
        len = loc->fe_data_len;
        off = loc->fe_data_off;
        loc->fe_data_len = 0;
        loc->fe_entry_num++;
        rc = fcb_elem_info(loc);
        if (len) {
            loc->fe_data_off = off + fcb_len_in_flash(loc->fe_range, len) +
                fcb_len_in_flash(loc->fe_range, 2);
        }
    }
    return rc;
}

int
fcb_getnext_nolock(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;

    if (loc->fe_range == NULL) {
        /*
         * Find the first one we have in flash.
         */
        loc->fe_sector = fcb->f_oldest_sec;
        loc->fe_range = fcb_get_sector_range(fcb, loc->fe_sector);
    }
    if (loc->fe_entry_num == 0) {
        /*
         * If offset is zero, we serve the first entry from the area.
         */
        loc->fe_entry_num = 1;
        loc->fe_data_off =
            fcb_len_in_flash(loc->fe_range, sizeof(struct fcb_disk_area));
        loc->fe_data_len = 0;
        rc = fcb_elem_info(loc);
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
            if (loc->fe_sector == fcb->f_active.fe_sector) {
                return FCB_ERR_NOVAR;
            }
            loc->fe_sector = fcb_getnext_sector(fcb, loc->fe_sector);
            loc->fe_range = fcb_get_sector_range(fcb, loc->fe_sector);
            loc->fe_entry_num = 1;
            loc->fe_data_off =
                fcb_len_in_flash(loc->fe_range, sizeof(struct fcb_disk_area));
            loc->fe_data_len = 0;
            rc = fcb_elem_info(loc);
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
