os\_msys\_get\_pkthdr
---------------------

.. code:: c

    struct os_mbuf *os_msys_get_pkthdr(uint16_t dsize, uint16_t user_hdr_len)

Retrieve a packet header mbuf from the system mbuf pools with
*user\_hdr\_len* bytes available for the user header in the mbuf.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| dsize        | Minimum        |
|              | requested size |
|              | of mbuf.       |
|              | Actual mbuf    |
|              | allocated may  |
|              | not            |
|              | accommodate    |
|              | *dsize*        |
+--------------+----------------+
| user\_hdr\_l | Size, in of    |
| en           | bytes, of user |
|              | header in the  |
|              | mbuf           |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

Pointer to mbuf or NULL if no mbufs were available.

Notes
^^^^^

The same notes apply to this API as to ``os_msys_get()``.

Example
^^^^^^^

.. code:: c

        struct os_mbuf *om;
        struct my_user_hdr_struct my_usr_hdr;

        /*
         * Allocate an mbuf with hopefully at least 100 bytes in its user data buffer
         * and that has a user header of size sizeof(struct my_user_hdr_struct)
         */
        om = os_msys_get_pkthdr(100, sizeof(struct my_user_hdr_struct));
        if (!om) {
            /* No mbufs available. */
            return -1;
        }
    }
