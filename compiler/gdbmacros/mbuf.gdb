# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#  http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

define mn_mqueue_print
    set $cur = ($arg0)->mq_head.stqh_first

    while $cur != 0
        printf "[entry %p]\n", $cur
        p/x *$cur

        set $cur = $cur->omp_next.stqe_next
    end
end

document mn_mqueue_print
usage: mn_mqueue_print <struct os_mqueue *>

Prints the contents of the specified mqueue.
end

define mn_mbuf_pkthdr_print
    # Calculate address of packet header.
    set $data_addr = (uint8_t *)&($arg0)->om_data
    set $hdr_addr = $data_addr + sizeof(struct os_mbuf)

    # Convert the address to a pointer of the correct type.
    set $pkthdr = (struct os_mbuf_pkthdr *)$hdr_addr

    # Print header.
    p *$pkthdr
end

document mn_mbuf_pkthdr_print
usage: mn_mbuf_pkthdr_print <struct os_mbuf *>

Prints the packet header associated with the specified mbuf.  If the specified
mbuf does not contain a packet header, the output is indeterminate.
end

define mn_mbuf_usrhdr_print
    # Calculate address of user header.
    set $data_addr = (uint8_t *)&($arg0)->om_data
    set $phdr_addr = $data_addr + sizeof(struct os_mbuf)
    set $uhdr_addr = $phdr_addr + sizeof(struct os_mbuf_pkthdr)

    # Determine length of user header.
    set $uhdr_len = $uhdr_addr - $phdr_addr

    # Print header.
    p/x *$uhdr_addr@$uhdr_len
end

document mn_mbuf_usrhdr_print
usage: mn_mbuf_usrhdr_print <struct os_mbuf *>

Prints the user header associated with the specified mbuf.  If the specified
mbuf does not contain a user header, the output is indeterminate.
end

define mn_mbuf_print
    set $om = (struct os_mbuf *)($arg0)

    printf "Mbuf header: "
    p *$om

    if ($om)->om_pkthdr_len > 0
        printf "Packet header: "
        mn_mbuf_pkthdr_print $om
    end

    if ($om)->om_pkthdr_len > sizeof (struct os_mbuf_pkthdr)
        printf "User header: "
        mn_mbuf_usrhdr_print $om
    end
end

document mn_mbuf_print
usage mn_mbuf_print <struct os_mbuf *>

Prints the following information about the specified mbuf:
    * mbuf header
    * mbuf packet header (if present).
end

define mn_mbuf_dump
    set $om = (struct os_mbuf *)($arg0)
    mn_mbuf_print $om

    printf "Mbuf data: "
    set $len = ($om)->om_len
    p/x *$om->om_data@$len
end

document mn_mbuf_dump
usage mn_mbuf_dump <struct os_mbuf *>

Prints the following information about the specified mbuf:
    * mbuf header
    * mbuf packet header (if present).
    * mbuf data contents
end

define mn_mbuf_chain_print
    set $totlen = 0
    set $om = ($arg0)
    while $om != 0
        printf "Mbuf addr: %p\n", $om
        mn_mbuf_print $om
        set $totlen += $om->om_len
        set $om = $om->om_next.sle_next
    end

    printf "total length: %d\n", $totlen
end

document mn_mbuf_chain_print
usage mn_mbuf_chain_print <struct os_mbuf *>

Applies the mn_mbuf_print function to each buffer in the specified mbuf chain.
That is, this function prints the following information about each mbuf:
    * mbuf header
    * mbuf packet header (if present).
end

define mn_mbuf_chain_dump
    set $totlen = 0
    set $om = ($arg0)
    while $om != 0
        printf "Mbuf addr: %p\n", $om
        mn_mbuf_dump $om
        set $totlen += $om->om_len
        set $om = $om->om_next.sle_next
    end

    printf "total length: %d\n", $totlen
end

document mn_mbuf_chain_dump
usage mn_mbuf_chain_dump <struct os_mbuf *>

Applies the mn_mbuf_dump function to each buffer in the specified mbuf chain.
That is, this function prints the following information about each mbuf:
    * mbuf header
    * mbuf packet header (if present).
    * mbuf data contents
end

define mn_mbuf_pool_print
    set $pool = ($arg0)
    set $om = (struct os_mbuf *)$pool->omp_pool.mp_membuf_addr
    set $elem_size = $pool->omp_databuf_len + sizeof (struct os_mbuf)
    set $end = (uint8_t *)$om + $pool->omp_pool.mp_num_blocks * $elem_size

    while $om < $end
        printf "Mbuf addr: %p\n", $om
        mn_mbuf_print $om
        set $om = (struct os_mbuf *)((uint8_t *)$om + $elem_size)
    end
end

document mn_mbuf_pool_print
usage: mn_mbuf_pool_print <struct os_mbuf_pool *>

Applies the mn_mbuf_print function to each element in the specified pool.  Both
allocated and unallocated mbufs are printed.
end

define mn_msys1_print
    mn_mbuf_pool_print &os_msys_init_1_mbuf_pool 
end

document mn_msys1_print
usage: mn_msys1_print

Prints all mbufs in the first msys pool.  Both allocated and unallocated mbufs
are printed.
end

define mn_msys1_free_print
    set $om = os_msys_init_1_mempool.slh_first

    while $om != 0
        printf "Mbuf addr: %p\n", $om
        set $om = $om->mb_next.sle_next
    end
end
