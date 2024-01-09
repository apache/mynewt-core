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
#include <drivers/include/nrfx_i2s.h>

struct nrf52_i2s {
    nrfx_i2s_t inst;
    nrfx_i2s_config_t nrfx_i2s_cfg;
    bool running;
    int8_t nrfx_queued_count;
    struct i2s *i2s;
    struct i2s_sample_buffer *nrfx_buffers[2];
};

static struct nrf52_i2s nrf52_i2s = {
    NRFX_I2S_INSTANCE(0),
};

static void
nrfx_add_buffer(struct i2s *i2s, struct i2s_sample_buffer *buffer)
{
    nrfx_i2s_buffers_t nrfx_buffers = {0};
    nrfx_err_t err;
    uint16_t buffer_size;

    assert(i2s != NULL);
    if (buffer == NULL) {
        return;
    }

    if (i2s->direction == I2S_OUT || i2s->direction == I2S_OUT_IN) {
        nrfx_buffers.p_tx_buffer = buffer->sample_data;
        buffer_size = buffer->sample_count * i2s->sample_size_in_bytes / 4;
    } else {
        buffer_size = buffer->capacity * i2s->sample_size_in_bytes / 4;
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
        err = nrfx_i2s_start(&nrf52_i2s.inst, &nrfx_buffers, buffer_size, 0);
    } else {
        err = nrfx_i2s_next_buffers_set(&nrf52_i2s.inst, &nrfx_buffers);
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
        buffer->sample_count = buffer->capacity;
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

    NVIC_SetVector(nrfx_get_irq_number(NRF_I2S), (uint32_t)nrfx_i2s_0_irq_handler);

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
    if (cfg->nrfx_i2s_cfg.sdin_pin != NRF_I2S_PIN_NOT_CONNECTED) {
        i2s->direction = I2S_IN;
    }
    if (cfg->nrfx_i2s_cfg.sdout_pin != NRF_I2S_PIN_NOT_CONNECTED) {
        i2s->direction |= I2S_OUT;
    }

    rc = i2s_init(i2s, cfg->pool);

    if (rc != OS_OK) {
        nrfx_i2s_uninit(&nrf52_i2s.inst);
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

    if (nrf52_i2s.running) {
        nrf52_i2s.running = false;
        nrfx_i2s_stop(&nrf52_i2s.inst);
    }

    while (NULL != (buffer = i2s_driver_buffer_get(i2s))) {
        i2s_driver_buffer_put(i2s, buffer);
    }

    return 0;
}

/* Settings are stored for following sampling frequencies:
 * 8000, 16000, 22050, 32000, 441000, 48000 */
struct i2s_clock_cfg {
    nrf_i2s_mck_t mck_setup;
    nrf_i2s_ratio_t ratio;
};

static const uint16_t sample_rates[] = { 8000, 16000, 22050, 32000, 44100, 48000 };

static const struct i2s_clock_cfg mck_for_8_16_bit_samples[] = {
    { NRF_I2S_MCK_32MDIV125, NRF_I2S_RATIO_32X}, /*  8000:  8000     LRCK error  0.0% */
    { NRF_I2S_MCK_32MDIV63, NRF_I2S_RATIO_32X},  /* 16000: 15873.016 LRCK error -0.8% */
    { NRF_I2S_MCK_32MDIV15, NRF_I2S_RATIO_96X},  /* 22050: 22222.222 LRCK error  0.8% */
    { NRF_I2S_MCK_32MDIV31, NRF_I2S_RATIO_32X},  /* 32000: 32258.065 LRCK error  0.8% */
    { NRF_I2S_MCK_32MDIV23, NRF_I2S_RATIO_32X},  /* 44100: 43478.261 LRCK error -1.4% */
    { NRF_I2S_MCK_32MDIV21, NRF_I2S_RATIO_32X}   /* 48000: 47619.048 LRCK error -0.8% */
};

static const struct i2s_clock_cfg mck_for_24_bit_samples[] = {
    { NRF_I2S_MCK_32MDIV21, NRF_I2S_RATIO_192X}, /*  8000:  7936.508 LRCK error -0.8% */
    { NRF_I2S_MCK_32MDIV42, NRF_I2S_RATIO_48X},  /* 16000: 15873.016 LRCK error -0.8% */
    { NRF_I2S_MCK_32MDIV30, NRF_I2S_RATIO_48X},  /* 22050: 22222.222 LRCK error  0.8% */
    { NRF_I2S_MCK_32MDIV21, NRF_I2S_RATIO_48X},  /* 32000: 31746.032 LRCK error -0.8% */
    { NRF_I2S_MCK_32MDIV15, NRF_I2S_RATIO_48X},  /* 44100: 44444.444 LRCK error  0.8% */
    { NRF_I2S_MCK_32MDIV15, NRF_I2S_RATIO_48X}   /* 48000: 44444.444 LRCK error -7.4% */
};

static void
nrf52_select_i2s_clock_cfg(nrfx_i2s_config_t *cfg, uint32_t sample_rate)
{
    int i;

    if (cfg->ratio != 0 || cfg->mck_setup != 0) {
        /* User provided custom clock setup, no need to use stock values */
        return;
    }
    for (i = 0; i < ARRAY_SIZE(sample_rates); ++i) {
        if (sample_rates[i] == sample_rate) {
            if (cfg->sample_width == NRF_I2S_SWIDTH_8BIT ||
                cfg->sample_width == NRF_I2S_SWIDTH_16BIT) {
                cfg->ratio = mck_for_8_16_bit_samples[i].ratio;
                cfg->mck_setup = mck_for_8_16_bit_samples[i].mck_setup;
            } else if (cfg->sample_width == NRF_I2S_SWIDTH_24BIT) {
                cfg->ratio = mck_for_24_bit_samples[i].ratio;
                cfg->mck_setup = mck_for_24_bit_samples[i].mck_setup;
            } else {
                /* Invalid value of sample_width */
                assert(0);
            }
            break;
        }
    }
    assert(cfg->mck_setup);
}

int
i2s_driver_start(struct i2s *i2s)
{
    int rc = 0;

    if (!nrf52_i2s.running) {
        nrf52_i2s.running = true;
        nrf52_select_i2s_clock_cfg(&nrf52_i2s.nrfx_i2s_cfg, i2s->sample_rate);
        nrfx_i2s_init(&nrf52_i2s.inst, &nrf52_i2s.nrfx_i2s_cfg, nrf52_i2s_data_handler);

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
