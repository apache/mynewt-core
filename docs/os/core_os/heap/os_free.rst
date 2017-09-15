os\_free
--------

.. code:: c

    void os_free(void *mem)

Frees previously allocated memory back to the heap.

Arguments
^^^^^^^^^

+-------------+------------------------------------+
| Arguments   | Description                        |
+=============+====================================+
| mem         | Pointer to memory being released   |
+-------------+------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

Calls C-library *free()* behind the covers.

Example
^^^^^^^

.. code:: c

       os_free(info);
