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

/**
 * This file implements a simple in-RAM key database for long-term keys.  A key
 * is inserted into the database immediately after a successful pairing
 * procedure.  A key is retrieved from the database when the central performs
 * the encryption procedure (bonding).
 *
 * As this database is only stored in RAM, its contents are lost if bleprph is
 * restarted.
 */

#include <inttypes.h>
#include <string.h>

#include "console/console.h"
#include "host/ble_hs.h"

#include "bletiny.h"

#define STORE_MAX_SLV_LTKS   4
#define STORE_MAX_MST_LTKS   4
#define STORE_MAX_CCCDS      16

static struct ble_store_value_sec store_slv_secs[STORE_MAX_SLV_LTKS];
static int store_num_slv_secs;

static struct ble_store_value_sec store_mst_secs[STORE_MAX_MST_LTKS];
static int store_num_mst_secs;

static struct ble_store_value_cccd store_cccds[STORE_MAX_CCCDS];
static int store_num_cccds;

/*****************************************************************************
 * $sec                                                                      *
 *****************************************************************************/

static void
store_print_value_sec(struct ble_store_value_sec *sec)
{
    if (sec->ltk_present) {
        console_printf("ediv=%u rand=%llu authenticated=%d ltk=",
                       sec->ediv, sec->rand_num, sec->authenticated);
        print_bytes(sec->ltk, 16);
        console_printf(" ");
    }
    if (sec->irk_present) {
        console_printf("irk=");
        print_bytes(sec->irk, 16);
        console_printf(" ");
    }
    if (sec->csrk_present) {
        console_printf("csrk=");
        print_bytes(sec->csrk, 16);
        console_printf(" ");
    }

    console_printf("\n");
}

static void
store_print_key_sec(struct ble_store_key_sec *key_sec)
{
    if (key_sec->peer_addr_type != BLE_STORE_ADDR_TYPE_NONE) {
        console_printf("peer_addr_type=%d peer_addr=",
                       key_sec->peer_addr_type);
        print_bytes(key_sec->peer_addr, 6);
        console_printf(" ");
    }
    if (key_sec->ediv_rand_present) {
        console_printf("ediv=0x%02x rand=0x%llx ",
                       key_sec->ediv, key_sec->rand_num);
    }
}

static int
store_find_sec(struct ble_store_key_sec *key_sec,
               struct ble_store_value_sec *value_secs, int num_value_secs)
{
    struct ble_store_value_sec *cur;
    int i;

    for (i = 0; i < num_value_secs; i++) {
        cur = value_secs + i;

        if (key_sec->peer_addr_type != BLE_STORE_ADDR_TYPE_NONE) {
            if (cur->peer_addr_type != key_sec->peer_addr_type) {
                continue;
            }

            if (memcmp(cur->peer_addr, key_sec->peer_addr,
                       sizeof cur->peer_addr) != 0) {
                continue;
            }
        }

        if (key_sec->ediv_rand_present) {
            if (cur->ediv != key_sec->ediv) {
                continue;
            }

            if (cur->rand_num != key_sec->rand_num) {
                continue;
            }
        }

        return i;
    }

    return -1;
}

static int
store_read_slv_sec(struct ble_store_key_sec *key_sec,
                   struct ble_store_value_sec *value_sec)
{
    int idx;

    idx = store_find_sec(key_sec, store_slv_secs, store_num_slv_secs);
    if (idx == -1) {
        return BLE_HS_ENOENT;
    }

    *value_sec = store_slv_secs[idx];
    return 0;
}

static int
store_write_slv_sec(struct ble_store_value_sec *value_sec)
{
    struct ble_store_key_sec key_sec;
    int idx;

    console_printf("persisting slv sec; ");
    store_print_value_sec(value_sec);

    ble_store_key_from_value_sec(&key_sec, value_sec);
    idx = store_find_sec(&key_sec, store_slv_secs, store_num_slv_secs);
    if (idx == -1) {
        if (store_num_slv_secs >= STORE_MAX_SLV_LTKS) {
            console_printf("error persisting slv sec; too many entries (%d)\n",
                           store_num_slv_secs);
            return BLE_HS_ENOMEM;
        }

        idx = store_num_slv_secs;
        store_num_slv_secs++;
    }

    store_slv_secs[idx] = *value_sec;
    return 0;
}

static int
store_read_mst_sec(struct ble_store_key_sec *key_sec,
                   struct ble_store_value_sec *value_sec)
{
    int idx;

    idx = store_find_sec(key_sec, store_mst_secs, store_num_mst_secs);
    if (idx == -1) {
        return BLE_HS_ENOENT;
    }

    *value_sec = store_mst_secs[idx];
    return 0;
}

static int
store_write_mst_sec(struct ble_store_value_sec *value_sec)
{
    struct ble_store_key_sec key_sec;
    int idx;

    console_printf("persisting mst sec; ");
    store_print_value_sec(value_sec);

    ble_store_key_from_value_sec(&key_sec, value_sec);
    idx = store_find_sec(&key_sec, store_mst_secs, store_num_mst_secs);
    if (idx == -1) {
        if (store_num_mst_secs >= STORE_MAX_MST_LTKS) {
            console_printf("error persisting mst sec; too many entries "
                           "(%d)\n", store_num_mst_secs);
            return BLE_HS_ENOMEM;
        }

        idx = store_num_mst_secs;
        store_num_mst_secs++;
    }

    store_mst_secs[idx] = *value_sec;
    return 0;
}

/*****************************************************************************
 * $cccd                                                                     *
 *****************************************************************************/

static int
store_find_cccd(struct ble_store_key_cccd *key)
{
    struct ble_store_value_cccd *cccd;
    int skipped;
    int i;

    skipped = 0;
    for (i = 0; i < store_num_cccds; i++) {
        cccd = store_cccds + i;

        if (key->peer_addr_type != BLE_STORE_ADDR_TYPE_NONE) {
            if (cccd->peer_addr_type != key->peer_addr_type) {
                continue;
            }

            if (memcmp(cccd->peer_addr, key->peer_addr, 6) != 0) {
                continue;
            }
        }

        if (key->chr_val_handle != 0) {
            if (cccd->chr_val_handle != key->chr_val_handle) {
                continue;
            }
        }

        if (key->idx > skipped) {
            skipped++;
            continue;
        }

        return i;
    }

    return -1;
}

static int
store_read_cccd(struct ble_store_key_cccd *key_cccd,
                struct ble_store_value_cccd *value_cccd)
{
    int idx;

    idx = store_find_cccd(key_cccd);
    if (idx == -1) {
        return BLE_HS_ENOENT;
    }

    *value_cccd = store_cccds[idx];
    return 0;
}

static int
store_write_cccd(struct ble_store_value_cccd *value_cccd)
{
    struct ble_store_key_cccd key_cccd;
    int idx;

    ble_store_key_from_value_cccd(&key_cccd, value_cccd);
    idx = store_find_cccd(&key_cccd);
    if (idx == -1) {
        if (store_num_cccds >= STORE_MAX_SLV_LTKS) {
            console_printf("error persisting cccd; too many entries (%d)\n",
                           store_num_cccds);
            return BLE_HS_ENOMEM;
        }

        idx = store_num_cccds;
        store_num_cccds++;
    }

    store_cccds[idx] = *value_cccd;
    return 0;
}

/*****************************************************************************
 * $api                                                                      *
 *****************************************************************************/

/**
 * Searches the database for an object matching the specified criteria.
 *
 * @return                      0 if a key was found; else BLE_HS_ENOENT.
 */
int
store_read(int obj_type, union ble_store_key *key,
           union ble_store_value *value)
{
    int rc;

    switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_MST_SEC:
        /* An encryption procedure (bonding) is being attempted.  The nimble
         * stack is asking us to look in our key database for a long-term key
         * corresponding to the specified ediv and random number.
         *
         * Perform a key lookup and populate the context object with the
         * result.  The nimble stack will use this key if this function returns
         * success.
         */
        console_printf("looking up mst sec; ");
        store_print_key_sec(&key->sec);
        console_printf("\n");
        rc = store_read_mst_sec(&key->sec, &value->sec);
        return rc;

    case BLE_STORE_OBJ_TYPE_SLV_SEC:
        console_printf("looking up slv sec; ");
        store_print_key_sec(&key->sec);
        console_printf("\n");
        rc = store_read_slv_sec(&key->sec, &value->sec);
        return rc;

    case BLE_STORE_OBJ_TYPE_CCCD:
        rc = store_read_cccd(&key->cccd, &value->cccd);
        return rc;

    default:
        return BLE_HS_ENOTSUP;
    }
}

/**
 * Adds the specified object to the database.
 *
 * @return                      0 on success; BLE_HS_ENOMEM if the database is
 *                                  full.
 */
int
store_write(int obj_type, union ble_store_value *val)
{
    int rc;

    switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_MST_SEC:
        rc = store_write_mst_sec(&val->sec);
        return rc;

    case BLE_STORE_OBJ_TYPE_SLV_SEC:
        rc = store_write_slv_sec(&val->sec);
        return rc;

    case BLE_STORE_OBJ_TYPE_CCCD:
        rc = store_write_cccd(&val->cccd);
        return rc;

    default:
        return BLE_HS_ENOTSUP;
    }
}
