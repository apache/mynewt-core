Msys
====

Msys stands for "system mbufs" and is a set of API built on top of the
mbuf code. The basic idea behind msys is the following. The developer
can create different size mbuf pools and register them with msys. The
application then allocates mbufs using the msys API (as opposed to the
mbuf API). The msys code will choose the mbuf pool with the smallest
mbufs that can accommodate the requested size.

Let us walk through an example where the user registers three mbuf pools
with msys: one with 32 byte mbufs, one with 256 and one with 2048. If
the user requests an mbuf with 10 bytes, the 32-byte mbuf pool is used.
If the request is for 33 bytes the 256 byte mbuf pool is used. If an
mbuf data size is requested that is larger than any of the pools (say,
4000 bytes) the largest pool is used. While this behaviour may not be
optimal in all cases that is the currently implemented behaviour. All
this means is that the user is not guaranteed that a single mbuf can
hold the requested data.

The msys code will not allocate an mbuf from a larger pool if the chosen
mbuf pool is empty. Similarly, the msys code will not chain together a
number of smaller mbufs to accommodate the requested size. While this
behaviour may change in future implementations the current code will
simply return NULL. Using the above example, say the user requests 250
bytes. The msys code chooses the appropriate pool (i.e. the 256 byte
mbuf pool) and attempts to allocate an mbuf from that pool. If that pool
is empty, NULL is returned even though the 32 and 2048 byte pools are
not empty.

Note that no added descriptions on how to use the msys API are presented
here (other than in the API descriptions themselves) as the msys API is
used in exactly the same manner as the mbuf API. The only difference is
that mbuf pools are added to msys by calling ``os_msys_register().``

API
-----------------

.. doxygengroup:: OSMsys
    :content-only:
