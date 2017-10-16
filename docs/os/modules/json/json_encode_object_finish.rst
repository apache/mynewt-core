 json\_encode\_object\_finish 
------------------------------

.. code-block:: console

       int json_encode_object_finish(struct json_encoder *encoder)

This function finalizes the encoded JSON object. This means writing out
the last "}" character.

Arguments
^^^^^^^^^

+-------------+------------------------+
| Arguments   | Description            |
+=============+========================+
| encoder     | json\_encoder to use   |
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

