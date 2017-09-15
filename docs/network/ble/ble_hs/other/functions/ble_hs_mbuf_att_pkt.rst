ble\_hs\_mbuf\_att\_pkt
-----------------------

.. code:: c

    struct os_mbuf *
    ble_hs_mbuf_att_pkt(void)

Description
~~~~~~~~~~~

Allocates an mbuf suitable for an ATT command packet. The resulting
packet has sufficient leading space for:

.. raw:: html

   <ul>

.. raw:: html

   <li>

ACL data header

.. raw:: html

   </li>

.. raw:: html

   <li>

L2CAP B-frame header

.. raw:: html

   </li>

.. raw:: html

   <li>

Largest ATT command base (prepare write request / response).

.. raw:: html

   </li>

.. raw:: html

   </ul>

Parameters
~~~~~~~~~~

None

Returned values
~~~~~~~~~~~~~~~

+-----------------+----------------------+
| *Value*         | *Condition*          |
+=================+======================+
| An empty mbuf   | Success.             |
+-----------------+----------------------+
| null            | Memory exhaustion.   |
+-----------------+----------------------+
