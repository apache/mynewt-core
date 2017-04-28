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

#define CFG1_MMUSSHIFT  25

        .weak __pic32_tlb_init_values_count
        .weak __pic32_tlb_init_values_begin
        .weak __pic32_tlb_init_values_end

        .section .tlb.__pic32_tlb_init_ebi_sqi, code, keep
        .set nomips16
        .set noreorder

        .global __pic32_tlb_init_ebi_sqi
        .ent __pic32_tlb_init_ebi_sqi
        
__pic32_tlb_init_ebi_sqi:
        /* Determine if we have a TLB */
        /* Read C0 Config0 */
        mfc0 v1, _CP0_CONFIG
        /* check MT field */
        ext v1, v1, 7, 3
        li a3, 0x1
        bne v1, a3, __done_init_tlb

        /* read C0 Config1 */
        mfc0 v0, _CP0_CONFIG1

__start_init_tlb:
        /* Extract MMU Size */
        ext v1, v0, CFG1_MMUSSHIFT, 6

        mtc0 zero, _CP0_ENTRYLO0
        mtc0 zero, _CP0_ENTRYLO1
        mtc0 zero, _CP0_PAGEMASK
        mtc0 zero, _CP0_WIRED

        move a0, $0
        lui a0, 0x8000

__next_tlb_entry_pair:
        mtc0 v1, _CP0_INDEX
        mtc0 a0, _CP0_ENTRYHI

        ehb
        tlbwi

        add   a0, (2<<13)
        add   v1, -1
        bne   v1, zero, __next_tlb_entry_pair
        nop

        /* Get the count of the entries in the table from a linker-script symbol */
__count_tlb_init_values:
        lui   a0, %hi(__pic32_tlb_init_values_count)
        ori   a0, %lo(__pic32_tlb_init_values_count)
        
__device_tlb_entries:
        /* Set lowest count (a0) TLB entries as fixed (entry 0 and entry 1) */
        mtc0  a0, _CP0_WIRED
        
        /* Point a1 to the beginning of the linker-script generated table */
        la   a1, __pic32_tlb_init_values_begin
        la   a2, __pic32_tlb_init_values_end
        move a3, zero                               /* index */

        /* Loop through the tables until we have done the correct number of iterations. */
        /* When the current pointer (a1) points to the end of the table (a2) we are done. */
1:
        beq  a1, a2, __done_init_tlb    /* done */

        /* Create one page table entry consisting of two 16MB physical pages */
        /* 16MB page mask for Entry 0 */
        lui	t1,0x01FF                            /* in branch delay slot */
        ori	t1,t1,0xE000

        mtc0 t1, _CP0_PAGEMASK

        /* TLB entry index */
        mtc0 a3, _CP0_INDEX
        
        /* Each entry consists of three words */
        lw   t0, 0(a1)                               /* ENTRYHI  */
        lw   t1, 4(a1)                               /* ENTRYLO0 */
        lw   t2, 8(a1)                               /* ENTRYLO1 */

        /* physical base address <31:12>, cacheable (write back), dirty, global set */
        /* First 32MB page in Entrylo0 */
        mtc0 t1, _CP0_ENTRYLO0

        /* Second 32MB page in EntryLo1 */
        mtc0 t2, _CP0_ENTRYLO1

        /*  virtual base address (ASID 0) */
        mtc0 t0, _CP0_ENTRYHI
        ehb
        tlbwi

        /* Next entry */
        add a1, 12         /* Point to next entry (WORD_SIZE * 3) */
        add a3, 1          /* Increment index */
        b   1b
        nop

__done_init_tlb:
        jr ra
        nop

.size __pic32_tlb_init_ebi_sqi,.-__pic32_tlb_init_ebi_sqi; .end __pic32_tlb_init_ebi_sqi;

