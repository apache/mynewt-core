/*
 * Copyright (c) 2004, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/**
 * \defgroup memb Memory block management functions
 *
 * The memory block allocation routines provide a simple yet powerful
 * set of functions for managing a set of memory blocks of fixed
 * size. A set of memory blocks is statically declared with the
 * OC_MEMB() macro. Memory blocks are allocated from the declared
 * memory by the oc_memb_alloc() function, and are deallocated with the
 * oc_memb_free() function.
 *
 */

#ifndef OC_MEMB_H
#define OC_MEMB_H

#define CC_CONCAT2(s1, s2) s1##s2
/**
 * A C preprocessing macro for concatenating two preprocessor tokens.
 *
 * We need use two macros (CC_CONCAT and CC_CONCAT2) in order to allow
 * concatenation of two \#defined macros.
 */
#define CC_CONCAT(s1, s2) CC_CONCAT2(s1, s2)

/**
 * Declare a memory block.
 *
 * This macro is used to statically declare a block of memory that can
 * be used by the block allocation functions. The macro statically
 * declares a C array with a size that matches the specified number of
 * blocks and their individual sizes.
 *
 * Example:
 \code
 MEMB(connections, struct connection, 16);
 \endcode
 *
 * \param name The name of the memory block (later used with
 * oc_memb_init(), oc_memb_alloc() and oc_memb_free()).
 *
 * \param structure The name of the struct that the memory block holds
 *
 * \param num The total number of memory chunks in the block.
 *
 */
#define OC_MEMB(name, structure, num)                                          \
  static char CC_CONCAT(name, _memb_count)[num];                               \
  static structure CC_CONCAT(name, _memb_mem)[num];                            \
  static struct oc_memb name = { sizeof(structure), num,                       \
                                 CC_CONCAT(name, _memb_count),                 \
                                 (void *)CC_CONCAT(name, _memb_mem) }

struct oc_memb
{
  unsigned short size;
  unsigned short num;
  char *count;
  void *mem;
};

/**
 * Initialize a memory block that was declared with MEMB().
 *
 * \param m A memory block previously declared with MEMB().
 */
void oc_memb_init(struct oc_memb *m);

/**
 * Allocate a memory block from a block of memory declared with MEMB().
 *
 * \param m A memory block previously declared with MEMB().
 */
void *oc_memb_alloc(struct oc_memb *m);

/**
 * Deallocate a memory block from a memory block previously declared
 * with MEMB().
 *
 * \param m m A memory block previously declared with MEMB().
 *
 * \param ptr A pointer to the memory block that is to be deallocated.
 *
 * \return The new reference count for the memory block (should be 0
 * if successfully deallocated) or -1 if the pointer "ptr" did not
 * point to a legal memory block.
 */
char oc_memb_free(struct oc_memb *m, void *ptr);

int oc_memb_inmemb(struct oc_memb *m, void *ptr);

int oc_memb_numfree(struct oc_memb *m);

#endif /* OC_MEMB_H */
