/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifdef OC_SECURITY
#include "oc_store.h"
#include "oc_acl.h"
#include "oc_core_res.h"
#include "oc_cred.h"
#include "oc_doxm.h"
#include "oc_pstat.h"
#include "port/oc_storage.h"

void
oc_sec_load_doxm(void)
{
    long ret = 0;
    size_t size = 512;
    uint8_t buf[size];
    oc_rep_t *rep;

    if (oc_sec_provisioned()) {
        ret = oc_storage_read("/doxm", buf, size);
        if (ret > 0) {
            oc_parse_rep(buf, ret, &rep);
            oc_sec_decode_doxm(rep);
            oc_free_rep(rep);
        }
    }

    if (ret <= 0) {
        oc_sec_doxm_default();
    }

    oc_uuid_t *deviceuuid = oc_core_get_device_id(0);
    oc_sec_doxm_t *doxm = oc_sec_get_doxm();
    memcpy(deviceuuid, &doxm->deviceuuid, sizeof(oc_uuid_t));
}

void
oc_sec_load_pstat(void)
{
    long ret = 0;
    size_t size = 512;
    uint8_t buf[size];
    oc_rep_t *rep;

    ret = oc_storage_read("/pstat", buf, size);
    if (ret > 0) {
        oc_parse_rep(buf, ret, &rep);
        oc_sec_decode_pstat(rep);
        oc_free_rep(rep);
    }

    if (ret <= 0) {
        oc_sec_pstat_default();
    }
}

void
oc_sec_load_cred(void)
{
    long ret = 0;
    size_t size = 1024;
    uint8_t buf[size];
    oc_rep_t *rep;

    if (oc_sec_provisioned()) {
        ret = oc_storage_read("/cred", buf, size);

        if (ret <= 0) {
            return;
        }
        oc_parse_rep(buf, ret, &rep);
        oc_sec_decode_cred(rep, NULL);
        oc_free_rep(rep);
    }
}

void
oc_sec_load_acl(void)
{
    size_t size = 1024;
    long ret = 0;
    uint8_t buf[size];
    oc_rep_t *rep;

    oc_sec_acl_init();

    if (oc_sec_provisioned()) {
        ret = oc_storage_read("/acl", buf, size);
        if (ret > 0) {
            oc_parse_rep(buf, ret, &rep);
            oc_sec_decode_acl(rep);
            oc_free_rep(rep);
        }
    }

    if (ret <= 0) {
        oc_sec_acl_default();
    }
}

void
oc_sec_dump_state(void)
{
    uint8_t buf[1024];

    /* pstat */
    oc_rep_new(buf, 1024);
    oc_sec_encode_pstat();
    int size = oc_rep_finalize();
    if (size > 0) {
        LOG("oc_store: encoded pstat size %d\n", size);
        oc_storage_write("/pstat", buf, size);
    }

    /* cred */
    oc_rep_new(buf, 1024);
    oc_sec_encode_cred();
    size = oc_rep_finalize();
    if (size > 0) {
        LOG("oc_store: encoded cred size %d\n", size);
        oc_storage_write("/cred", buf, size);
    }

    /* doxm */
    oc_rep_new(buf, 1024);
    oc_sec_encode_doxm();
    size = oc_rep_finalize();
    if (size > 0) {
        LOG("oc_store: encoded doxm size %d\n", size);
        oc_storage_write("/doxm", buf, size);
    }

    /* acl */
    oc_rep_new(buf, 1024);
    oc_sec_encode_acl();
    size = oc_rep_finalize();
    if (size > 0) {
        LOG("oc_store: encoded ACL size %d\n", size);
        oc_storage_write("/acl", buf, size);
    }
}

#endif /* OC_SECURITY */
