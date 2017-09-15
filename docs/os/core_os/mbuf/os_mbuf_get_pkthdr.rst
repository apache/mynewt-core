os\_mbuf\_get\_pkthdr
---------------------

.. code:: c

    struct os_mbuf *os_mbuf_get_pkthdr(struct os_mbuf_pool *omp, uint8_t pkthdr_len);

Allocates a packet header mbuf from the mbuf pool pointed to by *omp*.
Adds a user header of length *pkthdr\_len* to packet header mbuf.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| om           | Pointer to     |
|              | mbuf pool from |
|              | which to       |
|              | allocate mbuf  |
+--------------+----------------+
| pkthdr\_len  | The user       |
|              | header packet  |
|              | length to      |
|              | allocate for   |
|              | the packet     |
|              | header mbuf    |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

Returns a pointer to the allocated mbuf or NULL if there are no mbufs
available or the user packet header was too large.

Notes
^^^^^

The packet header mbuf returned will have its data pointer incremented
by the sizeof(struct os\_mbuf\_pkthdr) as well as the amount of user
header data (i.e. *pkthdr\_len*). In other words, the data pointer is
offset from the start of the mbuf by: sizeof(struct os\_mbuf) +
sizeof(struct os\_mbuf\_pkthdr) + pkthdr\_len. The ``om_pkthdr_len``
element in the allocated mbuf is set to: sizeof(struct os\_mbuf\_pkthdr)
+ pkthdr\_len.

Example
^^^^^^^

.. code:: c

        struct os_mbuf *om;
        struct my_user_header my_hdr;

        /* Get a packet header mbuf with a user header in it */
        om = os_mbuf_get_pkthdr(&g_mbuf_pool, sizeof(struct my_user_header));
        if (om) {
            /* Packet header mbuf was allocated */
        }
