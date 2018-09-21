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
fcb_area_info(struct fcb *fcb, struct flash_area *fa, int *elemsp, int *bytesp)
{
    struct fcb_entry loc;
    int rc;
    int elems = 0;
    int bytes = 0;

    loc.fe_area = fa;
    loc.fe_elem_off = 0;

    while (1) {
        rc = fcb_getnext(fcb, &loc);
        if (rc) {
            break;
        }
        if (!fa) {
            fa = loc.fe_area;
        } else if (fa != loc.fe_area) {
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
