OS\_MBUF\_LEADINGSPACE
----------------------

.. code:: c

    OS_MBUF_LEADINGSPACE(__om)

Macro used to get the amount of leading space in an mbuf (in bytes).

Arguments
^^^^^^^^^

+-------------+----------------------------------------+
| Arguments   | Description                            |
+=============+========================================+
| \_\_om      | Pointer to mbuf (struct os\_mbuf \*)   |
+-------------+----------------------------------------+

Notes
^^^^^

This macro works on both normal mbufs and packet header mbufs. The
amount of leading space is the number of bytes between the current
om\_data pointer of the mbuf and the start of the mbuf user data buffer.

Example
^^^^^^^

.. code:: c

        uint8_t *dptr;
        uint16_t space;
        struct os_mbuf *om;
        struct my_data_struct my_data;

        /* Copy data from "my_data" into the start of an mbuf but only if there is enough room */
        space = OS_MBUF_LEADINGSPACE(om);
        if (space >= sizeof(struct my_data_struct)) {
            dptr = om->om_data - sizeof(struct my_data_struct);
            memcpy(dptr, &my_data, sizeof(struct my_data_struct));
        }
