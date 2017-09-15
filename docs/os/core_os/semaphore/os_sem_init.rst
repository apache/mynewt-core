 os\_sem\_init
--------------

.. code:: c

    os_error_t os_sem_init(struct os_sem *sem, uint16_t tokens)    

Initialize a semaphore with a given number of tokens. Should be called
before the semaphore is used.

Arguments
^^^^^^^^^

+-------------+---------------------------------------------------+
| Arguments   | Description                                       |
+=============+===================================================+
| \*sem       | Pointer to semaphore                              |
+-------------+---------------------------------------------------+
| tokens      | Initial number of tokens allocated to semaphore   |
+-------------+---------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

OS\_INVALID\_PARM: returned when \*sem is NULL on entry.

OS\_OK: semaphore initialized successfully.

Notes
^^^^^

Example
^^^^^^^

The following example shows how to initialize a semaphore used for
exclusive access.

.. code:: c

    struct os_mutex g_os_sem;
    os_error_t err;

    err = os_sem_init(&g_os_sem, 1);
    assert(err == OS_OK);
