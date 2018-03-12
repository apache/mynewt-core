/*
 * malloc.c
 *
 * Very simple linked-list based malloc()/free().
 */

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "malloc.h"

/* Both the arena list and the free memory list are double linked
   list with head node.  This the head node. Note that the arena list
   is sorted in order of address. */
static struct free_arena_header __malloc_head = {
    {
        ARENA_TYPE_HEAD,
        0,
        &__malloc_head,
        &__malloc_head,
    },
    &__malloc_head,
    &__malloc_head
};

static bool malloc_lock_nop() {return true;}
static void malloc_unlock_nop() {}

static malloc_lock_t malloc_lock = &malloc_lock_nop;
static malloc_unlock_t malloc_unlock = &malloc_unlock_nop;

static inline void mark_block_dead(struct free_arena_header *ah)
{
#ifdef DEBUG_MALLOC
    ah->a.type = ARENA_TYPE_DEAD;
#endif
}

static inline void remove_from_main_chain(struct free_arena_header *ah)
{
    struct free_arena_header *ap, *an;

    mark_block_dead(ah);

    ap = ah->a.prev;
    an = ah->a.next;
    ap->a.next = an;
    an->a.prev = ap;
}

static inline void remove_from_free_chain(struct free_arena_header *ah)
{
    struct free_arena_header *ap, *an;

    ap = ah->prev_free;
    an = ah->next_free;
    ap->next_free = an;
    an->prev_free = ap;
}

static inline void remove_from_chains(struct free_arena_header *ah)
{
    remove_from_free_chain(ah);
    remove_from_main_chain(ah);
}

static void *__malloc_from_block(struct free_arena_header *fp, size_t size)
{
    size_t fsize;
    struct free_arena_header *nfp, *na, *fpn, *fpp;

    fsize = fp->a.size;

    /* We need the 2* to account for the larger requirements of a
       free block */
    if (fsize >= size + 2 * sizeof(struct arena_header)) {
        /* Bigger block than required -- split block */
        nfp = (struct free_arena_header *)((char *)fp + size);
        na = fp->a.next;

        nfp->a.type = ARENA_TYPE_FREE;
        nfp->a.size = fsize - size;
        fp->a.type = ARENA_TYPE_USED;
        fp->a.size = size;

        /* Insert into all-block chain */
        nfp->a.prev = fp;
        nfp->a.next = na;
        na->a.prev = nfp;
        fp->a.next = nfp;

        /* Replace current block on free chain */
        nfp->next_free = fpn = fp->next_free;
        nfp->prev_free = fpp = fp->prev_free;
        fpn->prev_free = nfp;
        fpp->next_free = nfp;
    } else {
        fp->a.type = ARENA_TYPE_USED; /* Allocate the whole block */
        remove_from_free_chain(fp);
    }

    return (void *)(&fp->a + 1);
}

static struct free_arena_header *__free_block(struct free_arena_header *ah)
{
    struct free_arena_header *pah, *nah;

    pah = ah->a.prev;
    nah = ah->a.next;
    if (pah->a.type == ARENA_TYPE_FREE &&
        (char *)pah + pah->a.size == (char *)ah) {
        /* Coalesce into the previous block */
        pah->a.size += ah->a.size;
        pah->a.next = nah;
        nah->a.prev = pah;
        mark_block_dead(ah);

        ah = pah;
        pah = ah->a.prev;
    } else {
        /* Need to add this block to the free chain */
        ah->a.type = ARENA_TYPE_FREE;

        ah->next_free = __malloc_head.next_free;
        ah->prev_free = &__malloc_head;
        __malloc_head.next_free = ah;
        ah->next_free->prev_free = ah;
    }

    /* In either of the previous cases, we might be able to merge
       with the subsequent block... */
    if (nah->a.type == ARENA_TYPE_FREE &&
        (char *)ah + ah->a.size == (char *)nah) {
        ah->a.size += nah->a.size;

        /* Remove the old block from the chains */
        remove_from_chains(nah);
    }

    /* Return the block that contains the called block */
    return ah;
}

void *malloc(size_t size)
{
    struct free_arena_header *fp;
    void *more_mem;
    extern void *_sbrk(int incr);

    if (size == 0 || size > (SIZE_MAX - sizeof(struct arena_header))) {
        return NULL;
    }

    /* Add the obligatory arena header, and round up */
    size = (size + 2 * sizeof(struct arena_header) - 1) & ARENA_SIZE_MASK;

    if (!malloc_lock())
        return NULL;

    void *result = NULL;
retry_alloc:
    for (fp = __malloc_head.next_free; fp->a.type != ARENA_TYPE_HEAD;
         fp = fp->next_free) {
        if (fp->a.size >= size) {
            /* Found fit -- allocate out of this block */
            result = __malloc_from_block(fp, size);
            break;
        }
    }
    if (result == NULL) {
        more_mem = _sbrk(size);
        if (more_mem != (void *)-1) {
            add_malloc_block(more_mem, size);
            goto retry_alloc;
        }
    }
    malloc_unlock();
    return result;
}

/* Call this to give malloc some memory to allocate from */
void add_malloc_block(void *buf, size_t size)
{
    struct free_arena_header *fp = buf;
    struct free_arena_header *pah;

    if (size < sizeof(struct free_arena_header))
        return; // Too small.

    /* Insert the block into the management chains.  We need to set
       up the size and the main block list pointer, the rest of
       the work is logically identical to free(). */
    fp->a.type = ARENA_TYPE_FREE;
    fp->a.size = size;

    if (!malloc_lock())
        return;

    /* We need to insert this into the main block list in the proper
       place -- this list is required to be sorted.  Since we most likely
       get memory assignments in ascending order, search backwards for
       the proper place. */
    for (pah = __malloc_head.a.prev; pah->a.type != ARENA_TYPE_HEAD;
         pah = pah->a.prev) {
        if (pah < fp)
            break;
    }

    /* Now pah points to the node that should be the predecessor of
       the new node */
    fp->a.next = pah->a.next;
    fp->a.prev = pah;
    pah->a.next = fp;
    fp->a.next->a.prev = fp;

    /* Insert into the free chain and coalesce with adjacent blocks */
    fp = __free_block(fp);

    malloc_unlock();
}

void free(void *ptr)
{
    struct free_arena_header *ah;

    if (!ptr)
        return;

    ah = (struct free_arena_header *)
        ((struct arena_header *)ptr - 1);

#ifdef DEBUG_MALLOC
    assert(ah->a.type == ARENA_TYPE_USED);
#endif

    if (!malloc_lock())
        return;

    /* Merge into adjacent free blocks */
    ah = __free_block(ah);
    malloc_unlock();
}

void get_malloc_memory_status(size_t *free_bytes, size_t *largest_block)
{
    struct free_arena_header *fp;
    *free_bytes = 0;
    *largest_block = 0;

    if (!malloc_lock())
        return;

    for (fp = __malloc_head.next_free; fp->a.type != ARENA_TYPE_HEAD; fp = fp->next_free) {
        *free_bytes += fp->a.size;
        if (fp->a.size >= *largest_block) {
            *largest_block = fp->a.size;
        }
    }

    malloc_unlock();
}

void set_malloc_locking(malloc_lock_t lock, malloc_unlock_t unlock)
{
    if (lock)
        malloc_lock = lock;
    else
        malloc_lock = &malloc_lock_nop;

    if (unlock)
        malloc_unlock = unlock;
    else
        malloc_unlock = &malloc_unlock_nop;
}
