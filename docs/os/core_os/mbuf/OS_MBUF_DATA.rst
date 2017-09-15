OS\_MBUF\_DATA
--------------

.. code:: c

    OS_MBUF_DATA(__om, __type)

Macro used to cast the data pointer of an mbuf to a given type.

Arguments
^^^^^^^^^

+-------------+----------------------------------------+
| Arguments   | Description                            |
+=============+========================================+
| \_\_om      | Pointer to mbuf (struct os\_mbuf \*)   |
+-------------+----------------------------------------+
| \_\_type    | Type to cast                           |
+-------------+----------------------------------------+

Example
^^^^^^^

.. code:: c

        struct os_mbuf *om
        uint8_t *rxbuf;

        rxbuf = OS_MBUF_DATA(om, uint8_t *);
