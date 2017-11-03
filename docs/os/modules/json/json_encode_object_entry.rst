 json\_encode\_object\_entry 
-----------------------------

.. code-block:: console

       int json_encode_object_entry(struct json_encoder *encoder, char *key, struct json_value *val)

This function writes out a name for a field, followed by ":" character,
and the value itself. How value is treated depends on the type of the
value.

Arguments
^^^^^^^^^

+-------------+------------------------+
| Arguments   | Description            |
+=============+========================+
| encoder     | json\_encoder to use   |
+-------------+------------------------+
| key         | name to write out      |
+-------------+------------------------+
| val         | value to write out     |
+-------------+------------------------+

Returned values
^^^^^^^^^^^^^^^

0 on success.

Example
^^^^^^^

.. code:: c

    static int
    imgr_list(struct nmgr_jbuf *njb)
    {
        struct json_encoder *enc;
        struct json_value array;

        ...

        json_encode_object_start(enc);
        json_encode_object_entry(enc, "images", &array);
        json_encode_object_finish(enc);

        return 0;
    }

