ble\_hs\_mbuf\_from\_flat
-------------------------

.. code:: c

    struct os_mbuf *
    ble_hs_mbuf_from_flat(
        const void *buf,
          uint16_t  len
    )

Description
~~~~~~~~~~~

Allocates a an mbuf and fills it with the contents of the specified flat
buffer.

Parameters
~~~~~~~~~~

+---------------+----------------------------------+
| *Parameter*   | *Description*                    |
+===============+==================================+
| buf           | The flat buffer to copy from.    |
+---------------+----------------------------------+
| len           | The length of the flat buffer.   |
+---------------+----------------------------------+

Returned values
~~~~~~~~~~~~~~~

+--------------------------+----------------------+
| *Value*                  | *Condition*          |
+==========================+======================+
| A newly-allocated mbuf   | Success.             |
+--------------------------+----------------------+
| NULL                     | Memory exhaustion.   |
+--------------------------+----------------------+
