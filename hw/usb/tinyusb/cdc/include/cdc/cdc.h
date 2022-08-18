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

#ifndef __CDC_H__
#define __CDC_H__

#include <inttypes.h>
#include <stdbool.h>
#include <class/cdc/cdc_device.h>

struct cdc_itf;
typedef struct cdc_itf cdc_itf_t;

struct cdc_callbacks {
    /* Invoked when received new data */
    void (*cdc_rx_cb)(cdc_itf_t *itf);

    /* Invoked when received `wanted_char` */
    void (*cdc_rx_wanted_cb)(cdc_itf_t *itf, char wanted_char);

    /* Invoked when space becomes available in TX buffer */
    void (*cdc_tx_complete_cb)(cdc_itf_t *itf);

    /* Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE */
    void (*cdc_line_state_cb)(cdc_itf_t *itf, bool dtr, bool rts);

    /* Invoked when line coding is change via SET_LINE_CODING */
    void (*cdc_line_coding_cb)(cdc_itf_t *itf, cdc_line_coding_t const *p_line_coding);

    /* Invoked when received send break */
    void (*cdc_send_break_cb)(cdc_itf_t *itf, uint16_t duration_ms);
};

struct cdc_itf {
    const struct cdc_callbacks *callbacks;
    uint8_t cdc_num;
};

uint8_t cdc_itf_add(cdc_itf_t *cdc_ift);

#endif /* __CDC_H__ */
