ble\_uuid\_to\_str
------------------

.. code:: c

    char *
    ble_uuid_to_str(
        const ble_uuid_t *uuid,
                    char *dst
    )

Description
~~~~~~~~~~~

Converts the specified UUID to its string representation. Example string
representations:

.. raw:: html

   <ul>

.. raw:: html

   <li>

16-bit: 0x1234

.. raw:: html

   </li>

.. raw:: html

   <li>

32-bit: 0x12345678

.. raw:: html

   </li>

.. raw:: html

   <li>

128-bit: 12345678-1234-1234-1234-123456789abc

.. raw:: html

   </li>

.. raw:: html

   </ul>

Parameters
~~~~~~~~~~

+---------------+-------------------------------+
| *Parameter*   | *Description*                 |
+===============+===============================+
| uuid          | The source UUID to convert.   |
+---------------+-------------------------------+
| dst           | The destination buffer.       |
+---------------+-------------------------------+

Returned values
~~~~~~~~~~~~~~~

A pointer to the supplied destination buffer.
