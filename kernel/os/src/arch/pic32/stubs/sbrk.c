#ident "c/stubs/sbrk.c: Copyright (c) MIPS Technologies, Inc. All rights reserved."

/*
 * Unpublished work (c) MIPS Technologies, Inc.  All rights reserved.
 * Unpublished rights reserved under the copyright laws of the United
 * States of America and other countries.
 * 
 * This code is confidential and proprietary to MIPS Technologies,
 * Inc. ("MIPS Technologies") and may be disclosed only as permitted in
 * writing by MIPS Technologies. Any copying, reproducing, modifying,
 * use or disclosure of this code (in whole or in part) that is not
 * expressly permitted in writing by MIPS Technologies is strictly
 * prohibited. At a minimum, this code is protected under trade secret,
 * unfair competition, and copyright laws. Violations thereof may result
 * in criminal penalties and fines.
 * 
 * MIPS Technologies reserves the right to change this code to improve
 * function, design or otherwise. MIPS Technologies does not assume any
 * liability arising out of the application or use of this code, or of
 * any error or omission in such code.  Any warranties, whether express,
 * statutory, implied or otherwise, including but not limited to the
 * implied warranties of merchantability or fitness for a particular
 * purpose, are excluded.  Except as expressly provided in any written
 * license agreement from MIPS Technologies, the furnishing of this
 * code does not give recipient any license to any intellectual property
 * rights, including any patent rights, that cover this code.
 * 
 * This code shall not be exported, reexported, transferred, or released,
 * directly or indirectly, in violation of the law of any country or
 * international law, regulation, treaty, Executive Order, statute,
 * amendments or supplements thereto.  Should a conflict arise regarding
 * the export, reexport, transfer, or release of this code, the laws of
 * the United States of America shall be the governing law.
 * 
 * This code may only be disclosed to the United States government
 * ("Government"), or to Government users, with prior written consent
 * from MIPS Technologies.  This code constitutes one or more of the
 * following: commercial computer software, commercial computer software
 * documentation or other commercial items.  If the user of this code,
 * or any related documentation of any kind, including related technical
 * data or manuals, is an agency, department, or other entity of the
 * Government, the use, duplication, reproduction, release, modification,
 * disclosure, or transfer of this code, or any related documentation
 * of any kind, is restricted in accordance with Federal Acquisition
 * Regulation 12.212 for civilian agencies and Defense Federal Acquisition
 * Regulation Supplement 227.7202 for military agencies.  The use of this
 * code by the Government is further restricted in accordance with the
 * terms of the license agreement(s) and/or applicable contract terms
 * and conditions covering this code from MIPS Technologies.
 * 
 * 
 */

/* 
 * sbrk.c: a generic sbrk() emulation.
 */

#include <string.h>
#include <errno.h>

#include <sys/kmem.h>

/* memory layout */
struct sbd_region {
    _paddr_t    base;
    size_t      size;
    int         type;
};

/* _minbrk and _maxbrk can be set by startup code, or by a linker
   script, so we don't want them in bss where they'll get cleared, so
   they can't be common, but they must be capable of being
   overridden. */
void *		_minbrk __attribute__((weak)) = 0;
void *		_maxbrk __attribute__((weak)) = 0;

extern int	errno;
extern char     _end[];

#if 0
static pthread_mutex_t sbmx = PTHREAD_MUTEX_INITIALIZER;
#endif

static void *	curbrk = 0;

#ifndef MINHEAP
#define MINHEAP		(1 * 1024)
#endif

#ifndef MAXSTACK
#define MAXSTACK	(32 * 1024)
#endif

#ifndef PAGESIZE
#define PAGESIZE 128
#endif

#define SBD_MEM_END     0
#define SBD_MEM_RAM     1

int
getpagesize ()
{
    return PAGESIZE;
}


/*
 * The _sbd_memlayout() function returns a pointer to a phys memory
 * region table, but note that at present sbrk() only uses the first
 * entry.
 *
 * This function can be overridden by the board-specific code
 * if it has some other way to determine the real size of 
 * physical memory (e.g. reading the memory controller).
 */

const struct sbd_region * _sbd_memlayout (void);
#pragma weak _sbd_memlayout=_stub_sbd_memlayout
const struct sbd_region *_stub_sbd_memlayout (void);

const struct sbd_region *
_stub_sbd_memlayout (void)
{
    static struct sbd_region mem[2];
    extern char _heap[];
    extern char _min_heap_size[];

    mem[0].type = SBD_MEM_RAM;
    mem[0].base = (_paddr_t)(&_heap);
    mem[0].size = (size_t)(&_min_heap_size);

    return mem;
}


/* 
 * Initialise the sbrk heap. 
 *
 * This function is hard-wired to the idea that the code is linked to
 * KSEG0 or KSEG1 addresses. It could just about cope with being
 * linked to run in KUSEG, as long as there's a one-to-one mapping
 * from virtual to physical address. If you are playing real virtual
 * memory games then the functions in the module will have to be
 * replaced.
 */

void
_sbrk_init (void)
{
    const struct sbd_region * layout;
    void * minva,  * maxva;
    _paddr_t rbase, rtop, min, max;
    extern char _heap[];
    extern char _min_heap_size[];

    if (curbrk)
	return;

    if (_minbrk)
	/* user specified heap start */
	minva = _minbrk;
    else
	/* usually heap starts after data & bss segment */
#if (__C32_VERSION__ > 200)
	minva = &_heap;
#else
	minva = _end;
#endif

    if (_maxbrk)
	/* user specified heap top */
	maxva = _maxbrk;
    else {
	/* usually stack is at top of memory, and
	   heap grows up towards base of stack */
#if (__C32_VERSION__ > 200)
	  maxva = (void*)(&_heap) + (size_t)(&_min_heap_size);
#else
	  char * sp;
	  __asm__ ("move %0,$sp" : "=d" (sp));
	  maxva = sp - MAXSTACK;
#endif
    }

    /* convert min/max to physical addresses */
    if (IS_KVA01 (minva))
	min = KVA_TO_PA (minva);
    else
	/* use virtual address */
	min = (_paddr_t) minva;

    if (IS_KVA01 (maxva))
	max = KVA_TO_PA (maxva);
    else
	max = (_paddr_t) maxva;

    /* determine physical memory layout */
    layout = _sbd_memlayout ();

    /* base of heap must be inside memory region #0 */
    rbase = layout[0].base;
    rtop = rbase + layout[0].size;
    if (min < rbase || min >= rtop) {
	if (rbase >= KVA_TO_PA (_end))
	    /* no overlap of region with data - use region base */
	    min = rbase;
	else
	    /* can't determine a good heap base */
	    /* XXX could try _end in case of bad _minbrk setting */
	    return;
    }

    /* end of heap must be inside memory region #0 (and above base) */
    if (max < min || max >= rtop) {
	if (rtop > min)
	    /* use top of region as top of heap */
	    /* XXX what about poss overlap with stack? */
	    max = rtop;
	else
	    /* can't determine a good heap top */
	    return;
    }

    /* put minbrk/maxbrk in same kernel virtual segment as data */
    if (IS_KVA1 (_end)) {
	/* kseg1: uncached data segment */
	_minbrk = PA_TO_KVA1 (min);
	_maxbrk = PA_TO_KVA1 (max);
    }
    else if (IS_KVA0 (_end)) {
	/* kseg0: cached data segmnt */
	_minbrk = PA_TO_KVA0 (min);
	_maxbrk = PA_TO_KVA0 (max);
    }
    else {
	/* kuseg: use virtual addresses */
	_minbrk = (void *) min;
	_minbrk = (void *) max;
    }
    
    curbrk = _minbrk;
}


void *
_sbrk (int n)
{
    void *newbrk, *p;
    
#if 0
    pthread_mutex_lock (&sbmx);
#endif
    if (!curbrk) {
	_sbrk_init ();
	if (!curbrk) {
	    errno = ENOMEM;
#if 0
	    pthread_mutex_unlock (&sbmx);
#endif
	    return (void *)-1;
	}
    }

    p = curbrk;
    newbrk = curbrk + n;
    if (n > 0) {
	if (newbrk < curbrk || newbrk > _maxbrk) {
	    errno = ENOMEM;
#if 0
	    pthread_mutex_unlock (&sbmx);
#endif
	    return (void *)-1;
	}
    } else {
	if (newbrk > curbrk || newbrk < _minbrk) {
	    errno = EINVAL;
#if 0
	    pthread_mutex_unlock (&sbmx);
#endif
	    return (void *)-1;
	}
    }
    curbrk = newbrk;

#if 0
    pthread_mutex_unlock (&sbmx);
#endif

    return p;
}

void *
sbrk (int n)
{
    void *p;

    p = _sbrk(n);

    /* sbrk defined to return zeroed pages */
    if ((n > 0) && (p != (void *)-1))
	memset (p, 0, n);

    return p;
}
