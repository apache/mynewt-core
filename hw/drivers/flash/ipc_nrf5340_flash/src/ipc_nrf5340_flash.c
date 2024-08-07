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

#include <string.h>
#include <assert.h>
#include <os/mynewt.h>
#include <os/os_mutex.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <ipc_nrf5340/ipc_nrf5340.h>
#if MYNEWT_VAL(MCU_APP_CORE)
#include <mcu/nrf5340_hal.h>
#endif
#if MYNEWT_VAL(MCU_NET_CORE)
#include <mcu/nrf5340_net_hal.h>
#endif

#define CLIENT_OUT_CHANNEL MYNEWT_VAL(IPC_NRF5340_FLASH_CLIENT_OUT_CHANNEL)
#define CLIENT_IN_CHANNEL MYNEWT_VAL(IPC_NRF5340_FLASH_CLIENT_IN_CHANNEL)
#define SERVER_IN_CHANNEL MYNEWT_VAL(IPC_NRF5340_FLASH_SERVER_IN_CHANNEL)
#define SERVER_OUT_CHANNEL MYNEWT_VAL(IPC_NRF5340_FLASH_SERVER_OUT_CHANNEL)

typedef enum {
    FLASH_OP_DATA_DOWN      = 0x400,
    FLASH_OP_DATA_UP        = 0x800,
    FLASH_OP_REQ            = 0x000,
    FLASH_OP_RESP           = 0x100,

    FLASH_OP_INFO           = 0x001 | FLASH_OP_DATA_UP,
    FLASH_OP_WRITE          = 0x002 | FLASH_OP_DATA_DOWN,
    FLASH_OP_READ           = 0x003 | FLASH_OP_DATA_UP,
    FLASH_OP_ERASE_SECTOR   = 0x004,
} flash_op_t;

struct ipc_msg_hdr {
    uint16_t type;
    uint16_t msg_len;
};

#define HDR_SZ sizeof(struct ipc_msg_hdr)

struct ipc_msg  {
    struct ipc_msg_hdr hdr;
    uint8_t param[16];
    void *data;
    uint16_t data_len;
    uint8_t header_len;
};

static inline void
put8(struct ipc_msg *msg, uint8_t data)
{
    *((uint8_t *)msg + msg->header_len) = data;
    msg->header_len++;
}

static inline void
put16(struct ipc_msg *msg, uint16_t data)
{
    put8(msg, (uint8_t)data);
    put8(msg, (uint8_t)(data >> 8));
}

static inline void
put32(struct ipc_msg *msg, uint32_t u32)
{
    put16(msg, (uint16_t)u32);
    put16(msg, (uint16_t)(u32 >> 16));
}

static inline const uint8_t
get8(const uint8_t **p)
{
    uint8_t data = **p;
    ++(*p);
    return data;
}

static inline uint16_t
get16(const uint8_t **p)
{
    uint16_t d = get8(p);
    return (((uint16_t)get8(p)) << 8) | d;
}

static inline uint32_t
get32(const uint8_t **p)
{
    uint32_t d = get16(p);
    return (((uint32_t)get16(p)) << 16) | d;
}

#if MYNEWT_VAL(IPC_NRF5340_FLASH_CLIENT)

struct ipc_flash {
    struct hal_flash hal_flash;
    struct os_sem sem;
    struct os_mutex mutex;
    struct ipc_msg *cmd;
    struct ipc_msg resp;
};

static int
nrf5340_ipc_flash_lock(struct ipc_flash *flash)
{
    int rc;

    rc = os_mutex_pend(&flash->mutex, OS_WAIT_FOREVER);

    return rc;
}

static int
nrf5340_ipc_flash_unlock(struct ipc_flash *flash)
{
    int rc;

    rc = os_mutex_release(&flash->mutex);

    return rc;
}

static void
nrf5340_ipc_flash_client_read(void *buf, uint32_t size)
{
    uint16_t read_cnt;

    read_cnt = ipc_nrf5340_read(CLIENT_IN_CHANNEL, buf, size);
    assert(read_cnt == size);
}

static int
nrf5340_ipc_flash_cmd(struct ipc_flash *flash, struct ipc_msg *cmd)
{
    const uint8_t *p;
    int rc;

    nrf5340_ipc_flash_lock(flash);
    assert(flash->cmd == NULL);
    flash->cmd = cmd;

    if (cmd->hdr.type & FLASH_OP_DATA_DOWN) {
        cmd->hdr.msg_len = cmd->header_len + cmd->data_len;
    } else {
        cmd->hdr.msg_len = cmd->header_len;
    }
    ipc_nrf5340_send(CLIENT_OUT_CHANNEL, &cmd->hdr.type, cmd->header_len);
    if (cmd->header_len < cmd->hdr.msg_len) {
        ipc_nrf5340_send(CLIENT_OUT_CHANNEL, cmd->data, cmd->data_len);
    }

    if (os_sem_pend(&flash->sem,
                    os_time_ms_to_ticks32(MYNEWT_VAL(IPC_NRF5340_FLASH_CLIENT_TIMEOUT)))) {
        /* If server did not respond in time, there is not point in waiting */
        assert(0);
    }

    p = flash->resp.param;
    rc = get32(&p);

    flash->cmd = NULL;

    nrf5340_ipc_flash_unlock(flash);

    return rc;
}

static int
nrf5340_ipc_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
                       uint32_t num_bytes)
{
    int rc;
    struct ipc_flash *flash = (struct ipc_flash *)dev;
    struct ipc_msg cmd = {
        .hdr.type = FLASH_OP_READ,
        .header_len = sizeof(struct ipc_msg_hdr),
        .data = dst,
        .data_len = num_bytes,
    };
    put32(&cmd, address);
    put32(&cmd, num_bytes);

    rc = nrf5340_ipc_flash_cmd(flash, &cmd);

    return rc;
}

static int
nrf5340_ipc_flash_write(const struct hal_flash *dev, uint32_t address,
                        const void *src, uint32_t num_bytes)
{
    int rc;
    struct ipc_flash *flash = (struct ipc_flash *)dev;
    struct ipc_msg cmd = {
        .hdr.type = FLASH_OP_WRITE,
        .header_len = sizeof(struct ipc_msg_hdr),
        .data = (void *)src,
        .data_len = num_bytes,
    };
    put32(&cmd, address);
    put32(&cmd, num_bytes);

    rc = nrf5340_ipc_flash_cmd(flash, &cmd);

    return rc;
}

static int
nrf5340_ipc_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    int rc;
    struct ipc_flash *flash = (struct ipc_flash *)dev;
    uint32_t sector_sz = flash->hal_flash.hf_size / flash->hal_flash.hf_sector_cnt;
    struct ipc_msg cmd = {
        .hdr.type = FLASH_OP_ERASE_SECTOR,
        .header_len = sizeof(struct ipc_msg_hdr),
    };
    put32(&cmd, sector_address & ~(sector_sz - 1));

    rc = nrf5340_ipc_flash_cmd(flash, &cmd);

    return rc;
}

static int
nrf5340_ipc_flash_sector_info(const struct hal_flash *dev, int idx,
                              uint32_t *address, uint32_t *sz)
{
    struct ipc_flash *flash = (struct ipc_flash *)dev;
    uint32_t sector_sz = flash->hal_flash.hf_size / flash->hal_flash.hf_sector_cnt;

    assert(idx < flash->hal_flash.hf_sector_cnt);
    *address = dev->hf_base_addr + idx * sector_sz;
    *sz = sector_sz;

    return 0;
}

static int nrf5340_ipc_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs nrf5340_ipc_flash_funcs = {
    .hff_read = nrf5340_ipc_flash_read,
    .hff_write = nrf5340_ipc_flash_write,
    .hff_erase_sector = nrf5340_ipc_flash_erase_sector,
    .hff_sector_info = nrf5340_ipc_flash_sector_info,
    .hff_init = nrf5340_ipc_flash_init,
    .hff_erase = NULL,
};

static int
nrf5340_ipc_flash_info(struct ipc_flash *flash, struct hal_flash *info)
{
    int rc;
    struct ipc_msg cmd = {
        .hdr.type = FLASH_OP_INFO,
        .data_len = sizeof(*info),
        .data = info,
        .header_len = HDR_SZ,
    };

    rc = nrf5340_ipc_flash_cmd(flash, &cmd);

    return rc;
}

static void
nrf5340_ipc_flash_recv_cb(int channel, void *user_data)
{
    struct ipc_flash *flash = user_data;
    uint16_t available;
    uint16_t payload_size;
    int read_cnt;

    assert(channel == CLIENT_IN_CHANNEL);

    available = ipc_nrf5340_available(CLIENT_IN_CHANNEL);
    /* If header was not yet received wait for sizeof(header) bytes */
    if (flash->resp.hdr.type == 0) {
        if (available < HDR_SZ) {
            return;
        }
        nrf5340_ipc_flash_client_read(&flash->resp.hdr.type, HDR_SZ);
        available -= HDR_SZ;
    }

    payload_size = flash->resp.hdr.msg_len - HDR_SZ;

    /* Make sure whole message is ready */
    if (available < payload_size) {
        return;
    }

    assert(flash->cmd);
    assert(flash->resp.hdr.type);
    assert(flash->resp.hdr.type == (flash->cmd->hdr.type | FLASH_OP_RESP));

    switch (flash->cmd->hdr.type) {
    case FLASH_OP_INFO:
    case FLASH_OP_READ:
        /* Read error code */
        nrf5340_ipc_flash_client_read(&flash->resp.param, sizeof(uint32_t));
        available = payload_size - sizeof(uint32_t);
        read_cnt = 0;
        if (available > flash->cmd->data_len) {
            /* Fill up user provided buffer */
            read_cnt = flash->cmd->data_len;
            nrf5340_ipc_flash_client_read(flash->cmd->data, flash->cmd->data_len);
        } else if (available > 0) {
            /* Fill some of the user provided buffer */
            read_cnt = available;
            nrf5340_ipc_flash_client_read(flash->cmd->data, available);
        }
        if (read_cnt < available) {
            /* More data available then user buffer, should not happen */
            ipc_nrf5340_consume(CLIENT_IN_CHANNEL, available - read_cnt);
        }
        os_sem_release(&flash->sem);
        break;
    case FLASH_OP_WRITE:
    case FLASH_OP_ERASE_SECTOR:
        /* Error code is expected, apart from header */
        assert(flash->resp.hdr.msg_len - HDR_SZ == sizeof(uint32_t));
        nrf5340_ipc_flash_client_read(flash->resp.param, sizeof(uint32_t));
        os_sem_release(&flash->sem);
        break;
    default:
        assert(0);
    }
    /* Clear type filed so header will be read first next time */
    flash->resp.hdr.type = 0;
}

static int
nrf5340_ipc_flash_init(const struct hal_flash *dev)
{
    struct ipc_flash *flash = (struct ipc_flash *)dev;
    struct hal_flash flash_desc;
    int rc;

    os_sem_init(&flash->sem, 0);
    os_mutex_init(&flash->mutex);

    rc = nrf5340_ipc_flash_lock(flash);
    assert(rc == 0);

    /* Setup receive callback first */
    ipc_nrf5340_recv(CLIENT_IN_CHANNEL, nrf5340_ipc_flash_recv_cb, flash);

    /*
     * Request flash info, data will be used to report remote flash
     * data to current core
     */
    rc = nrf5340_ipc_flash_info(flash, &flash_desc);
    if (rc == 0) {
        flash->hal_flash = flash_desc;
        flash->hal_flash.hf_itf = &nrf5340_ipc_flash_funcs;
    }

    (void)nrf5340_ipc_flash_unlock(flash);

    return rc;
}

struct ipc_flash nrf5340_ipc_flash_dev = {
    .hal_flash = {
        .hf_itf = &nrf5340_ipc_flash_funcs,
    },
};

const struct hal_flash *
ipc_flash(void)
{
    return &nrf5340_ipc_flash_dev.hal_flash;
}

#endif

#if MYNEWT_VAL(IPC_NRF5340_FLASH_SERVER)

static struct ipc_msg server_resp;
static struct ipc_msg server_req;

static void
nrf5340_ipc_flash_server_read(void *buf, uint32_t size)
{
    uint16_t read_cnt;

    read_cnt = ipc_nrf5340_read(SERVER_IN_CHANNEL, buf, size);
    assert(read_cnt == size);
}

static int
nrf5340_ipc_flash_resp(struct ipc_msg *req)
{
    server_resp.hdr.type = req->hdr.type | FLASH_OP_RESP;
    server_resp.hdr.msg_len = server_resp.header_len + server_resp.data_len;

    /* Send part included in ipc_msg structure. */
    ipc_nrf5340_send(SERVER_OUT_CHANNEL, &server_resp, server_resp.header_len);
    if (server_resp.data_len > 0) {
        /* Send part passed by pointer if any. */
        ipc_nrf5340_send(SERVER_OUT_CHANNEL, server_resp.data, server_resp.data_len);
    }

    req->hdr.type = 0;

    return 0;
}

static int
nrf5340_ipc_flash_std_resp(struct ipc_msg *req, int rc, void *data, uint32_t data_len)
{
    server_resp.header_len = HDR_SZ;
    server_resp.data_len = data_len;
    server_resp.data = data;

    /* Add rc to header, this updates header_len */
    put32(&server_resp, (uint32_t)rc);

    return nrf5340_ipc_flash_resp(req);
}

static int
nrf5340_ipc_flash_info_resp(struct ipc_msg *req)
{
    return nrf5340_ipc_flash_std_resp(req, 0, (void *)&nrf_flash_dev, sizeof(nrf_flash_dev));
}

static int
nrf5340_ipc_flash_read_resp(struct ipc_msg *req)
{
    const uint8_t *p;
    uint32_t address;
    uint32_t size;

    assert(req->hdr.msg_len = HDR_SZ + 2 * sizeof(uint32_t));
    nrf5340_ipc_flash_server_read(&req->param, req->hdr.msg_len - HDR_SZ);

    p = req->param;
    address = get32(&p);
    size = get32(&p);

    return nrf5340_ipc_flash_std_resp(req, 0, (void *)address, size);
}

static void
flash_write_cb(struct os_event *event)
{
    int rc = 0;
    struct ipc_msg *req = event->ev_arg;
    uint32_t left;
    uint32_t address;
    uint32_t chunk;

    nrf5340_ipc_flash_server_read(&address, sizeof(uint32_t));
    nrf5340_ipc_flash_server_read(&left, sizeof(uint32_t));
    assert(left == req->hdr.msg_len - (HDR_SZ + 2 * sizeof(uint32_t)));

    while (left && rc == 0) {
        chunk = sizeof(req->param);
        if (chunk > left) {
            chunk = left;
        }
        nrf5340_ipc_flash_server_read(req->param, chunk);
        rc = hal_flash_write(0, address, req->param, chunk);
        left -= chunk;
        address += chunk;
    }

    nrf5340_ipc_flash_std_resp(req, rc, NULL, 0);
}

static struct os_event flash_write_event = {
    .ev_cb = flash_write_cb,
};

static void
nrf5340_ipc_flash_server_write(struct ipc_msg *req)
{
    flash_write_event.ev_arg = req;
    os_eventq_put(os_eventq_dflt_get(), &flash_write_event);
}

static void
flash_erase_cb(struct os_event *event)
{
    int rc;
    struct ipc_msg *req = event->ev_arg;
    const uint8_t *p = req->param;

    rc = hal_flash_erase_sector(0, get32(&p));

    nrf5340_ipc_flash_std_resp(req, rc, NULL, 0);
}

static struct os_event erase_event = {
    .ev_cb = flash_erase_cb,
};

static void
nrf5340_ipc_flash_server_erase(struct ipc_msg *req)
{
    nrf5340_ipc_flash_server_read(&req->param, sizeof(uint32_t));
    erase_event.ev_arg = req;
    os_eventq_put(os_eventq_dflt_get(), &erase_event);
}

static int
nrf5340_ipc_flash_not_supported_resp(struct ipc_msg *req)
{
    ipc_nrf5340_consume(SERVER_IN_CHANNEL, req->hdr.msg_len - HDR_SZ);

    return nrf5340_ipc_flash_std_resp(req, SYS_ENOTSUP, NULL, 0);
}

void
ipc_nrf5340_flash_server_cb(int channel, void *arg)
{
    uint16_t available;
    uint16_t payload_size;

    assert(channel == SERVER_IN_CHANNEL);

    available = ipc_nrf5340_available(SERVER_IN_CHANNEL);

    if (server_req.hdr.type == 0) {
        if (available < HDR_SZ) {
            return;
        }
        nrf5340_ipc_flash_server_read(&server_req, HDR_SZ);
        available -= HDR_SZ;
    }

    payload_size = server_req.hdr.msg_len - HDR_SZ;

    /* Make sure whole message is ready */
    if (available < payload_size) {
        return;
    }

    switch (server_req.hdr.type) {
    case FLASH_OP_READ:
        nrf5340_ipc_flash_read_resp(&server_req);
        break;
    case FLASH_OP_WRITE:
        nrf5340_ipc_flash_server_write(&server_req);
        break;
    case FLASH_OP_ERASE_SECTOR:
        nrf5340_ipc_flash_server_erase(&server_req);
        break;
    case FLASH_OP_INFO:
        nrf5340_ipc_flash_info_resp(&server_req);
        break;
    default:
        nrf5340_ipc_flash_not_supported_resp(&server_req);
    }
}

void
ipc_nrf5340_flash_server_init(void)
{
    ipc_nrf5340_recv(SERVER_IN_CHANNEL, ipc_nrf5340_flash_server_cb, NULL);
}
#endif
