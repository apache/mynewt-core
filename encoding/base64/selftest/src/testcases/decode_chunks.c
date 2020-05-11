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

#include "os/mynewt.h"
#include "base64_test_priv.h"

static void
pass(const char *src, const char *expected)
{
    struct base64_decoder dec;
    struct os_mbuf_pkthdr *omp;
    struct os_mbuf *cur;
    struct os_mbuf *om;
    int delta;
    int len;
    int rc;

    /* Split source data among several buffers in an mbuf chain. */

    om = os_msys_get_pkthdr(0, 0);
    TEST_ASSERT_FATAL(om != NULL);

    rc = os_mbuf_append(om, src, strlen(src));
    TEST_ASSERT_FATAL(rc == 0);

    /* Just make sure the source requires 2+ mbufs. */ 
    TEST_ASSERT_FATAL(SLIST_NEXT(om, om_next) != NULL);

    /* Decode source data in place. */

    omp = OS_MBUF_PKTHDR(om);

    memset(&dec, 0, sizeof dec);
    for (cur = om; cur != NULL; cur = SLIST_NEXT(cur, om_next)) {
        dec.src = (const char *)cur->om_data;
        dec.src_len = cur->om_len;
        dec.dst = cur->om_data;
        dec.dst_len = -1;

        len = base64_decoder_go(&dec);
        if (len < 0) {
            os_mbuf_free_chain(om);
            return;
        }

        delta = cur->om_len - len;
        cur->om_len = len;

        omp->omp_len -= delta;
    }

    // Verify mbuf contents.
    rc = os_mbuf_cmpf(om, 0, expected, strlen(expected));
    TEST_ASSERT(rc == 0);
}

TEST_CASE_SELF(decode_chunks)
{
    pass(
        "RnJvbSBteSBpbmZhbmN5IEkgd2FzIG5vdGVkIGZvciB0aGUgZG9jaWxpdHkgYW5kIGh1bWFuaXR5IG9mIG15IGRpc3Bvc2l0aW9uLiBNeSB0ZW5kZXJuZXNzIG9mIGhlYXJ0IHdhcyBldmVuIHNvIGNvbnNwaWN1b3VzIGFzIHRvIG1ha2UgbWUgdGhlIGplc3Qgb2YgbXkgY29tcGFuaW9ucy4gSSB3YXMgZXNwZWNpYWxseSBmb25kIG9mIGFuaW1hbHMsIGFuZCB3YXMgaW5kdWxnZWQgYnkgbXkgcGFyZW50cyB3aXRoIGEgZ3JlYXQgdmFyaWV0eSBvZiBwZXRzLiBXaXRoIHRoZXNlIEkgc3BlbnQgbW9zdCBvZiBteSB0aW1lLCBhbmQgbmV2ZXIgd2FzIHNvIGhhcHB5IGFzIHdoZW4gZmVlZGluZyBhbmQgY2FyZXNzaW5nIHRoZW0uIFRoaXMgcGVjdWxpYXJpdHkgb2YgY2hhcmFjdGVyIGdyZXcgd2l0aCBteSBncm93dGgsIGFuZCBpbiBteSBtYW5ob29kLCBJIGRlcml2ZWQgZnJvbSBpdCBvbmUgb2YgbXkgcHJpbmNpcGFsIHNvdXJjZXMgb2YgcGxlYXN1cmUuIFRvIHRob3NlIHdobyBoYXZlIGNoZXJpc2hlZCBhbiBhZmZlY3Rpb24gZm9yIGEgZmFpdGhmdWwgYW5kIHNhZ2FjaW91cyBkb2csIEkgbmVlZCBoYXJkbHkgYmUgYXQgdGhlIHRyb3VibGUgb2YgZXhwbGFpbmluZyB0aGUgbmF0dXJlIG9yIHRoZSBpbnRlbnNpdHkgb2YgdGhlIGdyYXRpZmljYXRpb24gdGh1cyBkZXJpdmFibGUuIFRoZXJlIGlzIHNvbWV0aGluZyBpbiB0aGUgdW5zZWxmaXNoIGFuZCBzZWxmLXNhY3JpZmljaW5nIGxvdmUgb2YgYSBicnV0ZSwgd2hpY2ggZ29lcyBkaXJlY3RseSB0byB0aGUgaGVhcnQgb2YgaGltIHdobyBoYXMgaGFkIGZyZXF1ZW50IG9jY2FzaW9uIHRvIHRlc3QgdGhlIHBhbHRyeSBmcmllbmRzaGlwIGFuZCBnb3NzYW1lciBmaWRlbGl0eSBvZiBtZXJlIE1hbi4K",
        "From my infancy I was noted for the docility and humanity of my disposition. My tenderness of heart was even so conspicuous as to make me the jest of my companions. I was especially fond of animals, and was indulged by my parents with a great variety of pets. With these I spent most of my time, and never was so happy as when feeding and caressing them. This peculiarity of character grew with my growth, and in my manhood, I derived from it one of my principal sources of pleasure. To those who have cherished an affection for a faithful and sagacious dog, I need hardly be at the trouble of explaining the nature or the intensity of the gratification thus derivable. There is something in the unselfish and self-sacrificing love of a brute, which goes directly to the heart of him who has had frequent occasion to test the paltry friendship and gossamer fidelity of mere Man."
    );
}
