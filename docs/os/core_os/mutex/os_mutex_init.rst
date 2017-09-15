os\_mutex\_init
---------------

.. code:: c

    os_error_t os_mutex_init(struct os_mutex *mu)

Initialize the mutex. Must be called before the mutex can be used.

Arguments
^^^^^^^^^

+-------------+--------------------+
| Arguments   | Description        |
+=============+====================+
| \*mu        | Pointer to mutex   |
+-------------+--------------------+

Returned values
^^^^^^^^^^^^^^^

OS\_INVALID\_PARM: returned when \*mu is NULL on entry.

OS\_OK: mutex initialized successfully.

Notes
^^^^^

Example
^^^^^^^

.. code:: c

    struct os_mutex g_mutex1;
    os_error_t err;

    err = os_mutex_init(&g_mutex1);
    assert(err == OS_OK);
