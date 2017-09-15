 os\_mbuf\_copyinto
-------------------

.. code:: c

    int os_mbuf_copyinto(struct os_mbuf *om, int off, const void *src, int len);

Copies the contents of a flat buffer into an mbuf chain, starting at the
specified destination offset. If the mbuf is too small for the source
data, it is extended as necessary. If the destination mbuf contains a
packet header, the header length is updated.

Arguments
^^^^^^^^^

+-------------+-------------------------------------------------------------+
| Arguments   | Description                                                 |
+=============+=============================================================+
| om          | Pointer to mbuf chain                                       |
+-------------+-------------------------------------------------------------+
| off         | Start copy offset, in bytes, from beginning of mbuf chain   |
+-------------+-------------------------------------------------------------+
| src         | Address from which bytes are copied                         |
+-------------+-------------------------------------------------------------+
| len         | Number of bytes to copy from src                            |
+-------------+-------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

| 0: success.
| All other values indicate an error.

Example
^^^^^^^

.. code:: c

        int rc;
        uint16_t pktlen;
        struct os_mbuf *om;
        struct my_data_struct my_data;  
        
        /* Get initial packet length */
        pktlen = OS_MBUF_PKTLEN(om);

        /* Copy "my_data" into mbuf */
        rc = os_mbuf_copyinto(om, 0, &my_data, sizeof(struct my_data_struct));
        if (rc) {
            os_mbuf_free_chain(om);
            return;
        }

        /* Packet length should have increased by size of "my_data" */
        pktlen += sizeof(struct my_data_struct);
        assert(pktlen == OS_MBUF_PKTLEN(om));
