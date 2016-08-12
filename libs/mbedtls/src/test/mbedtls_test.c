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
#include <stdio.h>
#include <string.h>

#include <testutil/testutil.h>

#include "mbedtls/mbedtls_test.h"

#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/aes.h"
#include "mbedtls/arc4.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ccm.h"
#include "mbedtls/dhm.h"
#include "mbedtls/ecjpake.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/gcm.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/md5.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/ripemd160.h"
#include "mbedtls/rsa.h"
#include "mbedtls/x509.h"
#include "mbedtls/xtea.h"

TEST_CASE(sha1_test)
{
    int rc;

    rc = mbedtls_sha1_self_test(0);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(sha256_test)
{
    int rc;

    rc = mbedtls_sha256_self_test(0);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(sha512_test)
{
    int rc;

    rc = mbedtls_sha512_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(aes_test)
{
    int rc;

    rc = mbedtls_aes_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(arc4_test)
{
    int rc;

    rc = mbedtls_arc4_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(bignum_test)
{
    int rc;

    rc = mbedtls_mpi_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(ccm_test)
{
    int rc;

    rc = mbedtls_ccm_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(dhm_test)
{
    int rc;

    rc = mbedtls_dhm_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(ecp_test)
{
    int rc;

    rc = mbedtls_ecp_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(entropy_test)
{
#if 0 /* XXX fix this later, no strong entropy source atm */
    int rc;

    rc = mbedtls_entropy_self_test(1);
    TEST_ASSERT(rc == 0);
#endif
}

TEST_CASE(gcm_test)
{
    int rc;

    rc = mbedtls_gcm_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(hmac_drbg_test)
{
    int rc;

    rc = mbedtls_hmac_drbg_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(md5_test)
{
    int rc;

    rc = mbedtls_md5_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(pkcs5_test)
{
    int rc;

    rc = mbedtls_pkcs5_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(ripemd160_test)
{
    int rc;

    rc = mbedtls_ripemd160_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(rsa_test)
{
    int rc;

    rc = mbedtls_rsa_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(x509_test)
{
    int rc;

    rc = mbedtls_x509_self_test(1);
    TEST_ASSERT(rc == 0);
}

TEST_CASE(xtea_test)
{
    int rc;

    rc = mbedtls_xtea_self_test(1);
    TEST_ASSERT(rc == 0);
}


TEST_SUITE(mbedtls_test_all)
{
    sha1_test();
    sha256_test();
    sha512_test();
    aes_test();
    arc4_test();
    bignum_test();
    ccm_test();
    dhm_test();
    ecp_test();
    entropy_test();
    gcm_test();
    hmac_drbg_test();
    md5_test();
    pkcs5_test();
    ripemd160_test();
    rsa_test();
    x509_test();
    xtea_test();
}

#ifdef MYNEWT_SELFTEST
int
main(int argc, char **argv)
{
    tu_config.tc_print_results = 1;
    tu_init();

    mbedtls_test_all();

    return tu_any_failed;
}

#endif

