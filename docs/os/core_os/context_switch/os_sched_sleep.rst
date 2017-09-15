os\_sched\_sleep
------------------

.. code:: c

    int
    os_sched_sleep(struct os_task *t, os_time_t nticks)

Task 't' state is changed from *ready to run* to *sleeping*. Sleep time
will be specified in *nticks*.

Task will be woken up after sleep timer expires, unless there are other
signals causing it to wake up.

If *nticks* is set to *OS\_TIMEOUT\_NEVER*, task never wakes up with a
sleep timer.

Arguments
^^^^^^^^^

+-------------+----------------------------------------+
| Arguments   | Description                            |
+=============+========================================+
| t           | Pointer to task                        |
+-------------+----------------------------------------+
| nticks      | Number of ticks to sleep in OS ticks   |
+-------------+----------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success.

Notes
^^^^^

Must be called with interrupts disabled.

Example
^^^^^^^

.. code:: c

    struct os_event *
    os_eventq_get(struct os_eventq *evq)
    {
        struct os_event *ev;
        os_sr_t sr;

        OS_ENTER_CRITICAL(sr);
    pull_one:
        ev = STAILQ_FIRST(&evq->evq_list);
        if (ev) {
            STAILQ_REMOVE(&evq->evq_list, ev, os_event, ev_next);
            ev->ev_queued = 0;
        } else {
            evq->evq_task = os_sched_get_current_task();
            os_sched_sleep(evq->evq_task, OS_TIMEOUT_NEVER);
            OS_EXIT_CRITICAL(sr);

            os_sched(NULL);

            OS_ENTER_CRITICAL(sr);
            evq->evq_task = NULL;
            goto pull_one;
        }
        OS_EXIT_CRITICAL(sr);

        return (ev);
    }
