 json\_encode\_object\_key 
---------------------------

.. code-block:: console

       int json_encode_object_key(struct json_encoder *encoder, char *key)

This function writes out a name for a field, followed by ":" character.
You would use this e.g. when the value that follows is a JSON object.

Arguments
^^^^^^^^^

+-------------+------------------------+
| Arguments   | Description            |
+=============+========================+
| encoder     | json\_encoder to use   |
+-------------+------------------------+
| key         | name to write out      |
+-------------+------------------------+

Returned values
^^^^^^^^^^^^^^^

0 on success.

Example
^^^^^^^

.. code:: c

    int
    nmgr_def_taskstat_read(struct nmgr_jbuf *njb)
    {
        ...

        struct json_value jv;

        json_encode_object_start(&njb->njb_enc);
        JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
        json_encode_object_entry(&njb->njb_enc, "rc", &jv);

        json_encode_object_key(&njb->njb_enc, "tasks");
        json_encode_object_start(&njb->njb_enc);
        ...
    }
