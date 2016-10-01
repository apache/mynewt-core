/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   cbor_cnt_writer.h
 * Author: paulfdietrich
 *
 * Created on September 30, 2016, 4:50 PM
 */

#ifndef CBOR_CNT_WRITER_H
#define CBOR_CNT_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

    /* use this count writer if you want to try out a cbor encoding to see
     * how long it would be (before allocating memory). This replaced the
     * code in tinycbor.h that would try to do this once the encoding failed
     * in a buffer.  Its much easier to understand this way (for me)
     */

struct CborCntWriter {
    int bytes_written;
};

inline void
cbor_cnt_writer_init(struct CborCntWriter *cb) {
    cb->bytes_written = 0;
}

inline int
cbor_cnt_writer(void *arg, char *data, int len) {
    struct CborCntWriter *cb = (struct CborCntWriter *) arg;
    cb->bytes_written += len;
    return CborNoError;
}

inline int
cbor_cnt_writer_length(struct CborCntWriter *cb) {
    return cb->bytes_written;
}

#ifdef __cplusplus
}
#endif

#endif /* CBOR_CNT_WRITER_H */

