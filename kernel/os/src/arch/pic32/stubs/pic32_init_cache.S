/*-------------------------------------------------------------------------
 *
 * Copyright (c) 2014, Microchip Technology Inc. and its subsidiaries ("Microchip")
 * All rights reserved.
 * 
 * This software is developed by Microchip Technology Inc. and its
 * subsidiaries ("Microchip").
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.      Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 * 2.      Redistributions in binary form must reproduce the above
 *         copyright notice, this list of conditions and the following
 *         disclaimer in the documentation and/or other materials provided
 *         with the distribution.
 * 3.      Microchip's name may not be used to endorse or promote products
 *         derived from this software without specific prior written
 *         permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MICROCHIP "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL MICROCHIP BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING BUT NOT LIMITED TO
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWSOEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *-------------------------------------------------------------------------*/

#include <xc.h>
#include <cp0defs.h>

        /* Symbols defined in linker script */
        .weak __pic32_init_cache_program_base_addr
        .weak __pic32_init_cache_data_base_addr

/* Cache Coherency Attributes */
#define _CACHE_WRITEBACK_WRITEALLOCATE      3
#define _CACHE_WRITETHROUGH_WRITEALLOCATE   1
#define _CACHE_WRITETHROUGH_NOWRITEALLOCATE 0
#define _CACHE_DISABLE                      2

#ifndef _CP0_ERRCTL
#define _CP0_ERRCTL                         $26, 0
#endif

#ifndef _CP0_TAGLO
#define _CP0_TAGLO                          $28, 0
#endif

/* Set __PIC32_CACHE_MODE to the desired coherency attribute */
#define __PIC32_CACHE_MODE _CACHE_WRITEBACK_WRITEALLOCATE

/* ==================================== */
#define Index_Store_Tag_I 0x08
#define Index_Store_Tag_D 0x09

#define tmp t0
#define cfg t1
#define icachesize t2
#define ilinesize t3
#define iways t4
#define dcachesize t5
#define dlinesize t6
#define dways t7
#define save_ra v1

#define INIT_L1_CACHE
#if defined(INIT_L1_CACHE)
.sdata; .globl __pic32_icache_size; .type __pic32_icache_size,@object; .size __pic32_icache_size,4; __pic32_icache_size:; .word -1
.sdata; .globl __pic32_icache_linesize; .type __pic32_icache_linesize,@object; .size __pic32_icache_linesize,4; __pic32_icache_linesize:; .word -1
.sdata; .globl __pic32_icache_ways; .type __pic32_icache_ways,@object; .size __pic32_icache_ways,4; __pic32_icache_ways:; .word 1

.sdata; .globl __pic32_dcache_size; .type __pic32_dcache_size,@object; .size __pic32_dcache_size,4; __pic32_dcache_size:; .word -1
.sdata; .globl __pic32_dcache_linesize; .type __pic32_dcache_linesize,@object; .size __pic32_dcache_linesize,4; __pic32_dcache_linesize:; .word -1
.sdata; .globl __pic32_dcache_ways; .type __pic32_dcache_ways,@object; .size __pic32_dcache_ways,4; __pic32_dcache_ways:; .word 1

.sdata; .globl __pic32_scache_size; .type __pic32_scache_size,@object; .size __pic32_scache_size,4; __pic32_scache_size:; .word -1
.sdata; .globl __pic32_scache_linesize; .type __pic32_scache_linesize,@object; .size __pic32_scache_linesize,4; __pic32_scache_linesize:; .word -1
.sdata; .globl __pic32_scache_ways; .type __pic32_scache_ways,@object; .size __pic32_scache_ways,4; __pic32_scache_ways:; .word 1

.section .cache_init.cache, code
        .set nomips16
        .ent __size_cache; __size_cache:
        mfc0 cfg,_CP0_CONFIG

        li icachesize,0
        li ilinesize,0
        li dcachesize,0
        li dlinesize,0

        /* Check that we have Config1 */
        and tmp,cfg,_CP0_CONFIG_M_MASK
        mfc0 cfg,_CP0_CONFIG1
        beqz tmp,9f
        nop

        /* Get icache line size (log2) */
        and tmp,cfg,_CP0_CONFIG1_IL_MASK
        srl tmp,_CP0_CONFIG1_IL_POSITION
        beqz tmp,8f # no i-cache
        addu tmp,1

        /* Get number of icache ways */
        and iways,cfg,_CP0_CONFIG1_IA_MASK
        srl iways,_CP0_CONFIG1_IA_POSITION
        addu iways,1
        move icachesize,iways

        /* total icache size = lines/way * linesize *ways */
        li ilinesize,1
        sll ilinesize,tmp
        sll icachesize,tmp

        /* get icache lines per way */
        and tmp,cfg,_CP0_CONFIG1_IS_MASK
        srl tmp,_CP0_CONFIG1_IS_POSITION
        addu tmp,6
        sll icachesize,tmp

        /* Get dcache line size (log2) */
8:      and tmp,cfg,_CP0_CONFIG1_DL_MASK
        srl tmp,_CP0_CONFIG1_DL_POSITION
        beqz tmp,8f # no d-cache
        addu tmp,1

        /* Get number of dcache ways */
        and dways,cfg,_CP0_CONFIG1_DA_MASK
        srl dways,_CP0_CONFIG1_DA_POSITION
        addu dways,1
        move dcachesize,dways

        /* Total dcache size = lines/way * linesize * ways */
        li dlinesize,1
        sll dlinesize,tmp
        sll dcachesize,tmp

        and tmp,cfg,_CP0_CONFIG1_DS_MASK
        srl tmp,_CP0_CONFIG1_DS_POSITION
        addu tmp,6
        sll dcachesize,tmp

#undef cfg
#undef tmp

8:
9:   j ra
     nop
    .size __size_cache,.-__size_cache; .end __size_cache;

/*
 * void __pic32_size_cache()
 *
 * Work out size of I & D caches (assume already initialized)
 */
        .section .cache_init.pic32_size_cache, code
        .set nomips16
        .globl __pic32_size_cache; .ent __pic32_size_cache;

__pic32_size_cache:
        lw t0,__pic32_icache_size
        move a3,ra
        bgtz t0,8f # already known?

        bal __size_cache
        move ra,a3

..savesize:
        sw icachesize,__pic32_icache_size
        sw dcachesize,__pic32_dcache_size
        sw ilinesize,__pic32_icache_linesize
        sw dlinesize,__pic32_dcache_linesize
        sw iways,__pic32_icache_ways
        sw dways,__pic32_dcache_ways

8:      j ra
        nop
        .size __pic32_size_cache,.-__pic32_size_cache; .end __pic32_size_cache

/*
 * void __pic32_init_cache()
 *
 * Work out size and initialize I & D caches.
 */
        .section .cache_init.pic32_init_cache, code
        .set nomips16
        .globl __pic32_init_cache; .ent __pic32_init_cache;
__pic32_init_cache:

        move save_ra,ra
        bal __size_cache

        /* Run uncached */
        .set noreorder
        .set nomacro
#if 0
        bal 1f
        #
        li cfg,0xa0000000
1:      or cfg,ra
        addu cfg,16
        jr cfg
        move ra,v1
#endif

        /*
         * The caches may be in an indeterminate state, so we force an
         * invalidate, load/fill, and invalidate for each line.
         */

        /* Disable all i/u and cache exceptions */
        .set macro
        .set noreorder
        # Disable interrupts and set UM=1
        # Save current status in tmp
        mfc0 t0,_CP0_STATUS
        li t1,~_CP0_STATUS_IE_MASK
        and t1,t0
        or t1,_CP0_STATUS_ERL_MASK
        mtc0 t1,_CP0_STATUS
        ehb

        mtc0 zero,_CP0_ERRCTL
        mtc0 zero,_CP0_TAGLO # 4K taglo / 2*K itaglo
        ehb

        /* Initialize primary instruction cache */
        .set noreorder
4:      la a0,__pic32_init_cache_program_base_addr
        bne a0,zero,0f
        /* Use a default if the symbol is not defined */
        li a0,0x9D000000 /* KSEG0_PROGRAM_BASE */
0:      beqz icachesize,8f    # icachesize
        addu a1,a0,icachesize # limit = base + icachesize
1:      addu a0,ilinesize  # line size
        bne a0,a1,1b
        cache Index_Store_Tag_I,-4(a0) # BDSLOT: clear tag


        /* Initialize primary data cache */
        .set noreorder
8:      la a0,__pic32_init_cache_data_base_addr
        bne a0,zero,0f
        /* Use a default if the symbol is not defined */
        li a0,0x80000000  /* KSEG_DATA_BASE */

0:      beqz dcachesize,8f
        addu a1,a0,dcachesize # limit = base + dcachesize
1:      addu a0,dlinesize
        bne a0,a1,1b
        cache Index_Store_Tag_D,-4(a0) # BDSLOT: clear tag

        .set reorder

8:      sync

        /* Store the sizes only after the caches are initialized */
4:      sw icachesize,__pic32_icache_size
        sw dcachesize,__pic32_dcache_size
        sw ilinesize,__pic32_icache_linesize
        sw dlinesize,__pic32_dcache_linesize
        sw iways,__pic32_icache_ways
        sw dways,__pic32_dcache_ways

        .set noreorder

        # restore status
        mtc0 t0,_CP0_STATUS
        ehb

        # Configure Cache Mode
        mfc0 t1, _CP0_CONFIG
        ori t1, _CP0_CONFIG_K0_MASK
        xori t1, _CP0_CONFIG_K0_MASK
        ori t1, __PIC32_CACHE_MODE
        mtc0 t1, _CP0_CONFIG
        ehb
        
         .set reorder

        move ra, save_ra
        jr ra
        nop
        .size __pic32_init_cache,.-__pic32_init_cache; .end __pic32_init_cache;
#endif /* INIT_L1_CACHE */

#undef _CACHE_WRITEBACK_WRITEALLOCATE
#undef _CACHE_WRITETHROUGH_WRITEALLOCATE
#undef _CACHE_WRITETHROUGH_NOWRITEALLOCATE
#undef _CACHE_DISABLE

#undef _CP0_ERRCTL
#undef _CP0_TAGLO
