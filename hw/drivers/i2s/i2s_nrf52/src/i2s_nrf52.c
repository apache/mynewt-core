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

#include <os/mynewt.h>
#include <bsp/bsp.h>
#include <i2s/i2s.h>
#include <i2s/i2s_driver.h>
#include <i2s_nrf52/i2s_nrf52.h>
#include <nrfx/drivers/include/nrfx_i2s.h>

struct nrf52_i2s {
    nrfx_i2s_config_t nrfx_i2s_cfg;
    bool running;
    int8_t nrfx_queued_count;
    struct i2s *i2s;
    struct i2s_sample_buffer *nrfx_buffers[2];
};

static struct nrf52_i2s nrf52_i2s;

static void
nrfx_add_buffer(struct i2s *i2s, struct i2s_sample_buffer *buffer)
{
    nrfx_i2s_buffers_t nrfx_buffers = {0};
    nrfx_err_t err;

    assert(i2s != NULL);
    if (buffer == NULL) {
        return;
    }

    if (i2s->direction == I2S_OUT || i2s->direction == I2S_OUT_IN) {
        nrfx_buffers.p_tx_buffer = buffer->sample_data;
    }
    if (i2s->direction == I2S_IN || i2s->direction == I2S_OUT_IN) {
        nrfx_buffers.p_rx_buffer = buffer->sample_data;
    }

    assert(nrf52_i2s.nrfx_queued_count < 2);
    assert(nrf52_i2s.nrfx_buffers[nrf52_i2s.nrfx_queued_count] == NULL);

    nrf52_i2s.nrfx_buffers[nrf52_i2s.nrfx_queued_count] = buffer;
    nrf52_i2s.nrfx_queued_count++;
    if (nrf52_i2s.nrfx_queued_count == 1) {
        i2s_driver_state_changed (i2s, I2S_STATE_RUNNING);
        err = nrfx_i2s_start(&nrfx_buffers, buffer->sample_count * i2s->sample_size_in_bytes / 4, 0);
    } else {
        err = nrfx_i2s_next_buffers_set(&nrfx_buffers);
    }

    assert(err == NRFX_SUCCESS);
}

static void
feed_nrfx(void)
{
    struct i2s_sample_buffer *buffer;

    buffer = i2s_driver_buffer_get(nrf52_i2s.i2s);
    nrfx_add_buffer(nrf52_i2s.i2s, buffer);
}

static void
nrf52_i2s_data_handler(const nrfx_i2s_buffers_t *p_released, uint32_t status)
{
    struct i2s_sample_buffer *buffer;

    if (p_released != NULL &&
        (p_released->p_rx_buffer != NULL || p_released->p_tx_buffer != NULL)) {
        nrf52_i2s.nrfx_queued_count--;
        assert(nrf52_i2s.nrfx_queued_count >= 0);
        buffer = nrf52_i2s.nrfx_buffers[0];
        assert(buffer->sample_data == p_released->p_tx_buffer || buffer->sample_data == p_released->p_rx_buffer);
        nrf52_i2s.nrfx_buffers[0] = nrf52_i2s.nrfx_buffers[1];
        nrf52_i2s.nrfx_buffers[1] = NULL;
        i2s_driver_buffer_put(nrf52_i2s.i2s, buffer);
    }
    if (nrf52_i2s.running && nrf52_i2s.nrfx_queued_count < 2) {
        assert(nrf52_i2s.nrfx_buffers[1] == NULL);
        feed_nrfx();
    }
    if (status == NRFX_I2S_STATUS_TRANSFER_STOPPED) {
        i2s_driver_state_changed(nrf52_i2s.i2s, I2S_STATE_STOPPED);
    }
}

static int
nrf52_i2s_init(struct i2s *i2s, const struct i2s_cfg *cfg)
{
    int rc;

    nrf52_i2s.i2s = i2s;

    NVIC_SetVector(nrfx_get_irq_number(NRF_I2S), (uint32_t) nrfx_i2s_irq_handler);

    nrf52_i2s.nrfx_i2s_cfg = cfg->nrfx_i2s_cfg;
    switch (cfg->nrfx_i2s_cfg.sample_width) {
    case NRF_I2S_SWIDTH_8BIT:
        i2s->sample_size_in_bytes = 1;
        break;
    case NRF_I2S_SWIDTH_16BIT:
        i2s->sample_size_in_bytes = 2;
        break;
    case NRF_I2S_SWIDTH_24BIT:
        i2s->sample_size_in_bytes = 4;
        break;
    }

    i2s->direction = I2S_INVALID;
    if (cfg->nrfx_i2s_cfg.sdin_pin != NRFX_I2S_PIN_NOT_USED) {
        i2s->direction = I2S_IN;
    }
    if (cfg->nrfx_i2s_cfg.sdout_pin != NRFX_I2S_PIN_NOT_USED) {
        i2s->direction |= I2S_OUT;
    }

    rc = i2s_init(i2s, cfg->pool);

    if (rc != OS_OK) {
        nrfx_i2s_uninit();
        goto end;
    }

    i2s->sample_rate = cfg->sample_rate;
    i2s->driver_data = &nrf52_i2s;
end:
    return rc;
}

int
i2s_create(struct i2s *i2s, const char *name, const struct i2s_cfg *cfg)
{
    return os_dev_create(&i2s->dev, name, OS_DEV_INIT_PRIMARY,
                         100, (os_dev_init_func_t)nrf52_i2s_init, (void *)cfg);
}

int
i2s_driver_stop(struct i2s *i2s)
{
    struct i2s_sample_buffer *buffer;

    nrf52_i2s.running = false;
    nrfx_i2s_stop();

    assert(nrf52_i2s.i2s->state == I2S_STATE_STOPPED);

    while (NULL != (buffer = i2s_driver_buffer_get(i2s))) {
        i2s_driver_buffer_put(i2s, buffer);
    }

    return 0;
}

static void
nrf52_select_i2s_clock_cfg(nrfx_i2s_config_t *cfg, uint32_t sample_rate)
{
    cfg->ratio = NRF_I2S_RATIO_32X;
    switch (sample_rate) {
    case 16000:
        cfg->mck_setup = NRF_I2S_MCK_32MDIV63;
        break;
    case 22500:
        cfg->mck_setup = NRF_I2S_MCK_32MDIV42;
        break;
    case 32000:
        cfg->mck_setup = NRF_I2S_MCK_32MDIV31;
        break;
    case 441000:
        cfg->mck_setup = NRF_I2S_MCK_32MDIV23;
        break;
    case 48000:
        cfg->ratio = NRF_I2S_RATIO_48X;
        cfg->mck_setup = NRF_I2S_MCK_32MDIV21;
        break;
    default:
        assert(0);
        cfg->mck_setup = NRF_I2S_MCK_32MDIV63;
    }
}

int
i2s_driver_start(struct i2s *i2s)
{
    int rc = 0;

    if (!nrf52_i2s.running) {
        nrf52_i2s.running = true;
        nrf52_select_i2s_clock_cfg(&nrf52_i2s.nrfx_i2s_cfg, i2s->sample_rate);
        nrfx_i2s_init(&nrf52_i2s.nrfx_i2s_cfg, nrf52_i2s_data_handler);

        assert(nrf52_i2s.nrfx_buffers[0] == NULL);
        assert(nrf52_i2s.nrfx_buffers[1] == NULL);
        assert(!STAILQ_EMPTY(&i2s->driver_queue));

        nrf52_i2s.nrfx_queued_count = 0;
        feed_nrfx();
    }
    return rc;
}

void
i2s_driver_buffer_queued(struct i2s *i2s)
{
    if (nrf52_i2s.nrfx_queued_count < 2 && nrf52_i2s.running) {
        feed_nrfx();
    }
}

int
i2s_driver_suspend(struct i2s *i2s, os_time_t timeout, int arg)
{
    return OS_OK;
}

int
i2s_driver_resume(struct i2s *i2s)
{
    return OS_OK;
}
