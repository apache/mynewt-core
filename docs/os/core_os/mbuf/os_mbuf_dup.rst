 os\_mbuf\_dup
--------------

.. code:: c

    struct os_mbuf *os_mbuf_dup(struct os_mbuf *om)

Duplicate a chain of mbufs. Return the start of the duplicated chain.

Arguments
^^^^^^^^^

+-------------+--------------------------------------+
| Arguments   | Description                          |
+=============+======================================+
| om          | Pointer to mbuf chain to duplicate   |
+-------------+--------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Pointer to the duplicated chain or NULL if not enough mbufs were
available to duplicate the chain.

Example
^^^^^^^

.. code:: c

        struct os_mbuf *om;
        struct os_mbuf *new_om;
        
        /* Make a copy of om, returning -1 if not able to duplicate om */
        new_om = os_mbuf_dup(om);
        if (!new_om) {
            return -1;
        }
