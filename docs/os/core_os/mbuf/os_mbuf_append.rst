 os\_mbuf\_append
-----------------

.. code:: c

    int os_mbuf_append(struct os_mbuf *om, const void *data,  uint16_t len)

Appends a data buffer of length *len* to the end of an mbuf chain,
adjusting packet length if *om* is a packet header mbuf. If not enough
trailing space exists at the end of the mbuf chain, mbufs are allocated
to hold the data.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| om           | Pointer to     |
|              | mbuf. Can be   |
|              | head of a      |
|              | chain of       |
|              | mbufs, a       |
|              | single mbuf or |
|              | a packet       |
|              | header mbuf    |
+--------------+----------------+
| data         | Pointer to     |
|              | data buffer to |
|              | copy from      |
+--------------+----------------+
| len          | Number of      |
|              | bytes to copy  |
|              | from data      |
|              | buffer to the  |
|              | end of the     |
|              | mbuf           |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

| 0: success
| OS\_ENOMEM: Could not allocate enough mbufs to hold data.
| OS\_EINVAL: *om* was NULL on entry.

Notes
^^^^^

If not enough mbufs were available the packet header length of the mbuf
may get adjusted even though the entire data buffer was not appended to
the end of the mbuf.

If any mbufs are allocated, they are allocated from the same pool as
*om*

Example
^^^^^^^

.. code:: c

        int rc;
        uint16_t pktlen;
        struct os_mbuf *om;
        struct my_data_struct my_data;
        
        /* Get initial packet length */
        pktlen = OS_MBUF_PKTLEN(om);

        /* Append "my_data" to end of mbuf, freeing mbuf if unable to append all the data */
        rc = os_mbuf_append(om, &my_data, sizeof(struct my_pkt_header));
        if (rc) {
            os_mbuf_free_chain(om);
        }
        pktlen += sizeof(struct my_pkt_header);

        /* New packet length should be initial packet length plus length of "my_data" */
        assert(pktlen == OS_MBUF_PKTLEN(om));
