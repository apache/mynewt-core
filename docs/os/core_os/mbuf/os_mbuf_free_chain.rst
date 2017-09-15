 os\_mbuf\_free\_chain
----------------------

.. code:: c

    int os_mbuf_free_chain(struct os_mbuf *om);

Frees a chain of mbufs

Arguments
^^^^^^^^^

+-------------+-------------------------+
| Arguments   | Description             |
+=============+=========================+
| om          | Pointer to mbuf chain   |
+-------------+-------------------------+

Returned values
^^^^^^^^^^^^^^^

| 0: success
| Any other value indicates error

Notes
^^^^^

Note that for each mbuf in the chain, ``os_mbuf_free()`` is called.

Example
^^^^^^^

.. code:: c

        int rc;
        struct os_mbuf *om;

        /* Free mbuf chain */
        rc = os_mbuf_free_chain(om);
        assert(rc == 0);
