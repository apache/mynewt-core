ble\_gap\_disc\_active
----------------------

.. code:: c

    int
    ble_gap_disc_active(void)

Description
~~~~~~~~~~~

Indicates whether a discovery procedure is currently in progress.

Parameters
~~~~~~~~~~

None

Returned values
~~~~~~~~~~~~~~~

+-----------+---------------------------------------+
| *Value*   | *Condition*                           |
+===========+=======================================+
| 0         | No discovery procedure in progress.   |
+-----------+---------------------------------------+
| 1         | Discovery procedure in progress.      |
+-----------+---------------------------------------+
