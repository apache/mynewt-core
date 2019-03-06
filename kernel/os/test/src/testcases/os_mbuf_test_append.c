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
#include "os_test_priv.h"

TEST_CASE_SELF(os_mbuf_test_append)
{
    struct os_mbuf *om;
    int rc;
    uint8_t databuf[] = {0xa, 0xb, 0xc, 0xd};
    uint8_t cmpbuf[] = {0xff, 0xff, 0xff, 0xff};

    os_mbuf_test_setup();

    om = os_mbuf_get(&os_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL, "Error allocating mbuf");
    os_mbuf_test_misc_assert_sane(om, NULL, 0, 0, 0);

    rc = os_mbuf_append(om, databuf, sizeof(databuf));
    TEST_ASSERT_FATAL(rc == 0, "Cannot add %d bytes to mbuf",
            sizeof(databuf));
    os_mbuf_test_misc_assert_sane(om, databuf, sizeof databuf, sizeof databuf,
                                  0);

    memcpy(cmpbuf, OS_MBUF_DATA(om, uint8_t *), om->om_len);
    TEST_ASSERT_FATAL(memcmp(cmpbuf, databuf, sizeof(databuf)) == 0,
            "Databuf doesn't match cmpbuf");
}
