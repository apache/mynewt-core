os\_mbuf\_memcmp
----------------

.. code:: c

    int os_mbuf_memcmp(const struct os_mbuf *om, int off, const void *data, int len)

Performs a memory compare of the specified region of an mbuf chain
against a flat buffer.

Arguments
^^^^^^^^^

+-------------+---------------------------------------------------------------+
| Arguments   | Description                                                   |
+=============+===============================================================+
| om          | Pointer to mbuf                                               |
+-------------+---------------------------------------------------------------+
| off         | Offset, in bytes, from start of mbuf to start of comparison   |
+-------------+---------------------------------------------------------------+
| data        | Pointer to flat data buffer to compare                        |
+-------------+---------------------------------------------------------------+
| len         | Number of bytes to compare                                    |
+-------------+---------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

A value of zero means the memory regions are identical; all other values
represent either an error or a value returned from memcmp.

Notes
^^^^^

This function will compare bytes starting from *off* bytes from the
start of the mbuf chain with a data buffer.

Example
^^^^^^^

.. code:: c

        int rc;
        struct os_mbuf *om;
        uint8_t my_data_buffer[32];

        /* Get a packet header mbuf with a user header in it */
        rc = os_mbuf_memcmp(om, 0, my_data_buffer, 32);
        if (!rc) {
            /* "my_data_buffer" and the data from offset 0 in the mbuf chain are identical! */
        }    
