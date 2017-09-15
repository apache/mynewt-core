os\_mbuf\_pullup
----------------

.. code:: c

    struct os_mbuf *os_mbuf_pullup(struct os_mbuf *om, uint16_t len)

Rearrange an mbuf chain so that len bytes are contiguous, and in the
data area of an mbuf (so that OS\_MBUF\_DATA() will work on a structure
of size len.) Returns the resulting mbuf chain on success, free's it and
returns NULL on failure.

Arguments
^^^^^^^^^

+-------------+---------------------------------------------------------+
| Arguments   | Description                                             |
+=============+=========================================================+
| om          | Pointer to mbuf                                         |
+-------------+---------------------------------------------------------+
| len         | Length, in bytes, to pullup (make contiguous in mbuf)   |
+-------------+---------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Pointer to mbuf at head of chain; NULL if not enough mbufs were
available to accommodate *len* or if the requested pullup size was too
large.

Notes
^^^^^

Hopefully it is apparent to the user that you cannot pullup more bytes
than the mbuf can accommodate. Pullup does not allocate more than one
mbuf; the entire pullup length must be contained within a single mbuf.

The mbuf that is being pulled up into does not need to be a packet
header mbuf; it can be a normal mbuf. The user should note that the
maximum pullup length does depend on the type of mbuf being pulled up
into (a packet header or normal mbuf).

Example
^^^^^^^

.. code:: c

        struct os_mbuf *om;
        struct os_mbuf *tmp;
        struct my_header_struct my_header;

        /* Make sure "my_header" is contiguous in the mbuf */
        tmp = os_mbuf_pullup(om, sizeof(my_header_struct));
        if (!tmp) {
            /* Pullup failed. The chain pointed to by *om has been freed */
            return -1;
        }

        /* copy data from mbuf into header structure */
        memcpy(&my_header, tmp->om_data, sizeof(struct my_header_struct));
