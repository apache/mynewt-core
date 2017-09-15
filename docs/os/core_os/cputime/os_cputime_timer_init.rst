os\_cputime\_timer\_init
------------------------

.. code:: c

    void os_cputime_timer_init(struct hal_timer *timer, hal_timer_cb fp, void *arg)

Initializes a cputime timer, indicated by ``timer``, with a pointer to a
callback function to call when the timer expires. When an optional
opaque argument is specified, it is passed to the timer callback
function.

The callback function has the following prototype:

.. code:: c

    void hal_timer_cb(void *arg)

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| ``timer``    | Pointer to the |
|              | hal\_timer to  |
|              | initialize.    |
|              | This value     |
|              | cannot be      |
|              | NULL.          |
+--------------+----------------+
| ``fp``       | Pointer to the |
|              | callback       |
|              | function to    |
|              | call when the  |
|              | timer expires. |
|              | This value     |
|              | cannot be      |
|              | NULL.          |
+--------------+----------------+
| ``arg``      | Optional       |
|              | opaque         |
|              | argument to    |
|              | pass to the    |
|              | hal timer      |
|              | callback       |
|              | function.      |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

Example
^^^^^^^

Example of ble\_ll setting up a response timer:

.. code:: c

    ble_ll_wfr_timer_exp(void *arg)
    {
        int rx_start;
        uint8_t lls;

             ...

        /* If we have started a reception, there is nothing to do here */
        if (!rx_start) {
            switch (lls) {
            case BLE_LL_STATE_ADV:
                ble_ll_adv_wfr_timer_exp();
                break;

             ...

            /* Do nothing here. Fall through intentional */
            case BLE_LL_STATE_INITIATING:
            default:
                break;
            }
        }
    }

    void ble_ll_init(void)
    {
           ...

        os_cputime_timer_init(&g_ble_ll_data.ll_wfr_timer, ble_ll_wfr_timer_exp, NULL);

           ...
    }
