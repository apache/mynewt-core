Advertising
===========

Overview
^^^^^^^^

A peripheral announces its presence to the world by broadcasting
advertisements. An advertisement typically contains additional
information about the peripheral sending it, such as the device name and
an abbreviated list of supported services. The presence of this
information helps a listening central to determine whether it is
interested in connecting to the peripheral. Advertisements are quite
limited in the amount of information they can contain, so only the most
important information should be included.

When a listening device receives an advertisement, it can choose to
connect to the peripheral, or query the sender for more information.
This second action is known as an *active scan*. A peripheral responds
to an active scan with some extra information that it couldn't fit in
its advertisement. This additional information is known as *scan
response data*. *bleprph* does not configure any scan response data, so
this feature is not discussed in the remainder of this tutorial.

*bleprph* constantly broadcasts advertisements until a central connects
to it. When a connection is terminated, *bleprph* resumes advertising.

Let's take a look at *bleprph*'s advertisement code (*main.c*):

.. code:: c

    /**
     * Enables advertising with the following parameters:
     *     o General discoverable mode.
     *     o Undirected connectable mode.
     */
    static void
    bleprph_advertise(void)
    {
        struct ble_hs_adv_fields fields;
        int rc;

        /* Set the advertisement data included in our advertisements. */
        memset(&fields, 0, sizeof fields);
        fields.name = (uint8_t *)bleprph_device_name;
        fields.name_len = strlen(bleprph_device_name);
        fields.name_is_complete = 1;
        rc = ble_gap_adv_set_fields(&fields);
        if (rc != 0) {
            BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
            return;
        }

        /* Begin advertising. */
        rc = ble_gap_adv_start(BLE_GAP_DISC_MODE_GEN, BLE_GAP_CONN_MODE_UND,
                               NULL, 0, NULL, bleprph_on_connect, NULL);
        if (rc != 0) {
            BLEPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
            return;
        }
    }

Now let's examine this code in detail.

Setting advertisement data
^^^^^^^^^^^^^^^^^^^^^^^^^^

A NimBLE peripheral specifies what information to include in its
advertisements with the following function:

.. code:: c

    int
    ble_gap_adv_set_fields(struct ble_hs_adv_fields *adv_fields)

The *adv\_fields* argument specifies the fields and their contents to
include in subsequent advertisements. The Bluetooth `Core Specification
Supplement <https://www.bluetooth.org/DocMan/handlers/DownloadDoc.ashx?doc_id=302735>`__
defines a set of standard fields that can be included in an
advertisement; the member variables of the *struct ble\_hs\_adv\_fields*
type correspond to these standard fields. Information that doesn't fit
neatly into a standard field should be put in the *manufacturing
specific data* field.

As you can see in the above code listing, the
``struct ble_hs_adv_fields`` instance is allocated on the stack. It is
OK to use the stack for this struct and the data it references, as the
``ble_gap_adv_set_fields()`` function makes a copy of all the
advertisement data before it returns. *bleprph* doesn't take full
advantange of this; it stores its device name in a static array.

The code sets three members of the *struct ble\_hs\_adv\_fields*
instance:

-  name
-  name\_len
-  name\_is\_complete

The first two fields are used to communicate the device's name and are
quite straight-forward. The third field requires some explanation.
Bluetooth specifies two name-related advertisement fields: *Shortened
Local Name* and *Complete Local Name*. Setting the ``name_is_complete``
variable to 1 or 0 tells NimBLE which of these two fields to include in
advertisements. Some other advertisement fields also correspond to
multiple variables in the field struct, so it is a good idea to review
the *ble\_hs\_adv\_fields* reference to make sure you get the details
right in your app.

Begin advertising
^^^^^^^^^^^^^^^^^

An app starts advertising with the following function:

.. code:: c

    int
    ble_gap_adv_start(uint8_t discoverable_mode, uint8_t connectable_mode,
                      uint8_t *peer_addr, uint8_t peer_addr_type,
                      struct hci_adv_params *adv_params,
                      ble_gap_conn_fn *cb, void *cb_arg)

This function allows a lot of flexibility, and it might seem daunting at
first glance. *bleprph* specifies a simple set of arguments that is
appropriate for most peripherals. When getting started on a typical
peripheral, we recommend you use the same arguments as *bleprph*, with
the exception of the last two (*cb* and *cb\_arg*). These last two
arguments will be specific to your app, so let's talk about them.

*cb* is a callback function. It gets executed when a central connects to
your peripheral after receiving an advertisement. The *cb\_arg* argument
gets passed to the *cb* callback. If your callback doesn't need the
*cb\_arg* parameter, you can do what *bleprph* does and pass *NULL*.
Once a connection is established, the *cb* callback becomes permanently
associated with the connection. All subsequent events related to the
connection are communicated to your app via calls to this callback
function. Connection callbacks are an important part of building a BLE
app, and we examine *bleprph*'s connection callback in detail in the
next section of this tutorial.

**One final note:** Your peripheral automatically stops advertising when
a central connects to it. You can immediately resume advertising if you
want to allow another central to connect, but you will need to do so
explicitly by calling ``ble_gap_adv_start()`` again. Also, be aware
NimBLE's default configuration only allows a single connection at a
time. NimBLE supports multiple concurrent connections, but you must
configure it to do so first.
