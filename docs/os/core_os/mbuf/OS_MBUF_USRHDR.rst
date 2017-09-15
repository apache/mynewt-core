OS\_MBUF\_USRHDR
----------------

.. code:: c

    OS_MBUF_USRHDR(__om)

Macro used to get a pointer to the user packet header of an mbuf.

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

        struct os_mbuf *om
        struct user_header *hdr;

        hdr = OS_MBUF_USRHDR(om);
