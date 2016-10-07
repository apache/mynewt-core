/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   cbor_mbuf_reader.h
 * Author: paulfdietrich
 *
 * Created on October 5, 2016, 1:19 PM
 */

#ifndef CBOR_MBUF_READER_H
#define CBOR_MBUF_READER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tinycbor/cbor.h>
#include <os/os_mbuf.h>

struct CborMbufReader {
    struct cbor_decoder_reader r;
    int init_off;                     /* initial offset into the data */
    struct os_mbuf *m;
};

void
cbor_mbuf_reader_init(struct CborMbufReader *cb,
                        struct os_mbuf *m,
                        int intial_offset);

#ifdef __cplusplus
}
#endif

#endif /* CBOR_MBUF_READER_H */

