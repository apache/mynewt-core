os\_mutex\_release
------------------

.. code:: c

    os_error_t os_mutex_release(struct os_mutex *mu)

Release ownership of a mutex

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

OS\_BAD\_MUTEX: The mutex was not owned by the task attempting to
release it.

OS\_NOT\_STARTED: Attempt to release a mutex before the os has been
started.

Example
^^^^^^^

.. code:: c

    struct os_mutex g_mutex1;
    os_error_t err;

    err = os_mutex_pend(&g_mutex1, 0);
    assert(err == OS_OK);

    /* Perform operations requiring exclusive access */

    err = os_mutex_release(&g_mutex1);
    assert(err == OS_OK);
