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

#include "fcb/fcb2.h"
#include "fcb_priv.h"

int
fcb2_area_info(struct fcb2 *fcb, int sector, int *elemsp, int *bytesp)
{
    struct fcb2_entry loc;
    struct fcb2_sector_info info;
    int rc;
    int elems = 0;
    int bytes = 0;

    rc = fcb2_get_sector_info(fcb, sector, &info);
    if (rc) {
        return rc;
    }
    loc.fe_range = info.si_range;
    loc.fe_sector = info.si_sector_in_range + loc.fe_range->fsr_first_sector;
    loc.fe_entry_num = 0;
    /* In case caller passed oldest, get real sector number */
    if (sector == FCB2_SECTOR_OLDEST) {
        sector = loc.fe_sector;
    }

    while (1) {
        rc = fcb2_getnext(fcb, &loc);
        if (rc) {
            break;
        }
        if (sector != loc.fe_sector) {
            break;
        }
        elems++;
        bytes += loc.fe_data_len;
    }
    if (elemsp) {
        *elemsp = elems;
    }
    if (bytesp) {
        *bytesp = bytes;
    }
    return 0;
}
