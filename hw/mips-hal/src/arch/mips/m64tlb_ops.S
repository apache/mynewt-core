/*
 * Copyright 2015, Imagination Technologies Limited and/or its
 *                 affiliated group companies.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/


/*
 * m64tlb_ops.S: MIPS XPA TLB support functions
 *
 */

.set nomips16
#include <mips/m64tlb.h>
#include <mips/asm.h>
#include <mips/endian.h>

#define PTR_XO32_MTC0(top, bot, reg)	\
#if BYTE_ORDER == BIG_ENDIAN		\
	mtc0	bot, reg; 		\
	mthc0	top, reg; 		\
#else					\
	mtc0	top, reg;		\
	mthc0	bot, reg;		\
#endif

#define PTR_XO32_MFC0(top, bot, reg)	\
#if BYTE_ORDER == BIG_ENDIAN		\
	mfc0	bot, reg; 		\
	mfhc0	top, reg; 		\
#else					\
	mfc0	top, reg;		\
	mfhc0	bot, reg;		\
#endif

#define PTR_XO32_MTC0(top, bot, reg)	\
#if BYTE_ORDER == BIG_ENDIAN		\
	mtc0	bot, reg; 		\
	mthc0	top, reg; 		\
#else					\
	mtc0	top, reg;		\
	mthc0	bot, reg;		\
#endif

#define PTR_XO32_MFC0(top, bot, reg)	\
#if BYTE_ORDER == BIG_ENDIAN		\
	mfc0	bot, reg; 		\
	mfhc0	top, reg; 		\
#else					\
	mfc0	top, reg;		\
	mfhc0	bot, reg;		\
#endif

/*
 * void m64_tlbwi2(tlbhi64_t hi, tlblo64_t lo0, tlblo64_t lo1, unsigned long long mask,
 *		   unsigned int index)
 *
 * Writes hi,lo0,lo1 and msk in to the TLB entry specified by index.
 *
 */
LEAF(m64_tlbwi2)
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(a0, a1, C0_ENTRYHI)
	PTR_XO32_MTC0(a2, a3, C0_ENTRYLO0)

	PTR_L	t1, 20(sp)
	PTR_L	t2, 16(sp)
	PTR_XO32_MTC0(t2, t1, C0_ENTRYLO1)

	PTR_L	t1, 28(sp)
	PTR_L	t2, 24(sp)
	PTR_XO32_MTC0(t2, t1, C0_PAGEMASK)

	lw	ta0, 32(sp)
#else /* _MIPS_SIM==N32 || _MIPS_SIM==N64 */
	PTR_MTC0 a0, C0_ENTRYHI
	PTR_MTC0 a1, C0_ENTRYLO0
	PTR_MTC0 a2, C0_ENTRYLO1
	PTR_MTC0 a3, C0_PAGEMASK
#endif

	mtc0	ta0, C0_INDEX
	ehb			# mtc0, hazard barrier on tlbwi
	tlbwi
	.set	push
	.set	noreorder
	jr.hb	ra
	nop
	.set	pop

END(m64_tlbwi2)

/*
 * void m64_tlbwr2(tlbhi64_t hi, tlblo64_t lo0, tlblo64_t lo1, unsigned long long mask)
 *
 * Writes hi, lo0, lo1 and msk into the TLB entry specified by the
 * Random register.
 *
 */
LEAF(m64_tlbwr2)
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(a0, a1, C0_ENTRYHI)
	PTR_XO32_MTC0(a2, a3, C0_ENTRYLO0)

	PTR_L	t1, 20(sp)
	PTR_L	t2, 16(sp)
	PTR_XO32_MTC0(t2, t1, C0_ENTRYLO1)

	PTR_L	t1, 28(sp)
	PTR_L	t2, 24(sp)
	PTR_XO32_MTC0(t2, t1, C0_PAGEMASK)
#else
	PTR_MTC0 a0, C0_ENTRYHI
	PTR_MTC0 a1, C0_ENTRYLO0
	PTR_MTC0 a2, C0_ENTRYLO1
	PTR_MTC0 a3, C0_PAGEMASK
#endif
	ehb
	tlbwr
	.set	push
	.set	noreorder
	jr.hb	ra
	nop
	.set	pop

END(m64_tlbwr2)

/*
 * int m64_tlbrwr2(tlbhi64_t hi, tlblo64_t lo0, tlblo64_t lo1, unsigned long long mask)
 *
 * Probes the TLB for an entry matching hi and if present rewrites that
 * entry, otherwise updates a random entry. A safe way to update the TLB.
 *
 */
LEAF(m64_tlbrwr2)
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MFC0(t1, t0, C0_ENTRYHI)
	PTR_XO32_MTC0(a0, a1, C0_ENTRYHI)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MFC0 t0, C0_ENTRYHI
	PTR_MTC0 a0, C0_ENTRYHI
#endif /* _MIPS_SIM==_ABIO32 */
	ehb		# MTCO, hazard on tlbp

	tlbp
	ehb		# tlbp, hazard on MFCO $index

	mfc0	v0, C0_INDEX
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(a2, a3, C0_ENTRYLO0)

	PTR_L	t2, 20(sp)
	PTR_L	t3, 16(sp)
	PTR_XO32_MTC0(t3, t2, C0_ENTRYLO1)

	PTR_L	t2, 28(sp)
	PTR_L	t3, 24(sp)
	PTR_XO32_MTC0(t3, t2, C0_PAGEMASK)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MTC0 a1, C0_ENTRYLO0
	PTR_MTC0 a2, C0_ENTRYLO1
	PTR_MTC0 a3, C0_PAGEMASK
#endif /* _MIPS_SIM==_ABIO32 */
	ehb		# mtc0, hazard on tlbwi
	bltz	v0, 1f	# no matching entry

	tlbwi
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(t1, t0, C0_ENTRYHI)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MTC0 t0, C0_ENTRYHI
#endif /* _MIPS_SIM==_ABIO32 */
	.set	push
	.set	noreorder
	jr.hb	ra
	nop
	.set	pop

1:	tlbwr
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(t1, t0, C0_ENTRYHI)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MTC0 t0, C0_ENTRYHI
#endif /* _MIPS_SIM==_ABIO32 */
	.set	push
	.set	noreorder
	jr.hb	ra
	nop
	.set	pop

END(m64_tlbrwr2)

/*
 * void m64_tlbri2(tlbhi64_t *phi, tlblo64_t *plo0, tlblo64_t *plo1,
 *		   unsigned long long * mask, unsigned int index)
 *
 * Reads the TLB entry with specified by index, and returns the EntryHi, EntryLo0,
 * EntryLo1 and PageMask parts in *phi, *plo0, *plo1 and *pmsk respectively.
 *
 */
LEAF(m64_tlbri2)
#if _MIPS_SIM==_ABIO32
	PTR_L   ta0, 16(sp)	# index
#endif /* _MIPS_SIM==_ABIO32 */
	mtc0	ta0, C0_INDEX
	ehb			# mtc0, hazard on tlbr

	tlbr
	ehb			# tlbr, hazard on entry*, pagemask
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MFC0(t0, t1, C0_ENTRYHI)
	PTR_XO32_MFC0(t2, t3, C0_ENTRYLO0)
	PTR_XO32_MFC0(t4, t5, C0_ENTRYLO1)
	PTR_XO32_MFC0(t6, t7, C0_PAGEMASK)
	PTR_S	t0, 0(a0)
	PTR_S	t1, 4(a0)
	PTR_S	t2, 0(a1)
	PTR_S	t3, 4(a1)
	PTR_S	t4, 0(a2)
	PTR_S	t5, 4(a2)
	PTR_S	t6, 0(a3)
	PTR_S	t7, 4(a3)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MFC0 t0, C0_ENTRYHI
	PTR_MFC0 t1, C0_ENTRYLO0
	PTR_MFC0 t2, C0_ENTRYLO1
	PTR_MFC0 t3, C0_PAGEMASK
	PTR_S	t0, 0(a0)
	PTR_S	t1, 0(a1)
	PTR_S	t2, 0(a2)
	PTR_S	t3, 0(a3)
#endif /* _MIPS_SIM==_ABIO32 */
	jr	ra
END(m64_tlbri2)

/*
 * int m64_tlbprobe2(tlbhi64_t hi, tlblo64_t *lo0, tlblo64_t *lo1,
 * 	unsigned int *mask)
 *
 * Probes the TLB for an entry matching hi and returns its index, or -1 if
 * not found. If found, then the EntryLo0, EntryLo1 and PageMask parts of the
 * entry are also returned in *plo0, *plo1 and *pmsk respectively.
 *
 */
LEAF(m64_tlbprobe2)
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MFC0(t9, t8, C0_ENTRYHI)
	PTR_L	t0, 16(sp)
	PTR_XO32_MTC0(a0, a1, C0_ENTRYHI)

#else /* _MIPS_SIM==_ABIO32 */
	PTR_MFC0 t8, C0_ENTRYHI
	PTR_MTC0 a0, C0_ENTRYHI
#endif /* _MIPS_SIM==_ABIO32 */
	ehb			# mtc0, hazard on tlbp

	tlbp
	ehb			# tlpb, hazard on index.

	mfc0	v0, C0_INDEX
	bltz	v0, 1f		# Return -1 if not found.

	tlbr
	ehb			# tlbr, hazard on entry*, pagemask
# return entrylo, entrylo1, pagermask
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MFC0(t2, t3, C0_ENTRYLO0)
	PTR_XO32_MFC0(t4, t5, C0_ENTRYLO1)
	PTR_XO32_MFC0(t6, t7, C0_PAGEMASK)
	PTR_S	t2, 0(a2)
	PTR_S	t3, 4(a2)
	PTR_S	t4, 0(a3)
	PTR_S	t5, 4(a3)
	PTR_S	t6, 0(t0)
	PTR_S	t7, 4(t0)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MFC0 t1, C0_ENTRYLO0
	PTR_MFC0 t2, C0_ENTRYLO1
	PTR_MFC0 t3, C0_PAGEMASK
	PTR_S	t1, 0(a1)
	PTR_S	t2, 0(a2)
	PTR_S	t3, 0(a3)
#endif /* _MIPS_SIM==_ABIO32 */

#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(t9, t8, C0_ENTRYHI)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MTC0 t8, C0_ENTRYHI
#endif /* _MIPS_SIM==_ABIO32 */
	.set	push
	.set	noreorder
	jr.hb	ra
	nop
	.set	pop

1:	li	v0,-1
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(t9, t8, C0_ENTRYHI)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MTC0 t8, C0_ENTRYHI
#endif /* _MIPS_SIM==_ABIO32 */

	.set	push
	.set	noreorder
	jr.hb	ra
	nop
	.set	pop

END(m64_tlbprobe2)

/*
 * void m64_tlbinval(tlbhi64_t a0)
 *
 * Probes the TLB for an entry matching hi, and if present invalidates it.
 *
 */
LEAF(m64_tlbinval)

#if _MIPS_SIM==_ABIO32
	PTR_XO32_MFC0(t9, t8, C0_ENTRYHI)
	PTR_XO32_MTC0(a0, a1, C0_ENTRYHI)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MFC0 t8, C0_ENTRYHI
	PTR_MTC0 a0, C0_ENTRYHI
#endif /* _MIPS_SIM==_ABIO32 */
	ehb			# mtc0, Hazard on tlbp

	tlbp
	ehb			# tlbp, Hazard on index, entry*

	mfc0	v0, C0_INDEX
	bltz	v0, 4f

#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(zero, zero, C0_ENTRYLO0)
	PTR_XO32_MTC0(zero, zero, C0_ENTRYLO1)
#else
	PTR_MTC0 zero, C0_ENTRYLO0
	PTR_MTC0 zero, C0_ENTRYLO1
#endif /* _MIPS_SIM==_ABIO32 */

	mfc0	v1, C0_CONFIG3
	ext	v1, v1, CFG3_M_SHIFT, 1
	beqz	v1, 2f

	mfc0	v1, C0_CONFIG4
	ext	v1, v1, CFG4_IE_SHIFT, CFG4_IE_BITS
	beqz	v1, 2f

	li	v1, C0_ENTRYHI_EHINV_MASK
	b	3f

#if _MIPS_SIM==_ABIO32
2:	li	t1, -1
	li	v1, (KSEG0_BASE - 2<<13)
5:	addiu	v1, v1, (2<<13)
	PTR_XO32_MTC0(t1, v1, C0_ENTRYHI)
	ehb				# mtc0, Hazard on tlbp

	tlbp
	ehb				# tlbp, hazard on index

	mfc0	t2, C0_INDEX
	bgez	t2, 5b

	mtc0	v0, C0_INDEX
3:	PTR_XO32_MTC0(t1, v1, C0_ENTRYHI)
#else
2:	li	v1, (KSEG0_BASE-2<<13)	# replace with this
5:	addiu	v1, v1, (2<<13)
	PTR_MTC0 v1, C0_ENTRYHI
	ehb				# mtc0, Hazard on tlbp

	tlbp
	ehb				# tlbp, hazard on index

	mfc0	t2, C0_INDEX
	bgez	t2, 5b

	mtc0	v0, C0_INDEX
3:	PTR_MTC0 v1, C0_ENTRYHI
#endif
	ehb			# mtco, hazard on tlbwi

	tlbwi
	ehb			# tlbwi, hazard on C0_ENTRYHI

4:
#if _MIPS_SIM==_ABIO32
	PTR_XO32_MTC0(t9, t8, C0_ENTRYHI)
#else /* _MIPS_SIM==_ABIO32 */
	PTR_MTC0 t8, C0_ENTRYHI
#endif /* _MIPS_SIM==_ABIO32 */
	.set	push
	.set	noreorder
	jr.hb	ra
	nop
	.set	pop

END(m64_tlbinval)
