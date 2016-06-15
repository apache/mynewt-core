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

#define KEYSTORE_MAX_ENTRIES   4

struct keystore_entry {
    uint64_t rand_num;
    uint16_t ediv;
    uint8_t ltk[16];

    unsigned authenticated:1;

    /* XXX: authreq. */
};

static struct keystore_entry keystore_entries[KEYSTORE_MAX_ENTRIES];
static int keystore_num_entries;

/**
 * Searches the database for a long-term key matching the specified criteria.
 *
 * @return                      0 if a key was found; else BLE_HS_ENOENT.
 */
int
keystore_lookup(uint16_t ediv, uint64_t rand_num,
                void *out_ltk, int *out_authenticated)
{
    struct keystore_entry *entry;
    int i;

    for (i = 0; i < keystore_num_entries; i++) {
        entry = keystore_entries + i;

        if (entry->ediv == ediv && entry->rand_num == rand_num) {
            memcpy(out_ltk, entry->ltk, sizeof entry->ltk);
            *out_authenticated = entry->authenticated;

            return 0;
        }
    }

    return BLE_HS_ENOENT;
}

/**
 * Adds the specified key to the database.
 *
 * @return                      0 on success; BLE_HS_ENOMEM if the database is
 *                                  full.
 */
int
keystore_add(uint16_t ediv, uint64_t rand_num, uint8_t *ltk, int authenticated)
{
    struct keystore_entry *entry;

    if (keystore_num_entries >= KEYSTORE_MAX_ENTRIES) {
        return BLE_HS_ENOMEM;
    }

    entry = keystore_entries + keystore_num_entries;
    keystore_num_entries++;

    entry->ediv = ediv;
    entry->rand_num = rand_num;
    memcpy(entry->ltk, ltk, sizeof entry->ltk);
    entry->authenticated = authenticated;

    return 0;
}
