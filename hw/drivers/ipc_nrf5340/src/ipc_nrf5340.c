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
#if MYNEWT_VAL(IPC_NRF5340_NET_GPIO)
#include <mcu/nrf5340_hal.h>
#include <bsp/bsp.h>
#endif

/* Currently this allows only for 1-1 connection. */

#define IPC_MAX_CHANS MYNEWT_VAL(IPC_NRF5340_CHANNELS)
#define IPC_BUF_SIZE MYNEWT_VAL(IPC_NRF5340_BUF_SZ)

#if MYNEWT_VAL(MCU_APP_CORE) && MYNEWT_VAL(NRF5340_EMBED_NET_CORE)
/*
 * For combine build (app and net) network core image will be included in application
 * image only if it is referenced. Those to symbols are used to force linker to
 * include network core image.
 * Those are used also to provide net core image data (and its size) to virtual
 * flash driver that is used by bootloader to bring network image from application
 * flash to network flash slot 1.
 */
extern uint8_t _binary_net_core_img_start;
extern uint8_t _binary_net_core_img_end;
#endif

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

/* this function assumes that there is enough space for data */
static void
ipc_nrf5340_shm_write(struct ipc_shm *shm, const void *data, uint16_t data_len)
{
    uint16_t head = shm->head;
    uint16_t len;

    len = min(data_len, IPC_BUF_SIZE - head);
    memcpy(shm->buf + head, data, len);

    /* copy second fragment */
    if (data_len > len) {
        memcpy(shm->buf, data + len, data_len - len);
    }

    head += data_len;
    head %= IPC_BUF_SIZE;

    shm->head = head;
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

    /* Handle only interrupts that were enabled */
    irq_pend = NRF_IPC->INTPEND & NRF_IPC->INTEN;

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
    int i;

#if MYNEWT_VAL(MCU_APP_CORE)
#if MYNEWT_VAL(IPC_NRF5340_NET_GPIO)
    unsigned int gpios[] = { MYNEWT_VAL(IPC_NRF5340_NET_GPIO) };
    NRF_GPIO_Type *nrf_gpio;
#endif

#if MYNEWT_VAL(MCU_APP_CORE) && MYNEWT_VAL(NRF5340_EMBED_NET_CORE)
    /*
     * Get network core image size and placement in application flash.
     * Then pass those two values in NRF_IPC GPMEM registers to be used
     * by virtual flash driver on network side.
     */
    if (&_binary_net_core_img_end - &_binary_net_core_img_start > 32) {
        NRF_IPC->GPMEM[0] = (uint32_t)&_binary_net_core_img_start;
        NRF_IPC->GPMEM[1] = &_binary_net_core_img_end - &_binary_net_core_img_start;
    }
#endif

    /* Make sure network core if off when we set up IPC */
    NRF_RESET_S->NETWORK.FORCEOFF = RESET_NETWORK_FORCEOFF_FORCEOFF_Hold;
    memset(shms, 0, sizeof(shms));

#if MYNEWT_VAL(IPC_NRF5340_NET_GPIO)
    /* Configure GPIOs for Networking Core */
    for (i = 0; i < ARRAY_SIZE(gpios); i++) {
        nrf_gpio = HAL_GPIO_PORT(gpios[i]);
        nrf_gpio->PIN_CNF[HAL_GPIO_INDEX(gpios[i])] =
            GPIO_PIN_CNF_MCUSEL_NetworkMCU << GPIO_PIN_CNF_MCUSEL_Pos;
    }
#endif
#endif

#if MYNEWT_VAL(MCU_NET_CORE)
    /*
     * When network core IPCs starts it clears GPMEM from APP core registers
     * So IPC nows that netcore is running.
     * This is a workaround that is needed till application side code waits
     * on IPC for network core controller to sent NOP first.
     */
#define NRF_APP_IPC_NS                  ((NRF_IPC_Type *)0x4002A000)
#define NRF_APP_IPC_S                   ((NRF_IPC_Type *)0x5002A000)
    NRF_APP_IPC_S->GPMEM[0] = 0;
    NRF_APP_IPC_S->GPMEM[1] = 0;
#endif

    /* Enable IPC channels */
    for (i = 0; i < IPC_MAX_CHANS; i++) {
        NRF_IPC->SEND_CNF[i] = (0x01UL << i);
        NRF_IPC->RECEIVE_CNF[i] = 0;
    }

    NRF_IPC->INTENCLR = 0xFFFF;
    NVIC_ClearPendingIRQ(IPC_IRQn);
    NVIC_SetVector(IPC_IRQn, (uint32_t)ipc_nrf5340_isr);
    NVIC_EnableIRQ(IPC_IRQn);

#if MYNEWT_VAL(MCU_APP_CORE)
    /* this allows netcore to access appcore RAM */
    NRF_SPU_S->EXTDOMAIN[0].PERM = SPU_EXTDOMAIN_PERM_SECATTR_Secure << SPU_EXTDOMAIN_PERM_SECATTR_Pos;

    /* Start Network Core */
    NRF_RESET_S->NETWORK.FORCEOFF = RESET_NETWORK_FORCEOFF_FORCEOFF_Release;
#endif
#if MYNEWT_VAL(MCU_APP_CORE) && MYNEWT_VAL(NRF5340_EMBED_NET_CORE)
    /*
     * TODO: Remove below workaround:
     * For now app core waits for NET core to start.
     * It is needed for case when network core was updated with new
     * image and due to several second delay caused by bootloader
     * copy image application starts to send HCI reset requests via
     * IPC and network HCI transport is not really ready to handle
     * more then one command.
     */
    if (&_binary_net_core_img_end - &_binary_net_core_img_start > 32) {
        /*
         * Application side prepared image for net core.
         * When net core starts it's ipc_nrf5340_init() will clear those.
         */
        while (NRF_IPC->GPMEM[1]);
    }
#endif
}

void
ipc_nrf5340_recv(int channel, ipc_nrf5340_recv_cb cb, void *user_data)
{
    assert(channel < IPC_MAX_CHANS);

    if (cb) {
        assert(ipcs[channel].cb == NULL);

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
    struct ipc_shm *shm;
    uint16_t frag_len;
    uint16_t space;

    assert(channel < IPC_MAX_CHANS);
    shm = &shms[channel];

    if (data && len) {
        while (len) {
            do {
                space = IPC_BUF_SIZE - 1;
                space -= ipc_nrf5340_shm_get_data_length(shm->head, shm->tail);
#if !MYNEWT_VAL(IPC_NRF5340_BLOCKING_WRITE)
                /* assert since that will always fail for non-blocking write
                 * indicating use error
                 */
                assert(len < IPC_BUF_SIZE);
                if (len > space) {
                    return SYS_ENOMEM;
                }
#endif
            } while (space == 0);

            frag_len = min(len, space);
            ipc_nrf5340_shm_write(shm, data, frag_len);
            NRF_IPC->TASKS_SEND[channel] = 1;

            data += frag_len;
            len -= frag_len;
        }
    }

    return 0;
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
