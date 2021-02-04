/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <string.h>

#include "osdp/slab.h"

int
slab_init(slab_t *slab, size_t size, size_t count)
{
    slab->size = round_up_pow2(size);
    slab->blob = safe_calloc(count, slab->size);
    slab->alloc_map = safe_calloc((count + 31) / 32, sizeof(uint32_t));
    slab->count = count;
    return 0;
}

void
slab_del(slab_t *slab)
{
    safe_free(slab->blob);
    safe_free(slab->alloc_map);
    memset(slab, 0, sizeof(slab_t));
}

int
slab_alloc(slab_t *slab, void **p)
{
    size_t i = 0, offset = 0;

    while (i < slab->count &&
           slab->alloc_map[offset] & (1L << (i & 0x1f))) {
        if ((i & 0x1f) == 0x1f) {
            offset++;
        }
        i++;
    }
    if (i >= slab->count) {
        return -1;
    }
    slab->alloc_map[offset] |= 1L << (i & 0x1f);
    *p = slab->blob + (slab->size * i);
    memset(*p, 0, slab->size);
    return 0;
}

int
slab_free(slab_t *slab, void *p)
{
    size_t i;

    for (i = 0; i < slab->count; i++) {
        if ((slab->blob + (slab->size * i)) == p) {
            break;
        }
    }
    if (i >= slab->count) {
        return -1;
    }
    if (!(slab->alloc_map[i / 32] & (1L << (i & 0x1f)))) {
        return -2;
    }
    slab->alloc_map[i / 32] &= ~(1L << (i & 0x1f));
    return 0;
}
