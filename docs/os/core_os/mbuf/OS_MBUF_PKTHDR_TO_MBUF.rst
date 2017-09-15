OS\_MBUF\_PKTHDR\_TO\_MBUF
--------------------------

.. code:: c

    OS_MBUF_PKTHDR_TO_MBUF(__hdr)

Macro used to get a pointer to the mbuf given a pointer to the os mbuf
packet header

Arguments
^^^^^^^^^

+-------------+-----------------------------------------------------------------+
| Arguments   | Description                                                     |
+=============+=================================================================+
| \_\_hdr     | Pointer to os mbuf packet header (struct os\_mbuf\_pkthdr \*)   |
+-------------+-----------------------------------------------------------------+

Example
^^^^^^^

.. code:: c

        struct os_mbuf *om;
        struct os_mbuf_pkthdr *hdr;

        om = OS_MBUF_PKTHDR_TO_MBUF(hdr);
        os_mbuf_free_chain(om);
