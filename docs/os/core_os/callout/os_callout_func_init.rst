 os\_callout\_func\_init 
-------------------------

::

    void os_callout_func_init(struct os_callout_func *cf, struct os_eventq *evq, os_callout_func_t timo_func, void *ev_arg)

Initializes the given *struct os\_callout\_func*. Data structure is
filled in with elements given as argument.

Arguments
^^^^^^^^^

+--------------+-------------------------------------------------------+
| Arguments    | Description                                           |
+==============+=======================================================+
| cf           | Pointer to os\_callout\_func being initialized        |
+--------------+-------------------------------------------------------+
| evq          | Event queue where this gets delivered to              |
+--------------+-------------------------------------------------------+
| timo\_func   | Timeout function. Event processing should call this   |
+--------------+-------------------------------------------------------+
| ev\_arg      | Generic argument for the event                        |
+--------------+-------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

The same notes as with *os\_callout\_init()*.

Example
^^^^^^^

::

    struct os_callout_func g_native_cputimer;
    struct os_eventq g_native_cputime_evq;
    void native_cputimer_cb(void *arg);

        /* Initialize the callout function */
        os_callout_func_init(&g_native_cputimer,
                         &g_native_cputime_evq,
                         native_cputimer_cb,
                         NULL);
