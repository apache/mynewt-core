/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _UTILS_SLAB_H_
#define _UTILS_SLAB_H_

#include <stddef.h>
#include <stdint.h>

#include "osdp/osdp_utils.h"

typedef struct {
    uint8_t *blob;
    size_t size;
    size_t count;
    uint32_t *alloc_map;
} slab_t;

/**
 * @brief Macro helper for seting up global/local non-alloced instance of
 * stab_t. This is useful when dynamic allocation as done by slab_init() is
 * not possible/acceptable. A stab_t instance created with this macro, does
 * not need to slab_del().
 *
 * @note When using this macro, the blocks are not aligned and the user must
 * make sure the `type` param is properly aligned.
 */
#define SLAB_DEF(name, type, num)                   \
    uint8_t name ## _slab_blob[sizeof(type) * num];         \
    uint32_t name ## _slab_alloc_map[(num + 31) / 32] = { 0 };  \
    slab_t name = {                         \
        .blob = name ## _slab_blob,             \
        .size = sizeof(type),                   \
        .count = num,                       \
        .alloc_map = name ## _slab_alloc_map,           \
    };

/**
 * @brief Initializes a resource pool of `count` slabs of at-least
 * `size` bytes long. This method dynamically allocates resources
 * and must be paired with a call to slab_del() when the slab_t is
 * no longer needed.
 *
 * @return -1 on errors
 * @return  0 on success
 */
int slab_init(slab_t *slab, size_t size, size_t count);

/**
 * @brief Releases the entire allocation pool held by slab_t. All
 * previously issued slabs are invalid after this call.
 */
void slab_del(slab_t *slab);

/**
 * @brief Allocates a slab of memory from the resource pool held at
 * slab_t. The allocated block is at least `size` bytes large is
 * guaranteed to be aligned to a power the nearest power of 2.
 *
 * @return -1 on errors
 * @return  0 on success
 */
int slab_alloc(slab_t *slab, void **p);

/**
 * @brief Releases a slab that was previously allocated by a call
 * to slab_alloc(). This method can fail if the pointer passed did
 * not belong to the slab pool of slab_t.
 *
 * @return -1 on errors
 * @return  0 on success
 */
int slab_free(slab_t *slab, void *block);

#endif /* _UTILS_SLAB_H_ */
