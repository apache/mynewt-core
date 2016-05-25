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

#include "host/ble_hs.h"

#include "bleprph.h"

#define STORE_MAX_OUR_LTKS   4

static struct ble_store_value_ltk store_our_ltks[STORE_MAX_OUR_LTKS];
static int store_num_our_ltks;

static int
store_find_our_ltk(struct ble_store_key_ltk *key_ltk)
{
    struct ble_store_value_ltk *cur;
    int i;

    for (i = 0; i < store_num_our_ltks; i++) {
        cur = store_our_ltks + i;

        if (cur->ediv == key_ltk->ediv &&
            cur->rand_num == key_ltk->rand_num) {

            return i;
        }
    }

    return -1;
}

static void
store_print_ltk(struct ble_store_value_ltk *ltk)
{
    BLEPRPH_LOG(INFO, "ediv=%u rand=%llu authenticated=%d ", ltk->ediv,
                   ltk->rand_num, ltk->authenticated);
    BLEPRPH_LOG(INFO, "ltk=");
    print_bytes(ltk->key, 16);
    BLEPRPH_LOG(INFO, "\n");
}

/**
 * Searches the database for a long-term key matching the specified criteria.
 *
 * @return                      0 if a key was found; else BLE_HS_ENOENT.
 */
int
store_read(int obj_type, union ble_store_key *key, union ble_store_value *dst)
{
    int idx;

    switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_OUR_LTK:
        /* An encryption procedure (bonding) is being attempted.  The nimble
         * stack is asking us to look in our key database for a long-term key
         * corresponding to the specified ediv and random number.
         */
        BLEPRPH_LOG(INFO, "looking up ltk with ediv=0x%02x rand=0x%llx\n",
                    key->ltk.ediv, key->ltk.rand_num);

        /* Perform a key lookup and populate the context object with the
         * result.  The nimble stack will use this key if this function returns
         * success.
         */
        idx = store_find_our_ltk(&key->ltk);
        if (idx == -1) {
            return BLE_HS_ENOENT;
        }
        dst->ltk = store_our_ltks[idx];
        return 0;

    default:
        return BLE_HS_ENOTSUP;
    }
}

/**
 * Adds the specified key to the database.
 *
 * @return                      0 on success; BLE_HS_ENOMEM if the database is
 *                                  full.
 */
int
store_write(int obj_type, union ble_store_value *val)
{
    struct ble_store_key_ltk key_ltk;
    int idx;

    switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_OUR_LTK:
        BLEPRPH_LOG(INFO, "persisting our ltk; ");
        store_print_ltk(&val->ltk);

        ble_store_key_from_value_ltk(&key_ltk, &val->ltk);
        idx = store_find_our_ltk(&key_ltk);
        if (idx == -1) {
            if (store_num_our_ltks >= STORE_MAX_OUR_LTKS) {
                BLEPRPH_LOG(INFO, "error persisting LTK; too many entries\n");
                return BLE_HS_ENOMEM;
            }

            idx = store_num_our_ltks;
            store_num_our_ltks++;
        }

        store_our_ltks[idx] = val->ltk;
        return 0;

    default:
        return BLE_HS_ENOTSUP;
    }
}
