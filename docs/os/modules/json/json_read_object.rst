 json\_read\_object 
--------------------

.. code-block:: console

       int json_read_object(struct json_buffer *jb, const struct json_attr_t *attrs)

This function reads in JSON data stream, while looking for name/value
pairs described in *attrs*. *attrs* is an array; end of the array is
indicated by an entry with *NULL* as the name.

Arguments
^^^^^^^^^

+-------------+-----------------------------------+
| Arguments   | Description                       |
+=============+===================================+
| jb          | json\_decoder to use              |
+-------------+-----------------------------------+
| attrs       | array of attributes to look for   |
+-------------+-----------------------------------+

Returned values
^^^^^^^^^^^^^^^

0 on success.

Example
^^^^^^^

.. code:: c


    static int
    imgr_upload(struct nmgr_jbuf *njb)
    {
        ...
        const struct json_attr_t off_attr[4] = {
            [0] = {
                .attribute = "off",
                .type = t_uinteger,
                .addr.uinteger = &off,
                .nodefault = true
            },
            [1] = {
                .attribute = "data",
                .type = t_string,
                .addr.string = img_data,
                .len = sizeof(img_data)
            },
            [2] = {
                .attribute = "len",
                .type = t_uinteger,
                .addr.uinteger = &size,
                .nodefault = true
            }
        };
        ...

        rc = json_read_object(&njb->njb_buf, off_attr);
        if (rc || off == UINT_MAX) {
            rc = OS_EINVAL;
            goto err;
        }
        ...
    }

