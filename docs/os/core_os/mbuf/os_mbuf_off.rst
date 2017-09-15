os\_mbuf\_off
-------------

.. code:: c

    struct os_mbuf *os_mbuf_off(struct os_mbuf *om, int off, int *out_off)

Given an offset in the packet (i.e. user data byte offset in the mbuf
chain), return the mbuf and the offset in that mbuf where byte 'off' is
located. Note that the offset is 'returned' in *out\_off*.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| om           | Pointer to     |
|              | mbuf           |
+--------------+----------------+
| off          | Location in    |
|              | mbuf chain of  |
|              | desired byte   |
|              | offset         |
+--------------+----------------+
| out\_off     | Pointer to     |
|              | storage for    |
|              | the relative   |
|              | offset of the  |
|              | absolute       |
|              | location in    |
|              | the returned   |
|              | mbuf           |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

NULL if the offset is not within the mbuf chain or *om* points to NULL.

Notes
^^^^^

The user is allowed to call this function with the length of the mbuf
chain but no greater. This allows the user to get the mbuf and offset
(in that mbuf) where the next user data byte should be written.

While this api is provided to the user, other API are expected to be
used by the applciation developer (i.e. ``os_mbuf_append()`` or
``os_mbuf_copyinto()``).

Example
^^^^^^^

.. code:: c

        int relative_offset;
        uint16_t pktlen;
        struct os_mbuf *om;
        struct os_mbuf *tmp;

        /* Append a new line character to end of mbuf data */
        pktlen = OS_MBUF_PKTLEN(om);

        relative_offset = 0;
        tmp = os_mbuf_off(om, pktlen, &relative_offset);
        if (tmp) {
            /* Offset found. */
            tmp->om_data[relative_offset] = '\n';
        } else {
            /*
             * This mbuf does not contain enough bytes so this is an invalid offset.
             * In other words, the mbuf is less than 62 bytes in length.
             */
        }
