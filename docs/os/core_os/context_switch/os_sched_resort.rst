os\_sched\_resort
-------------------

.. code:: c

    void os_sched_resort(struct os_task *t)

Inform scheduler that the priority of the task *t* has changed (e.g. in
order to avoid priority inversion), and the *ready to run* list should
be re-sorted.

Arguments
^^^^^^^^^

+-------------+------------------------------------------------+
| Arguments   | Description                                    |
+=============+================================================+
| t           | Pointer to a task whose priority has changed   |
+-------------+------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

*t* must be *ready to run* before calling this.

Example
^^^^^^^

.. code:: c

    os_error_t
    os_mutex_pend(struct os_mutex *mu, uint32_t timeout)
    {
        ....
            /* Change priority of owner if needed */
        if (mu->mu_owner->t_prio > current->t_prio) {
            mu->mu_owner->t_prio = current->t_prio;
            os_sched_resort(mu->mu_owner);
        }
        ....
    }
