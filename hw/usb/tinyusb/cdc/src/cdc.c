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

#include <assert.h>
#include <cdc/cdc.h>

static cdc_itf_t *cdc_itfs[CFG_TUD_CDC];
static uint8_t cdc_itf_count;

/* Invoked when received new data */
void
tud_cdc_rx_cb(uint8_t itf)
{
    cdc_itf_t *cdc_itf = cdc_itfs[itf];

    if (cdc_itf->callbacks->cdc_rx_cb) {
        cdc_itf->callbacks->cdc_rx_cb(cdc_itf);
    }
}

/* Invoked when received `wanted_char` */
void
tud_cdc_rx_wanted_cb(uint8_t itf, char wanted_char)
{
    cdc_itf_t *cdc_itf = cdc_itfs[itf];

    if (cdc_itf->callbacks->cdc_rx_wanted_cb) {
        cdc_itf->callbacks->cdc_rx_wanted_cb(cdc_itf, wanted_char);
    }
}

/* Invoked when space becomes available in TX buffer */
void
tud_cdc_tx_complete_cb(uint8_t itf)
{
    cdc_itf_t *cdc_itf = cdc_itfs[itf];

    if (cdc_itf->callbacks->cdc_tx_complete_cb) {
        cdc_itf->callbacks->cdc_tx_complete_cb(cdc_itf);
    }
}

/* Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE */
void
tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    cdc_itf_t *cdc_itf = cdc_itfs[itf];

    if (cdc_itf->callbacks->cdc_line_state_cb) {
        cdc_itf->callbacks->cdc_line_state_cb(cdc_itf, dtr, rts);
    }
}

/* Invoked when line coding is change via SET_LINE_CODING */
void
tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p_line_coding)
{
    cdc_itf_t *cdc_itf = cdc_itfs[itf];

    if (cdc_itf->callbacks->cdc_line_coding_cb) {
        cdc_itf->callbacks->cdc_line_coding_cb(cdc_itf, p_line_coding);
    }
}

/* Invoked when received send break */
void
tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms)
{
    cdc_itf_t *cdc_itf = cdc_itfs[itf];

    if (cdc_itf->callbacks->cdc_send_break_cb) {
        cdc_itf->callbacks->cdc_send_break_cb(cdc_itf, duration_ms);
    }
}

uint8_t
cdc_itf_add(cdc_itf_t *cdc_itf)
{
    int sr;

    assert(cdc_itf_count < CFG_TUD_CDC);

    OS_ENTER_CRITICAL(sr);
    cdc_itfs[cdc_itf_count] = cdc_itf;
    cdc_itf->cdc_num = cdc_itf_count++;
    OS_EXIT_CRITICAL(sr);

    return cdc_itf->cdc_num;
}
