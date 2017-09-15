 os\_sanity\_check\_reset
-------------------------

.. code:: c

    int os_sanity_check_reset(struct os_sanity_check *sc)

Reset the sanity check pointed to by sc. This tells the sanity task that
this sanity check is considered valid for another ``sc_checkin_itvl``
time ticks.

 #### Arguments

+-------------+---------------------------+
| Arguments   | Description               |
+=============+===========================+
| ``sc``      | Pointer to sanity check   |
+-------------+---------------------------+

 #### Returned values

``OS_OK``: sanity check reset successful

All other error codes indicate an error.

 #### Example

.. code:: c

        int rc;

        rc = os_sanity_check_reset(&my_sc); 
        assert(rc == OS_OK);

