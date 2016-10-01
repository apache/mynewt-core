/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   cbor_mbuf_writer.h
 * Author: paulfdietrich
 *
 * Created on September 30, 2016, 4:59 PM
 */

#ifndef CBOR_MBUF_WRITER_H
#define CBOR_MBUF_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif


struct CborMbufWriter {
    int bytes_written;
    struct os_mbuf *m;
};

void
cbor_mbuf_writer_init(struct CborMbufWriter *cb, struct os_mbuf *m);

int
cbor_mbuf_writer(void *arg, const char *data, int len);


int
cbor_mbuf_bytes_written(struct CborMbufWriter *cb);

#ifdef __cplusplus
}
#endif

#endif /* CBOR_MBUF_WRITER_H */

