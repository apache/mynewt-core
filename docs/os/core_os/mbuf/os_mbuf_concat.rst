 os\_mbuf\_concat
-----------------

.. code:: c

    void os_mbuf_concat(struct os_mbuf *first, struct os_mbuf *second)

Attaches a second mbuf chain onto the end of the first. If the first
chain contains a packet header, the header's length is updated. If the
second chain has a packet header, its header is cleared.

Arguments
^^^^^^^^^

+-------------+--------------------------------+
| Arguments   | Description                    |
+=============+================================+
| first       | Pointer to first mbuf chain    |
+-------------+--------------------------------+
| second      | Pointer to second mbuf chain   |
+-------------+--------------------------------+

Returned values
^^^^^^^^^^^^^^^

None

Notes
^^^^^

No data is copied or moved nor are any mbufs freed.

Example
^^^^^^^

.. code:: c

        uint16_t pktlen1;
        uint16_t pktlen2;
        struct os_mbuf *pkt1;
        struct os_mbuf *pkt2;
        
        /* Get initial packet lengths */
        pktlen1 = OS_MBUF_PKTLEN(pkt1);
        pktlen2 = OS_MBUF_PKTLEN(pkt2);

        /*  Add pkt2 to end of pkt1 */
        os_mbuf_concat(pkt1, pkt2);

        /* New packet length should be sum of pkt1 and pkt2 */
        assert((pktlen1 + pktlen2) == OS_MBUF_PKTLEN(pkt1));
