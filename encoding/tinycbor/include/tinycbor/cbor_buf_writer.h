/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   cbor_buf_writer.h
 * Author: paulfdietrich
 *
 * Created on September 30, 2016, 4:38 PM
 */

#ifndef CBOR_BUF_WRITER_H
#define CBOR_BUF_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

struct CborBufWriter {
    uint8_t *ptr;
    const uint8_t *end;
};

void
cbor_buf_writer_init(struct CborBufWriter *cb, uint8_t *buffer, size_t data);

int
cbor_buf_writer(void *arg, const char *data, int len);

size_t
cbor_buf_writer_buffer_size(struct CborBufWriter *cb, const uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* CBOR_BUF_WRITER_H */
