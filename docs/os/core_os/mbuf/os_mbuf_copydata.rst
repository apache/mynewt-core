 os\_mbuf\_copydata
-------------------

.. code:: c

    int os_mbuf_copydata(const struct os_mbuf *m, int off, int len, void *dst)

Copy data from an mbuf chain starting *off* bytes from the beginning,
continuing for *len* bytes, into the indicated buffer.

Arguments
^^^^^^^^^

+-------------+-------------------------------------------------------------+
| Arguments   | Description                                                 |
+=============+=============================================================+
| m           | Pointer to mbuf chain                                       |
+-------------+-------------------------------------------------------------+
| off         | Start copy offset, in bytes, from beginning of mbuf chain   |
+-------------+-------------------------------------------------------------+
| len         | Number of bytes to copy                                     |
+-------------+-------------------------------------------------------------+
| dst         | Data buffer to copy into                                    |
+-------------+-------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

| 0: success.
| -1: The mbuf does not contain enough data

Example
^^^^^^^

.. code:: c

        int rc;
        struct os_mbuf *om;
        struct my_hdr_1 my_hdr1;    
        struct my_hdr_2 my_hdr2;    
        
        /* Header 1 and Header 2 are contiguous in packet at start. Retrieve them from the mbuf chain */    
        rc = os_mbuf_copydata(om, 0, sizeof(struct my_hdr_1), &my_hdr1);
        if (rc) {
            /* error! */
            return -1;
        }

        rc = os_mbuf_copydata(om, sizeof(struct my_hdr_1), sizeof(struct my_hdr_2), &my_hdr2);
        if (rc) {
            /* error! */
            return -1;
        }

