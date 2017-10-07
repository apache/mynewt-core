BLE Eddystone
-------------

Eddystone Beacon Protocol
~~~~~~~~~~~~~~~~~~~~~~~~~

A beaconing device announces its presence to the world by broadcasting
advertisements. The Eddystone protocol is built on top of the standard
BLE advertisement specification. Eddystone supports multiple data packet
types:

-  Eddystone-UID: a unique, static ID with a 10-byte Namespace component
   and a 6-byte Instance component.
-  Eddystone-URL: a compressed URL that, once parsed and decompressed,
   is directly usable by the client.
-  Eddystone-TLM: "telemetry" packets that are broadcast alongside the
   Eddystone-UID or Eddystone-URL packets and contains beacon’s “health
   status” (e.g., battery life).
-  Eddystone-EID to broadcast an ephemeral identifier that changes every
   few minutes and allow only parties that can resolve the identifier to
   use the beacon.

`This page <https://developers.google.com/beacons/eddystone>`__
describes the Eddystone open beacon format developed by Google.

Apache Mynewt currently supports Eddystone-UID and Eddystone-URL formats
only. This tutorial will explain how to get an Eddystone-URL beacon
going on a peripheral device.

Create an Empty BLE Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This tutorial picks up where the `BLE bare bones application
tutorial <../../os/tutorials/ble_bare_bones.html>`__ concludes. The first
step in creating a beaconing device is to create an empty BLE app, as
explained in that tutorial. Before proceeding, you should have:

-  An app called "ble\_app".
-  A target called "ble\_tgt".
-  Successfully executed the app on your target device.

Add beaconing
~~~~~~~~~~~~~

Here is a brief specification of how we want our beaconing app to
behave:

1. Wait until the host and controller are in sync.
2. Configure the NimBLE stack with an address to put in its
   advertisements.
3. Advertise an eddystone URL beacon indefinitely.

Let's take these one at a time.

 #### 1. Wait for host-controller sync

The first step, waiting for host-controller-sync, is mandatory in all
BLE applications. The NimBLE stack is inoperable while the two
components are out of sync. In a combined host-controller app, the sync
happens immediately at startup. When the host and controller are
separate, sync typically occurs in less than a second.

We achieve this by configuring the NimBLE host with a callback function
that gets called when sync takes place:

\`\`\`c hl\_lines="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 24" static
void ble\_app\_set\_addr() { }

static void ble\_app\_advertise(); { }

static void ble\_app\_on\_sync(void) { /\* Generate a non-resolvable
private address. \*/ ble\_app\_set\_addr();

::

    /* Advertise indefinitely. */
    ble_app_advertise();

}

int main(int argc, char \*\*argv) { sysinit();

::

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    /* As the last thing, process events from default event queue. */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

} \`\`\`

``ble_hs_cfg.sync_cb`` points to the function that should be called when
sync occurs. Our callback function, ``ble_app_on_sync()``, kicks off the
control flow that we specified above. Now we need to fill in the two
stub functions.

 #### 2. Configure the NimBLE stack with an address

A BLE device needs an address to do just about anything. Some devices
have a public Bluetooth address burned into them, but this is not always
the case. Furthermore, the NimBLE controller might not know how to read
an address out of your particular hardware. For a beaconing device, we
generally don't care what address gets used since nothing will be
connecting to us.

A reliable solution is to generate a *non-resolvable private address*
(nRPA) each time the application runs. Such an address contains no
identifying information, and they are expected to change frequently.

\`\`\`c hl\_lines="4 5 6 7 8 9 10 11" static void
ble\_app\_set\_addr(void) { ble\_addr\_t addr; int rc;

::

    rc = ble_hs_id_gen_rnd(1, &addr);
    assert(rc == 0);

    rc = ble_hs_id_set_rnd(addr.val);
    assert(rc == 0);

}

static void ble\_app\_advertise(); { }

static void ble\_app\_on\_sync(void) { /\* Generate a non-resolvable
private address. \*/ ble\_app\_set\_addr();

::

    /* Advertise indefinitely. */
    ble_app_advertise();

} \`\`\`

Our new function, ``ble_app_set_addr()``, makes two calls into the
stack:

-  ```ble_hs_id_gen_rnd`` <../../network/ble/ble_hs/ble_hs_id/functions/ble_hs_id_gen_rnd.html>`__:
   Generate an nRPA.
-  ```ble_hs_id_set_rnd`` <../../network/ble/ble_hs/ble_hs_id/functions/ble_hs_id_set_rnd.html>`__:
   Configure NimBLE to use the newly-generated address.

You can click either of the function names for more detailed
documentation.

 #### 3. Advertise indefinitely

The first step in advertising is to configure the host with advertising
data. This operation tells the host what data to use for the contents of
its advertisements. The NimBLE host provides special helper functions
for configuring eddystone advertisement data:

-  ```ble_eddystone_set_adv_data_uid`` <../../network/ble/ble_hs/other/functions/ble_eddystone_set_adv_data_uid.html>`__
-  ```ble_eddystone_set_adv_data_url`` <../../network/ble/ble_hs/other/functions/ble_eddystone_set_adv_data_url.html>`__

Our application will advertise eddystone URL beacons, so we are
interested in the second function. We reproduce the function prototype
here:

.. code:: c

    int
    ble_eddystone_set_adv_data_url(
        struct ble_hs_adv_fields *adv_fields,
                         uint8_t  url_scheme,
                            char *url_body,
                         uint8_t  url_body_len,
                         uint8_t  url_suffix
    )

We'll advertise the Mynewt URL: *https://mynewt.apache.org*. Eddystone
beacons use a form of URL compression to accommodate the limited space
available in Bluetooth advertisements. The ``url_scheme`` and
``url_suffix`` fields implement this compression; they are single byte
fields which correspond to strings commonly found in URLs. The following
arguments translate to the https://mynewt.apache.org URL:

+---------------+--------------------------------------+
| Parameter     | Value                                |
+===============+======================================+
| url\_scheme   | ``BLE_EDDYSTONE_URL_SCHEME_HTTPS``   |
+---------------+--------------------------------------+
| url\_body     | "mynewt.apache"                      |
+---------------+--------------------------------------+
| url\_suffix   | ``BLE_EDDYSTONE_URL_SUFFIX_ORG``     |
+---------------+--------------------------------------+

.. code:: c

    static void
    ble_app_advertise(void)
    {
        struct ble_hs_adv_fields fields;
        int rc;

        /* Configure an eddystone URL beacon to be advertised;
         * URL: https://apache.mynewt.org 
         */
        fields = (struct ble_hs_adv_fields){ 0 };
        rc = ble_eddystone_set_adv_data_url(&fields,
                                            BLE_EDDYSTONE_URL_SCHEME_HTTPS,
                                            "mynewt.apache",
                                            13,
                                            BLE_EDDYSTONE_URL_SUFFIX_ORG);
        assert(rc == 0);

        /* TODO: Begin advertising. */
    }

Now that the host knows what to advertise, the next step is to actually
begin advertising. The function to initiate advertising is:
```ble_gap_adv_start`` <../../network/ble/ble_hs/ble_gap/functions/ble_gap_adv_start.html>`__.
This function takes several parameters. For simplicity, we reproduce the
function prototype here:

.. code:: c

    int
    ble_gap_adv_start(
                                uint8_t  own_addr_type,
                       const ble_addr_t *direct_addr,
                                int32_t  duration_ms,
        const struct ble_gap_adv_params *adv_params,
                       ble_gap_event_fn *cb,
                                   void *cb_arg
    )

This function gives an application quite a bit of freedom in how
advertising is to be done. The default values are mostly fine for our
simple beaconing application. We will pass the following values to this
function:

+--------------+----------+----------+
| Parameter    | Value    | Notes    |
+==============+==========+==========+
| own\_addr\_t | BLE\_OWN | Use the  |
| ype          | \_ADDR\_ | nRPA we  |
|              | RANDOM   | generate |
|              |          | d        |
|              |          | earlier. |
+--------------+----------+----------+
| direct\_addr | NULL     | We are   |
|              |          | broadcas |
|              |          | ting,    |
|              |          | not      |
|              |          | targetin |
|              |          | g        |
|              |          | a peer.  |
+--------------+----------+----------+
| duration\_ms | BLE\_HS\ | Advertis |
|              | _FOREVER | e        |
|              |          | indefini |
|              |          | tely.    |
+--------------+----------+----------+
| adv\_params  | defaults | Can be   |
|              |          | used to  |
|              |          | specify  |
|              |          | low      |
|              |          | level    |
|              |          | advertis |
|              |          | ing      |
|              |          | paramete |
|              |          | rs.      |
+--------------+----------+----------+
| cb           | NULL     | We are   |
|              |          | non-conn |
|              |          | ectable, |
|              |          | so no    |
|              |          | need for |
|              |          | an event |
|              |          | callback |
|              |          | .        |
+--------------+----------+----------+
| cb\_arg      | NULL     | No       |
|              |          | callback |
|              |          | implies  |
|              |          | no       |
|              |          | callback |
|              |          | argument |
|              |          | .        |
+--------------+----------+----------+

These arguments are mostly self-explanatory. The exception is
``adv_params``, which can be used to specify a number of low-level
parameters. For a beaconing application, the default settings are
appropriate. We specify default settings by providing a zero-filled
instance of the ``ble_gap_adv_params`` struct as our argument.

\`\`\`c hl\_lines="4 19 20 21 22 23" static void
ble\_app\_advertise(void) { struct ble\_gap\_adv\_params adv\_params;
struct ble\_hs\_adv\_fields fields; int rc;

::

    /* Configure an eddystone URL beacon to be advertised;
     * URL: https://apache.mynewt.org 
     */
    fields = (struct ble_hs_adv_fields){ 0 };
    rc = ble_eddystone_set_adv_data_url(&fields,
                                        BLE_EDDYSTONE_URL_SCHEME_HTTPS,
                                        "mynewt.apache",
                                        13,
                                        BLE_EDDYSTONE_URL_SUFFIX_ORG);
    assert(rc == 0);

    /* Begin advertising. */
    adv_params = (struct ble_gap_adv_params){ 0 };
    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    assert(rc == 0);

} \`\`\`

Conclusion
~~~~~~~~~~

That's it! Now when you run this app on your board, you should be able
to see it with all your eddystone-aware devices. You can test it out
with the ``newt run`` command.

Source Listing
~~~~~~~~~~~~~~

For reference, here is the complete application source:

.. code:: c

    #include "sysinit/sysinit.h"
    #include "os/os.h"
    #include "console/console.h"
    #include "host/ble_hs.h"

    static void
    ble_app_set_addr(void)
    {
        ble_addr_t addr;
        int rc;

        rc = ble_hs_id_gen_rnd(1, &addr);
        assert(rc == 0);

        rc = ble_hs_id_set_rnd(addr.val);
        assert(rc == 0);
    }

    static void
    ble_app_advertise(void)
    {
        struct ble_gap_adv_params adv_params;
        struct ble_hs_adv_fields fields;
        int rc;

        /* Configure an eddystone URL beacon to be advertised;
         * URL: https://apache.mynewt.org 
         */
        fields = (struct ble_hs_adv_fields){ 0 };
        rc = ble_eddystone_set_adv_data_url(&fields,
                                            BLE_EDDYSTONE_URL_SCHEME_HTTPS,
                                            "mynewt.apache",
                                            13,
                                            BLE_EDDYSTONE_URL_SUFFIX_ORG);
        assert(rc == 0);

        /* Begin advertising. */
        adv_params = (struct ble_gap_adv_params){ 0 };
        rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                               &adv_params, NULL, NULL);
        assert(rc == 0);
    }

    static void
    ble_app_on_sync(void)
    {
        /* Generate a non-resolvable private address. */
        ble_app_set_addr();

        /* Advertise indefinitely. */
        ble_app_advertise();
    }

    int
    main(int argc, char **argv)
    {
        sysinit();

        ble_hs_cfg.sync_cb = ble_app_on_sync;

        /* As the last thing, process events from default event queue. */
        while (1) {
            os_eventq_run(os_eventq_dflt_get());
        }
    }
