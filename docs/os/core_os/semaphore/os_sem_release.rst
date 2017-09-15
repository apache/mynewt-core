 os\_sem\_release 
------------------

.. code:: c

    os_error_t os_sem_release(struct os_sem *sem)

Release a semaphore that you are holding. This adds a token to the
semaphore.

Arguments
^^^^^^^^^

+-------------+------------------------+
| Arguments   | Description            |
+=============+========================+
| \*sem       | Pointer to semaphore   |
+-------------+------------------------+

Returned values
^^^^^^^^^^^^^^^

OS\_NOT\_STARTED: Called before os has been started.

OS\_INVALID\_PARM: returned when \*sem is NULL on entry.

OS\_OK: semaphore released successfully.

Notes
^^^^^

Example
^^^^^^^

.. code:: c

    struct os_sem g_os_sem;
    os_error_t err;

    err = os_sem_pend(&g_os_sem, OS_TIMEOUT_NEVER);
    assert(err == OS_OK);

    /* Perform operations requiring semaphore lock */

    err = os_sem_release(&g_os_sem);
    assert(err == OS_OK);
