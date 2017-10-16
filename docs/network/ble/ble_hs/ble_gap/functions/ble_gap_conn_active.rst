ble\_gap\_conn\_active
----------------------

.. code:: c

    int
    ble_gap_conn_active(void)

Description
~~~~~~~~~~~

Indicates whether a connect procedure is currently in progress.

Parameters
~~~~~~~~~~

None

Returned values
~~~~~~~~~~~~~~~

+-----------+-------------------------------------+
| *Value*   | *Condition*                         |
+===========+=====================================+
| 0         | No connect procedure in progress.   |
+-----------+-------------------------------------+
| 1         | Connect procedure in progress.      |
+-----------+-------------------------------------+
