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
#include <string.h>
#include <stdio.h>

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "os/os.h"
#include "metrics/metrics.h"
#include "tinycbor/cbor.h"
#include "tinycbor/cbor_mbuf_writer.h"
#include "log/log.h"
#include "metrics_priv.h"

#define MBUF_MEMBLOCK_OVERHEAD \
    (sizeof(struct os_mbuf))

#define MEMPOOL_COUNT   MYNEWT_VAL(METRICS_POOL_COUNT)
#define MEMPOOL_SIZE    MYNEWT_VAL(METRICS_POOL_SIZE)

#define METRICS_TYPE_SERIES_MASK    0x80
#define METRICS_TYPE_SIGNED_MASK    0x40
#define METRICS_TYPE_SIZE_MASK      0x0f

union metrics_metric_val {
    uintptr_t notused;
    uint32_t val;
    struct os_mbuf *series;
};

struct metrics_event {
    struct metrics_event_hdr hdr;
    union metrics_metric_val vals[0];
};

/*
 * This is aligned size for mempool block to make sure we do not write series
 * data across mbuf boundary in chain - it simplifies code a lot.
 */
#define MEMPOOL_SIZE_ALIGNED \
    OS_ALIGN(MBUF_MEMBLOCK_OVERHEAD + MEMPOOL_SIZE, sizeof(uint32_t))

static os_membuf_t event_metric_data[ OS_MEMPOOL_SIZE(MEMPOOL_COUNT, MEMPOOL_SIZE) ];
static struct os_mbuf_pool event_metric_mbuf_pool;
static struct os_mempool event_metric_mempool;

int
metrics_event_init(struct metrics_event_hdr *hdr,
                  const struct metrics_metric_def *metrics, uint8_t count,
                  const char *name)
{
    assert((count > 0) && (count <= 32));

    memset(hdr, 0, sizeof(*hdr) + count * sizeof(union metrics_metric_val));
    hdr->name = name;
    hdr->count = count;
    hdr->defs = metrics;
    hdr->enabled = UINT32_MAX;

    return 0;
}

int
metrics_event_register(struct metrics_event_hdr *hdr)
{
#if MYNEWT_VAL(METRICS_CLI)
    return metrics_cli_register_event(hdr);
#else
    return SYS_ENOTSUP;
#endif
}

int
metrics_event_set_log(struct metrics_event_hdr *hdr, struct log *log,
                      int module, int level)
{
    hdr->log = log;
    hdr->log_module = module;
    hdr->log_level = level;

    return 0;
}

int
metrics_event_start(struct metrics_event_hdr *hdr, uint32_t timestamp)
{
    if (hdr->set) {
        metrics_event_end(hdr);
    }

    hdr->timestamp = timestamp;

    return 0;
}

int
metrics_event_end(struct metrics_event_hdr *hdr)
{
    struct metrics_event *em = (struct metrics_event *)hdr;
    const struct metrics_metric_def *def;
    union metrics_metric_val *v;
    struct os_mbuf *om;
    int ret;
    int i;

    ret = 0;

    if (hdr->log) {
        om = metrics_get_mbuf();
        if (om) {
            ret = metrics_event_to_cbor(hdr, om);
            if (ret == 0) {
                ret = log_append_mbuf_body(hdr->log, hdr->log_module,
                                           hdr->log_level, LOG_ETYPE_CBOR, om);
            } else {
                os_mbuf_free_chain(om);
            }
        } else {
            ret = SYS_ENOMEM;
        }
    }

    hdr->set = 0;

    for (i = 0; i < hdr->count; i++) {
        def = &hdr->defs[i];
        v = &em->vals[i];

        if (def->type & METRICS_TYPE_SERIES_MASK) {
            if (v->series) {
                os_mbuf_free_chain(v->series);
            }
            v->series = NULL;
        } else {
            v->val = 0;
        }
    }

    return ret;
}

int
metrics_set_state(struct metrics_event_hdr *hdr, uint8_t metric, bool state)
{
    assert(metric < hdr->count);

    if (state) {
        hdr->enabled |= (1 << metric);
    } else {
        hdr->enabled &= ~(1 << metric);
    }

    return 0;
}

int
metrics_set_state_mask(struct metrics_event_hdr *hdr, uint32_t mask)
{
    hdr->enabled |= mask;

    return 0;
}

int
metrics_clr_state_mask(struct metrics_event_hdr *hdr, uint32_t mask)
{
    hdr->enabled &= ~mask;

    return 0;
}

uint32_t
metrics_get_state_mask(struct metrics_event_hdr *hdr)
{
    return hdr->enabled;
}

static int
set_single_value(struct metrics_event_hdr *hdr, uint8_t metric,
                 uint32_t val)
{
    struct metrics_event *em = (struct metrics_event *)hdr;
    union metrics_metric_val *v;

    v = &em->vals[metric];
    v->val = val;

    hdr->set |= (1 << metric);

    return 0;
}

static int
set_series_value(struct metrics_event_hdr *hdr, uint8_t metric,
                 uint32_t val, uint8_t type)
{
    struct metrics_event *em = (struct metrics_event *)hdr;
    union metrics_metric_val *v;
    uint16_t type_len;

    v = &em->vals[metric];

    if (!v->series) {
        v->series = os_mbuf_get(&event_metric_mbuf_pool, 0);
    }

    val = htole32(val);
    type_len = type & METRICS_TYPE_SIZE_MASK;
    assert((type_len == 1) || (type_len == 2) || (type_len == 4));

    os_mbuf_append(v->series, &val, type_len);

    hdr->set |= (1 << metric);

    return 0;
}

int
metrics_set_value(struct metrics_event_hdr *hdr, uint8_t metric,
                 uint32_t val)
{
    const struct metrics_metric_def *def;

    assert(metric < hdr->count);

    if ((hdr->enabled & (1 << metric)) == 0) {
        return 0;
    }

    def = &hdr->defs[metric];

    if (def->type & METRICS_TYPE_SERIES_MASK) {
        return set_series_value(hdr, metric, val, def->type);
    } else {
        return set_single_value(hdr, metric, val);
    }
}

int
metrics_set_single_value(struct metrics_event_hdr *hdr, uint8_t metric, uint32_t val)
{
    const struct metrics_metric_def *def;

    assert(metric < hdr->count);

    def = &hdr->defs[metric];
    if (def->type & METRICS_TYPE_SERIES_MASK) {
        return SYS_EINVAL;
    }

    if ((hdr->enabled & (1 << metric)) == 0) {
        return 0;
    }

    return set_single_value(hdr, metric, val);
}

int
metrics_set_series_value(struct metrics_event_hdr *hdr, uint8_t metric, uint32_t val)
{
    const struct metrics_metric_def *def;

    assert(metric < hdr->count);

    def = &hdr->defs[metric];
    if ((def->type & METRICS_TYPE_SERIES_MASK) == 0) {
        return SYS_EINVAL;
    }

    if ((hdr->enabled & (1 << metric)) == 0) {
        return 0;
    }

    return set_series_value(hdr, metric, val, def->type);
}

static int
append_series_u8_to_cbor(CborEncoder *encoder, struct os_mbuf *om)
{
    uint8_t *ptr;

    while (om) {
        ptr = om->om_data;

        while (ptr < om->om_data + om->om_len) {
            if (cbor_encode_uint(encoder, *ptr)) {
                return SYS_EUNKNOWN;
            }
            ptr += sizeof(uint8_t);
        }

        om = SLIST_NEXT(om, om_next);
    }

    return 0;
}

static int
append_series_s8_to_cbor(CborEncoder *encoder, struct os_mbuf *om)
{
    uint8_t *ptr;

    while (om) {
        ptr = om->om_data;

        while (ptr < om->om_data + om->om_len) {
            if (cbor_encode_int(encoder, (int8_t)*ptr)) {
                return SYS_EUNKNOWN;
            }
            ptr += sizeof(uint8_t);
        }

        om = SLIST_NEXT(om, om_next);
    }

    return 0;
}

static int
append_series_u16_to_cbor(CborEncoder *encoder, struct os_mbuf *om)
{
    uint16_t u16;
    uint8_t *ptr;

    while (om) {
        ptr = om->om_data;

        while (ptr < om->om_data + om->om_len) {
            memcpy(&u16, ptr, sizeof(uint16_t));
            u16 = le16toh(u16);
            if (cbor_encode_uint(encoder, u16)) {
                return SYS_EUNKNOWN;
            }
            ptr += sizeof(uint16_t);
        }

        om = SLIST_NEXT(om, om_next);
    }

    return 0;
}

static int
append_series_s16_to_cbor(CborEncoder *encoder, struct os_mbuf *om)
{
    int16_t s16;
    uint8_t *ptr;

    while (om) {
        ptr = om->om_data;

        while (ptr < om->om_data + om->om_len) {
            memcpy(&s16, ptr, sizeof(int16_t));
            s16 = le16toh(s16);
            if (cbor_encode_int(encoder, s16)) {
                return SYS_EUNKNOWN;
            }
            ptr += sizeof(int16_t);
        }

        om = SLIST_NEXT(om, om_next);
    }

    return 0;
}

static int
append_series_u32_to_cbor(CborEncoder *encoder, struct os_mbuf *om)
{
    uint32_t u32;
    uint8_t *ptr;

    while (om) {
        ptr = om->om_data;

        while (ptr < om->om_data + om->om_len) {
            memcpy(&u32, ptr, sizeof(uint32_t));
            u32 = le32toh(u32);
            if (cbor_encode_uint(encoder, u32)) {
                return SYS_EUNKNOWN;
            }
            ptr += sizeof(uint32_t);
        }

        om = SLIST_NEXT(om, om_next);
    }

    return 0;
}

static int
append_series_s32_to_cbor(CborEncoder *encoder, struct os_mbuf *om)
{
    int32_t s32;
    uint8_t *ptr;

    while (om) {
        ptr = om->om_data;

        while (ptr < om->om_data + om->om_len) {
            memcpy(&s32, ptr, sizeof(int32_t));
            s32 = le32toh(s32);
            if (cbor_encode_int(encoder, s32)) {
                return SYS_EUNKNOWN;
            }
            ptr += sizeof(int32_t);
        }

        om = SLIST_NEXT(om, om_next);
    }

    return 0;
}

int
metrics_event_to_cbor(struct metrics_event_hdr *hdr, struct os_mbuf *om)
{
    struct metrics_event *em = (struct metrics_event *)hdr;
    const struct metrics_metric_def *def;
    union metrics_metric_val *v;
    struct cbor_mbuf_writer writer;
    struct CborEncoder encoder;
    struct CborEncoder map;
    struct CborEncoder arr;
    int i;
    int rc;

    cbor_mbuf_writer_init(&writer, om);
    cbor_encoder_init(&encoder, &writer.enc, 0);

    rc = cbor_encoder_create_map(&encoder, &map, CborIndefiniteLength);
    if (rc != 0) {
        return SYS_ENOMEM;
    }

    cbor_encode_text_stringz(&map, "ev");
    cbor_encode_text_stringz(&map, hdr->name);

    cbor_encode_text_stringz(&map, "ts");
    cbor_encode_uint(&map, hdr->timestamp);

    for (i = 0; i < hdr->count; i++) {
        /* Skip if not enabled */
        if ((hdr->enabled & (1 << i)) == 0) {
            continue;
        }

        def = &hdr->defs[i];
        v = &em->vals[i];
        cbor_encode_text_stringz(&map, def->name);

        /* Write as null if value was not set */
        if ((hdr->set & (1 << i)) == 0) {
            cbor_encode_null(&map);
            continue;
        }

        if ((def->type & METRICS_TYPE_SERIES_MASK) == 0) {
            if (def->type & METRICS_TYPE_SIGNED_MASK) {
                cbor_encode_int(&map, (int32_t)v->val);
            } else {
                cbor_encode_uint(&map, v->val);
            }
            continue;
        }

        rc = cbor_encoder_create_array(&map, &arr, CborIndefiniteLength);
        if (rc != 0) {
            return SYS_ENOMEM;
        }

        switch (def->type) {
        case METRICS_TYPE_SERIES_U8:
            append_series_u8_to_cbor(&arr, v->series);
            break;
        case METRICS_TYPE_SERIES_S8:
            append_series_s8_to_cbor(&arr, v->series);
            break;
        case METRICS_TYPE_SERIES_U16:
            append_series_u16_to_cbor(&arr, v->series);
            break;
        case METRICS_TYPE_SERIES_S16:
            append_series_s16_to_cbor(&arr, v->series);
            break;
        case METRICS_TYPE_SERIES_U32:
            append_series_u32_to_cbor(&arr, v->series);
            break;
        case METRICS_TYPE_SERIES_S32:
            append_series_s32_to_cbor(&arr, v->series);
            break;
        default:
            assert(0);
        }

        rc = cbor_encoder_close_container(&map, &arr);
        if (rc != 0) {
            return SYS_ENOMEM;
        }

        /*
         * Let's remove existing series chain to free space in case om is from
         * event_metric pool - assume we do this at the end of event so will
         * start over anyway.
         */
        if (om->om_omp == &event_metric_mbuf_pool) {
            os_mbuf_free_chain(v->series);
            v->series = NULL;
        }
    }

    rc = cbor_encoder_close_container(&encoder, &map);
    if (rc != 0) {
        return SYS_ENOMEM;
    }

    return 0;
}

struct os_mbuf *
metrics_get_mbuf(void)
{
    return os_mbuf_get(&event_metric_mbuf_pool, 0);
}

void
metrics_pkg_init(void)
{
    int rc;

    SYSINIT_ASSERT_ACTIVE();

    rc = os_mempool_init(&event_metric_mempool, MEMPOOL_COUNT, MEMPOOL_SIZE,
                         event_metric_data, "event_metric");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&event_metric_mbuf_pool, &event_metric_mempool,
                           MEMPOOL_SIZE, MEMPOOL_COUNT);
    assert(rc == 0);

#if MYNEWT_VAL(METRICS_CLI)
    metrics_cli_init();
#endif
}
