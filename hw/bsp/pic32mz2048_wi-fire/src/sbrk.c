/*
 * Copyright Imagination Technologies Limited and/or its
 *                      affiliated group companies.
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
  * sbrk.c: a generic sbrk() emulation.
  */

 #include <string.h>

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
_sbrkInit (void)
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

     if (!curbrk) {
 	_sbrkInit ();
 	if (!curbrk) {
 	    return (void *)-1;
 	}
     }

     p = curbrk;
     newbrk = curbrk + n;
     if (n > 0) {
 	if (newbrk < curbrk || newbrk > _maxbrk) {
 	    return (void *)-1;
 	}
     } else {
 	if (newbrk > curbrk || newbrk < _minbrk) {
 	    return (void *)-1;
 	}
     }
     curbrk = newbrk;

     return p;
 }
