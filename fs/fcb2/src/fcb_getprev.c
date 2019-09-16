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

#include "fcb/fcb2.h"
#include "fcb_priv.h"

static int
fcb2_sector_find_last(struct fcb2 *fcb, struct fcb2_entry *loc)
{
    int rc;
    int last_valid = 0;

    loc->fe_entry_num = 1;

    do {
        rc = fcb2_elem_info(loc);
        if (rc == 0) {
            last_valid = loc->fe_entry_num;
        }
        if (rc == FCB2_ERR_NOVAR) {
            /*
             * Out entries in this sector
             */
            if (last_valid == 0) {
                loc->fe_entry_num = 1;
                /*
                 * No valid entries in this sector.
                 */
                return FCB2_ERR_NOVAR;
            } else {
                loc->fe_entry_num = last_valid;
                rc = fcb2_elem_info(loc);
                assert(rc == 0); /* must be; just succeeded a bit earlier */
                return rc;
            }
        }
        loc->fe_entry_num++;
    } while (1);
    return rc;
}

int
fcb2_getprev(struct fcb2 *fcb, struct fcb2_entry *loc)
{
    int rc;

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB2_ERR_ARGS;
    }
    if (loc->fe_range == NULL) {
        /*
         * Find the last element.
         */
        *loc = fcb->f_active;
    }
     while (1) {
        loc->fe_entry_num--;
        if (loc->fe_entry_num < 1) {
            /*
             * Need to get from previous sector.
             */
            if (loc->fe_sector == fcb->f_oldest_sec) {
                return FCB2_ERR_NOVAR;
            }
            if (loc->fe_sector == 0) {
                loc->fe_sector = fcb->f_sector_cnt - 1;
            } else {
                loc->fe_sector--;
            }
            loc->fe_range = fcb2_get_sector_range(fcb, loc->fe_sector);
            rc = fcb2_sector_find_last(fcb, loc);
            if (rc == 0) {
                break;
            }
        } else {
            rc = fcb2_elem_info(loc);
            if (rc == 0) {
                break;
            }
        }
    }
    os_mutex_release(&fcb->f_mtx);
    return rc;
}
