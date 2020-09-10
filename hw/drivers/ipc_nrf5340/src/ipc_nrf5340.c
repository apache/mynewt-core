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

#include <errno.h>
#include <os/os.h>
#include <ipc_nrf5340/ipc_nrf5340.h>
#include <nrfx.h>

/* Currently this allows only for 1-1 connection. */

#define IPC_MAX_CHANS MYNEWT_VAL(IPC_NRF5340_CHANNELS)
#define IPC_BUF_SIZE MYNEWT_VAL(IPC_NRF5340_BUF_SZ)

struct ipc_channel {
    ipc_nrf5340_recv_cb cb;
    void *user_data;
};

struct ipc_shm {
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t buf[IPC_BUF_SIZE];
};

static struct ipc_channel ipcs[IPC_MAX_CHANS];
static struct ipc_shm __attribute__((section (".ipc"))) shms[IPC_MAX_CHANS];

static uint16_t
ipc_nrf5340_shm_get_data_length(uint16_t head, uint16_t tail)
{
    return ((unsigned int)(head - tail)) % IPC_BUF_SIZE;
}

static int
ipc_nrf5340_shm_write(struct ipc_shm *shm, const void *data, uint16_t data_len)
{
    uint16_t head = shm->head;
    uint16_t len;

    /* check if data will fit */
    while (data_len + ipc_nrf5340_shm_get_data_length(head, shm->tail) >= IPC_BUF_SIZE) {
#if !MYNEWT_VAL(IPC_NRF5340_BLOCKING_WRITE)
        return -ENOMEM;
#endif
    }

    len = min(data_len, IPC_BUF_SIZE - head);
    memcpy(shm->buf + head, data, len);

    /* copy second fragment */
    if (data_len > len) {
        memcpy(shm->buf, data + len, data_len - len);
    }

    head += data_len;
    head %= IPC_BUF_SIZE;

    shm->head = head;

    return 0;
}

static uint16_t
ipc_nrf5340_shm_read(struct ipc_shm *shm, void *buf, struct os_mbuf *om,
                     uint16_t len)
{
    uint16_t head = shm->head;
    uint16_t tail = shm->tail;
    uint16_t frag_len;

    len = min(ipc_nrf5340_shm_get_data_length(head, tail), len);
    if (len == 0) {
        return 0;
    }

    if (buf || om) {
        if (head >= tail) {
            if (buf) {
                memcpy(buf, shm->buf + tail, len);
            } else {
                os_mbuf_append(om, shm->buf + tail, len);
            }
        } else {
            /* in this case we may need to copy two fragments */
            frag_len = min(len, IPC_BUF_SIZE - tail);
            if (buf) {
                memcpy(buf, shm->buf + tail, frag_len);
            } else {
                os_mbuf_append(om, shm->buf + tail, frag_len);
            }

            if (frag_len < len) {
                assert(tail + frag_len == IPC_BUF_SIZE);
                assert(len - frag_len <= head);
                if (buf) {
                    memcpy(buf + frag_len, shm->buf, len - frag_len);
                } else {
                    os_mbuf_append(om, shm->buf, len - frag_len);
                }
            }
        }
    }

    tail += len;
    tail %= IPC_BUF_SIZE;

    shm->tail = tail;

    return len;
}

static void
ipc_nrf5340_isr(void)
{
    uint32_t irq_pend;
    int i;

    os_trace_isr_enter();

    irq_pend = NRF_IPC->INTPEND;

    for (i = 0; i < IPC_MAX_CHANS; i++) {
        if (irq_pend & (0x1UL << i)) {
            NRF_IPC->EVENTS_RECEIVE[i] = 0;
            ipcs[i].cb(i, ipcs[i].user_data);
        }
    }

    os_trace_isr_exit();
}

void
ipc_nrf5340_init(void)
{
    int channel;

#if MYNEWT_VAL(BSP_NRF5340)
    /* Make sure network core if off when we set up IPC */
    NRF_RESET_S->NETWORK.FORCEOFF  = RESET_NETWORK_FORCEOFF_FORCEOFF_Hold;
    memset(shms, 0, sizeof(shms));
#endif

    /* Enable IPC channels */
    for (channel = 0; channel < IPC_MAX_CHANS; channel++) {
        NRF_IPC->SEND_CNF[channel] = (0x01UL << channel);
    }

    NVIC_SetVector(IPC_IRQn, (uint32_t)ipc_nrf5340_isr);
    NVIC_EnableIRQ(IPC_IRQn);

#if MYNEWT_VAL(BSP_NRF5340)
    /* this allows netcore to access appcore RAM */
    NRF_SPU_S->EXTDOMAIN[0].PERM = SPU_EXTDOMAIN_PERM_SECATTR_Secure << SPU_EXTDOMAIN_PERM_SECATTR_Pos;

    /* Start Network Core */
    NRF_RESET_S->NETWORK.FORCEOFF = RESET_NETWORK_FORCEOFF_FORCEOFF_Release;
#endif
}

void
ipc_nrf5340_recv(int channel, ipc_nrf5340_recv_cb cb, void *user_data)
{
    assert(channel < IPC_MAX_CHANS);

    if (cb) {
        ipcs[channel].cb = cb;
        ipcs[channel].user_data = user_data;
        NRF_IPC->RECEIVE_CNF[channel] = (0x1UL << channel);
        NRF_IPC->INTENSET = (0x1UL << channel);
    } else {
        NRF_IPC->INTENCLR = (0x1UL << channel);
        NRF_IPC->RECEIVE_CNF[channel] = 0;
        ipcs[channel].cb = NULL;
        ipcs[channel].user_data = NULL;
    }
}

int
ipc_nrf5340_send(int channel, const void *data, uint16_t len)
{
    int rc = 0;

    assert(channel < IPC_MAX_CHANS);

    if (data && len) {
        rc = ipc_nrf5340_shm_write(&shms[channel], data, len);
    }

    NRF_IPC->TASKS_SEND[channel] = 1;

    return rc;
}

uint16_t
ipc_nrf5340_read(int channel, void *buf, uint16_t len)
{
    assert(channel < IPC_MAX_CHANS);

    return ipc_nrf5340_shm_read(&shms[channel], buf, NULL, len);
}

uint16_t
ipc_nrf5340_read_om(int channel, struct os_mbuf *om, uint16_t len)
{
    assert(channel < IPC_MAX_CHANS);

    return ipc_nrf5340_shm_read(&shms[channel], NULL, om, len);
}

uint16_t
ipc_nrf5340_available(int channel)
{
    assert(channel < IPC_MAX_CHANS);

    return ipc_nrf5340_shm_get_data_length(shms[channel].head,
                                           shms[channel].tail);
}

uint16_t
ipc_nrf5340_consume(int channel, uint16_t len)
{
    assert(channel < IPC_MAX_CHANS);

    return ipc_nrf5340_shm_read(&shms[channel], NULL, NULL, len);
}
