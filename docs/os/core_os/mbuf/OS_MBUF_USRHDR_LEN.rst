OS\_MBUF\_USRHDR\_LEN
---------------------

.. code:: c

    OS_MBUF_USRHDR_LEN(__om)

Macro used to retrieve the length of the user packet header in an mbuf.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| \_\_om       | Pointer to     |
|              | mbuf (struct   |
|              | os\_mbuf \*).  |
|              | Must be head   |
|              | of chain (i.e. |
|              | a packet       |
|              | header mbuf)   |
+--------------+----------------+

Example
^^^^^^^

.. code:: c

        uint16_t user_length;
        struct os_mbuf *om
        struct user_header *hdr;

        user_length  = OS_MBUF_USRHDR_LEN(om);
