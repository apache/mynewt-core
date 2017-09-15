os\_msys\_reset
---------------

.. code:: c

    void os_msys_reset(void) 

Resets msys module. This de-registers all pools from msys but does
nothing to the pools themselves (they still exist as mbuf pools).

Arguments
^^^^^^^^^

None

Returned values
^^^^^^^^^^^^^^^

None

Example
^^^^^^^

.. code:: c

        os_msys_reset(); 
