os\_mbuf\_prepend
-----------------

.. code:: c

    struct os_mbuf *os_mbuf_prepend(struct os_mbuf *om, int len)

Increases the length of an mbuf chain by adding data to the front. If
there is insufficient room in the leading mbuf, additional mbufs are
allocated and prepended as necessary. If this function fails to allocate
an mbuf, the entire chain is freed.

Arguments
^^^^^^^^^

+-------------+--------------------------------+
| Arguments   | Description                    |
+=============+================================+
| om          | Pointer to mbuf                |
+-------------+--------------------------------+
| len         | Length, in bytes, to prepend   |
+-------------+--------------------------------+

Returned values
^^^^^^^^^^^^^^^

Pointer to mbuf at head of chain; NULL if not enough mbufs were
available to accommodate *len*.

Notes
^^^^^

If *om* is a packet header mbuf, the total length of the packet is
adjusted by *len*. Note that the returned mbuf may not point to *om* if
insufficient leading space was available in *om*.

Example
^^^^^^^

.. code:: c

        uint16_t pktlen;
        struct os_mbuf *om;
        struct os_mbuf *tmp;

        /* Get initial packet length before prepend */
        pktlen = OS_MBUF_PKTLEN(om);

        tmp = os_mbuf_prepend(om, 32);
        if (!tmp) {
            /* Not able to prepend. The chain pointed to by *om has been freed */
            return -1;
        }

        /* The packet length should equal the original length plus what we prepended */
        assert((pktlen + 32) == OS_MBUF_PKTLEN(tmp));
