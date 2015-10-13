/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _OS_MBUF_H 
#define _OS_MBUF_H 


/**
 * A mbuf pool to allocate a mbufs out of.  This contains a pointer to the 
 * mempool to allocate mbufs out of, along with convenient housekeeping 
 * information on mbufs in the pool (e.g. length of variable packet header)
 */
struct os_mbuf_pool {
    /** 
     * Total length of the databuf in each mbuf.  This is the size of the 
     * mempool block, minus the mbuf header
     */
    uint16_t omp_databuf_len;
    /**
     * Total number of memblock's allocated in this mempool.
     */
    uint16_t omp_mbuf_count;
    /**
     * The length of the variable portion of the mbuf header
     */
    uint16_t omp_hdr_len;
    /**
     * The memory pool which to allocate mbufs out of 
     */
    struct os_mempool *omp_pool;
};


/**
 * A packet header structure that preceeds the mbuf packet headers.
 */
struct os_mbuf_pkthdr {
    /**
     * Overall length of the packet. 
     */
    uint32_t omp_len;
    /**
     * Next packet in the mbuf chain.
     */
    STAILQ_ENTRY(os_mbuf_pkthdr) omp_next;
};

/**
 * Chained memory buffer.
 */
struct os_mbuf {
    /**
     * Current pointer to data in the structure
     */
    uint8_t *om_data;
    /**
     * Flags associated with this buffer, see OS_MBUF_F_* defintions
     */
    uint16_t om_flags;
    /**
     * Length of data in this buffer 
     */
    uint16_t om_len;

    /**
     * Pointer to next entry in the chained memory buffer
     */
    SLIST_ENTRY(os_mbuf) om_next;

    /**
     * Pointer to the beginning of the data, after this buffer
     */
    uint8_t om_databuf[0];
};

/*
 * Mbuf flags: 
 *  - OS_MBUF_F_PKTHDR: Whether or not this mbuf is a packet header mbuf
 *  - OS_MBUF_F_USER: The base user defined mbuf flag, start defining your 
 *    own flags from this flag number.
 */

#define OS_MBUF_F_PKTHDR (0)
#define OS_MBUF_F_USER   (OS_MBUF_F_PKTHDR + 1) 

/*
 * Given a flag number, provide the mask for it
 *
 * @param __n The number of the flag in the mask 
 */
#define OS_MBUF_F_MASK(__n) (1 << (__n))

/* 
 * Checks whether a given mbuf is a packet header mbuf 
 *
 * @param __om The mbuf to check 
 */
#define OS_MBUF_IS_PKTHDR(__om) \
    ((__om)->om_flags & OS_MBUF_F_MASK(OS_MBUF_F_PKTHDR))


#define OS_MBUF_PKTHDR(__om) ((struct os_mbuf_pkthdr *)     \
    ((uint8_t *)&(__om)->om_data + sizeof(struct os_mbuf)))

/*
 * Access the data of a mbuf, and cast it to type
 *
 * @param __om The mbuf to access, and cast 
 * @param __type The type to cast it to 
 */
#define OS_MBUF_DATA(__om, __type) \
     (__type) ((__om)->om_data)

/**
 * Returns the end offset of a mbuf buffer 
 * 
 * @param __omp 
 */
#define OS_MBUF_END_OFF(__omp) ((__omp)->omp_databuf_len)

/**
 * Returns the start offset of a mbuf buffer
 */
#define OS_MBUF_START_OFF(__omp) (0)


/*
 * Called by OS_MBUF_LEADINGSPACE() macro
 */
static inline uint16_t 
_os_mbuf_leadingspace(struct os_mbuf_pool *omp, struct os_mbuf *om)
{
    uint16_t startoff;
    uint16_t leadingspace;

    startoff = 0;
    if (OS_MBUF_IS_PKTHDR(om)) {
        startoff = sizeof(struct os_mbuf_pkthdr) + omp->omp_hdr_len;
    }

    leadingspace = (uint16_t) (OS_MBUF_DATA(om, uint8_t *) - 
        ((uint8_t *) &om->om_databuf[0] + startoff));

    return (leadingspace);
}

/**
 * Returns the leading space (space at the beginning) of the mbuf. 
 * Works on both packet header, and regular mbufs, as it accounts 
 * for the additional space allocated to the packet header.
 * 
 * @param __omp Is the mbuf pool (which contains packet header length.)
 * @param __om  Is the mbuf in that pool to get the leadingspace for 
 *
 * @return Amount of leading space available in the mbuf 
 */
#define OS_MBUF_LEADINGSPACE(__omp, __om) _os_mbuf_leadingspace(__omp, __om)

/* Called by OS_MBUF_TRAILINGSPACE() macro. */
static inline uint16_t 
_os_mbuf_trailingspace(struct os_mbuf_pool *omp, struct os_mbuf *om)
{
    return (&om->om_databuf[0] + omp->omp_databuf_len) - om->om_data;
}

/**
 * Returns the trailing space (space at the end) of the mbuf.
 * Works on both packet header and regular mbufs.
 *
 * @param __omp The mbuf pool for this mbuf 
 * @param __om  Is the mbuf in that pool to get trailing space for 
 *
 * @return The amount of trailing space available in the mbuf 
 */
#define OS_MBUF_TRAILINGSPACE(__omp, __om) _os_mbuf_trailingspace(__omp, __om)


/* Initialize a mbuf pool */
int os_mbuf_pool_init(struct os_mbuf_pool *, struct os_mempool *mp, uint16_t, 
        uint16_t, uint16_t);

/* Allocate a new mbuf out of the os_mbuf_pool */ 
struct os_mbuf *os_mbuf_get(struct os_mbuf_pool *omp, uint16_t);

/* Allocate a new packet header mbuf out of the os_mbuf_pool */ 
struct os_mbuf *os_mbuf_get_pkthdr(struct os_mbuf_pool *omp);

/* Duplicate a mbuf from the pool */
struct os_mbuf *os_mbuf_dup(struct os_mbuf_pool *omp, struct os_mbuf *m);

/* Append data onto a mbuf */
int os_mbuf_append(struct os_mbuf_pool *omp, struct os_mbuf *m, void *, 
        uint16_t);

/* Free a mbuf */
int os_mbuf_free(struct os_mbuf_pool *omp, struct os_mbuf *mb);

/* Free a mbuf chain */
int os_mbuf_free_chain(struct os_mbuf_pool *omp, struct os_mbuf *om);

#endif /* _OS_MBUF_H */ 
