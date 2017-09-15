os\_msys\_register
------------------

.. code:: c

    int os_msys_register(struct os_mbuf_pool *new_pool) 

Register an mbuf pool for use as a system mbuf pool. The pool should be
initialized prior to registration.

Arguments
^^^^^^^^^

+-------------+----------------------------------------------------+
| Arguments   | Description                                        |
+=============+====================================================+
| new\_pool   | Pointer to mbuf pool to add to system mbuf pools   |
+-------------+----------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

0 on success; all other values indicate an error.

Example
^^^^^^^

.. code:: c

        rc = os_msys_register(&g_mbuf_pool);
        assert(rc == 0);
