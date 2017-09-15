os\_sched\_ctx\_sw\_hook
--------------------------

.. code:: c

    void os_sched_ctx_sw_hook(struct os_task *next_t)

Performs task accounting when context switching.

This function must be called from the architecture specific context
switching routine *os\_arch\_ctx\_sw()* before resuming execution of the
*running* task.

Arguments
^^^^^^^^^

N/A

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

Example
^^^^^^^

.. code:: c

    void
    os_arch_ctx_sw(struct os_task *t)
    {
        os_sched_ctx_sw_hook(t);

        /* Set PendSV interrupt pending bit to force context switch */
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
