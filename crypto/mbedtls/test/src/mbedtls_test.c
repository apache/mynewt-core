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
#include <stdio.h>
#include <string.h>

#include "os/mynewt.h"
#include "testutil/testutil.h"

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

TEST_CASE_DECL(sha1_test)
TEST_CASE_DECL(sha256_test)
TEST_CASE_DECL(sha512_test)
TEST_CASE_DECL(aes_test)
TEST_CASE_DECL(arc4_test)
TEST_CASE_DECL(bignum_test)
TEST_CASE_DECL(ccm_test)
TEST_CASE_DECL(dhm_test)
TEST_CASE_DECL(ecp_test)
TEST_CASE_DECL(entropy_test)
TEST_CASE_DECL(gcm_test)
TEST_CASE_DECL(hmac_drbg_test)
TEST_CASE_DECL(md5_test)
TEST_CASE_DECL(pkcs5_test)
TEST_CASE_DECL(ripemd160_test)
TEST_CASE_DECL(rsa_test)
TEST_CASE_DECL(x509_test)
TEST_CASE_DECL(xtea_test)

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

#if MYNEWT_VAL(SELFTEST)
int
main(int argc, char **argv)
{
    sysinit();

    mbedtls_test_all();

    return tu_any_failed;
}

#endif
