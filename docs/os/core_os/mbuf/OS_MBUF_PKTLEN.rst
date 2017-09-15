OS\_MBUF\_PKTLEN
----------------

.. code:: c

    OS_MBUF_PKTLEN(__om)

Macro used to get the length of an entire mbuf chain.

Arguments
^^^^^^^^^

+-------------+----------------------------------------+
| Arguments   | Description                            |
+=============+========================================+
| \_\_om      | Pointer to mbuf (struct os\_mbuf \*)   |
+-------------+----------------------------------------+

Example
^^^^^^^

.. code:: c

        uint16_t pktlen;
        struct os_mbuf *om;

        /* Check if there is any data in the mbuf chain */
        pktlen = OS_MBUF_PKTLEN(om);
        if (pktlen != 0) {
            /* mbuf chain has data */
        }
