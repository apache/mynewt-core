Service Registration
====================

Attribute Set
^^^^^^^^^^^^^

The NimBLE host uses a table-based design for GATT server configuration.
The set of supported attributes are expressed as a series of tables that
resides in your C code. When possible, we recommend using a single
monolithic table, as it results in code that is simpler and less error
prone. Multiple tables can be used if it is impractical for the entire
attribute set to live in one place in your code.

*bleprph* uses a single attribute table located in the *gatt\_svr.c*
file, so let's take a look at that now. The attribute table is called
``gatt_svr_svcs``; here are the first several lines from this table:

.. code:: c

    static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
        {
            /*** Service: GAP. */
            .type               = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid128            = BLE_UUID16(BLE_GAP_SVC_UUID16),
            .characteristics    = (struct ble_gatt_chr_def[]) { {
                /*** Characteristic: Device Name. */
                .uuid128            = BLE_UUID16(BLE_GAP_CHR_UUID16_DEVICE_NAME),
                .access_cb          = gatt_svr_chr_access_gap,
                .flags              = BLE_GATT_CHR_F_READ,
            }, {
                /*** Characteristic: Appearance. */
                .uuid128            = BLE_UUID16(BLE_GAP_CHR_UUID16_APPEARANCE),
                .access_cb          = gatt_svr_chr_access_gap,
                .flags              = BLE_GATT_CHR_F_READ,
            }, {
        // [...]

As you can see, the table is an array of service definitions (
``struct ble_gatt_svc_def``). This code excerpt contains a small part of
the *GAP service*. The GAP service exposes generic information about a
device, such as its name and appearance. Support for the GAP service is
mandatory for all BLE peripherals. Let's now consider the contents of
this table in more detail.

A service definition consists of the following fields:

+----------+------------+----------+
| *Field*  | *Meaning*  | *Notes*  |
+==========+============+==========+
| type     | Specifies  | Secondar |
|          | whether    | y        |
|          | this is a  | services |
|          | primary or | are not  |
|          | secondary  | very     |
|          | service.   | common.  |
|          |            | When in  |
|          |            | doubt,   |
|          |            | specify  |
|          |            | *BLE\_GA |
|          |            | TT\_SVC\ |
|          |            | _TYPE\_P |
|          |            | RIMARY*  |
|          |            | for new  |
|          |            | services |
|          |            | .        |
+----------+------------+----------+
| uuid128  | The        | If the   |
|          | 128-bit    | service  |
|          | UUID of    | has a    |
|          | this       | 16-bit   |
|          | service.   | UUID,    |
|          |            | you can  |
|          |            | convert  |
|          |            | it to    |
|          |            | its      |
|          |            | correspo |
|          |            | nding    |
|          |            | 128-bit  |
|          |            | UUID     |
|          |            | with the |
|          |            | ``BLE_UU |
|          |            | ID16()`` |
|          |            | macro.   |
+----------+------------+----------+
| characte | The array  |          |
| ristics  | of         |          |
|          | characteri |          |
|          | stics      |          |
|          | that       |          |
|          | belong to  |          |
|          | this       |          |
|          | service.   |          |
+----------+------------+----------+

A service is little more than a container of characteristics; the
characteristics themselves are where the real action happens. A
characteristic definition consists of the following fields:

+----------+------------+----------+
| *Field*  | *Meaning*  | *Notes*  |
+==========+============+==========+
| uuid128  | The        | If the   |
|          | 128-bit    | characte |
|          | UUID of    | ristic   |
|          | this       | has a    |
|          | characteri | 16-bit   |
|          | stic.      | UUID,    |
|          |            | you can  |
|          |            | convert  |
|          |            | it to    |
|          |            | its      |
|          |            | correspo |
|          |            | nding    |
|          |            | 128-bit  |
|          |            | UUID     |
|          |            | with the |
|          |            | ``BLE_UU |
|          |            | ID16()`` |
|          |            | macro.   |
+----------+------------+----------+
| access\_ | A callback | *For     |
| cb       | function   | reads:*  |
|          | that gets  | this     |
|          | executed   | function |
|          | whenever a | generate |
|          | peer       | s        |
|          | device     | the      |
|          | accesses   | value    |
|          | this       | that     |
|          | characteri | gets     |
|          | stic.      | sent     |
|          |            | back to  |
|          |            | the      |
|          |            | peer.\ * |
|          |            | For      |
|          |            | writes:* |
|          |            | this     |
|          |            | function |
|          |            | receives |
|          |            | the      |
|          |            | written  |
|          |            | value as |
|          |            | an       |
|          |            | argument |
|          |            | .        |
+----------+------------+----------+
| flags    | Indicates  | The full |
|          | which      | list of  |
|          | operations | flags    |
|          | are        | can be   |
|          | permitted  | found    |
|          | for this   | under    |
|          | characteri | ``ble_ga |
|          | stic.      | tt_chr_f |
|          | The NimBLE | lags``   |
|          | stack      | in       |
|          | responds   | `net/nim |
|          | negatively | ble/host |
|          | when a     | /include |
|          | peer       | /host/bl |
|          | attempts   | e\_gatt. |
|          | an         | h <https |
|          | unsupporte | ://githu |
|          | d          | b.com/ap |
|          | operation. | ache/myn |
|          |            | ewt-core |
|          |            | /blob/ma |
|          |            | ster/net |
|          |            | /nimble/ |
|          |            | host/inc |
|          |            | lude/hos |
|          |            | t/ble_ga |
|          |            | tt.h>`__ |
|          |            | .        |
+----------+------------+----------+

The access callback is what implements the characteristic's behavior.
Access callbacks are described in detail in the next section: `BLE
Peripheral - Characteristic Access <bleprph-chr-access/>`__.

The service definition array and each characteristic definition array is
terminated with an empty entry, represented with a 0. The below code
listing shows the last service in the array, including terminating zeros
for the characteristic array and service array.

\`\`\`c hl\_lines="26 31" { /\*\*\* Alert Notification Service. */ .type
= BLE\_GATT\_SVC\_TYPE\_PRIMARY, .uuid128 =
BLE\_UUID16(GATT\_SVR\_SVC\_ALERT\_UUID), .characteristics = (struct
ble\_gatt\_chr\_def[]) { { .uuid128 =
BLE\_UUID16(GATT\_SVR\_CHR\_SUP\_NEW\_ALERT\_CAT\_UUID), .access\_cb =
gatt\_svr\_chr\_access\_alert, .flags = BLE\_GATT\_CHR\_F\_READ, }, {
.uuid128 = BLE\_UUID16(GATT\_SVR\_CHR\_NEW\_ALERT), .access\_cb =
gatt\_svr\_chr\_access\_alert, .flags = BLE\_GATT\_CHR\_F\_NOTIFY, }, {
.uuid128 = BLE\_UUID16(GATT\_SVR\_CHR\_SUP\_UNR\_ALERT\_CAT\_UUID),
.access\_cb = gatt\_svr\_chr\_access\_alert, .flags =
BLE\_GATT\_CHR\_F\_READ, }, { .uuid128 =
BLE\_UUID16(GATT\_SVR\_CHR\_UNR\_ALERT\_STAT\_UUID), .access\_cb =
gatt\_svr\_chr\_access\_alert, .flags = BLE\_GATT\_CHR\_F\_NOTIFY, }, {
.uuid128 = BLE\_UUID16(GATT\_SVR\_CHR\_ALERT\_NOT\_CTRL\_PT),
.access\_cb = gatt\_svr\_chr\_access\_alert, .flags =
BLE\_GATT\_CHR\_F\_WRITE, }, { 0, /* No more characteristics in this
service. \*/ } }, },

::

    {
        0, /* No more services. */
    },

\`\`\`

Registration function
^^^^^^^^^^^^^^^^^^^^^

After you have created your service table, your app needs to register it
with the NimBLE stack. This is done by calling the following function:

.. code:: c

    int
    ble_gatts_register_svcs(const struct ble_gatt_svc_def *svcs,
                            ble_gatt_register_fn *cb, void *cb_arg)

The function parameters are documented below.

+--------------+------------+----------+
| *Parameter*  | *Meaning*  | *Notes*  |
+==============+============+==========+
| svcs         | The table  |          |
|              | of         |          |
|              | services   |          |
|              | to         |          |
|              | register.  |          |
+--------------+------------+----------+
| cb           | A callback | Optional |
|              | that gets  | ;        |
|              | executed   | pass     |
|              | each time  | NULL if  |
|              | a service, | you      |
|              | characteri | don't    |
|              | stic,      | want to  |
|              | or         | be       |
|              | descriptor | notified |
|              | is         | .        |
|              | registered |          |
|              | .          |          |
+--------------+------------+----------+
| cb\_arg      | An         | Optional |
|              | argument   | ;        |
|              | that gets  | pass     |
|              | passed to  | NULL if  |
|              | the        | there is |
|              | callback   | no       |
|              | function   | callback |
|              | on each    | or if    |
|              | invocation | you      |
|              | .          | don't    |
|              |            | need a   |
|              |            | special  |
|              |            | argument |
|              |            | .        |
+--------------+------------+----------+

The ``ble_gatts_register_svcs()`` function returns 0 on success, or a
*BLE\_HS\_E[...]* error code on failure.

More detailed information about the registration callback function can
be found in the `BLE User Guide <../../../network/ble/ble_intro/>`__
(TBD).

The *bleprph* app registers its services as follows:

.. code:: c

        rc = ble_gatts_register_svcs(gatt_svr_svcs, gatt_svr_register_cb, NULL);
        assert(rc == 0);

Descriptors and Included Services
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Your peripheral can also expose descriptors and included services. These
are less common, so they are not covered in this tutorial. For more
information, see the `BLE User
Guide <../../../network/ble/ble_intro/>`__.
