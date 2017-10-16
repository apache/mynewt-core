ble\_eddystone\_set\_adv\_data\_url
-----------------------------------

.. code:: c

    int
    ble_eddystone_set_adv_data_url(
        struct ble_hs_adv_fields *adv_fields,
                         uint8_t  url_scheme,
                            char *url_body,
                         uint8_t  url_body_len,
                         uint8_t  url_suffix
    )

Description
~~~~~~~~~~~

Configures the device to advertise eddystone URL beacons.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| adv\_fields    | The base         |
|                | advertisement    |
|                | fields to        |
|                | transform into   |
|                | an eddystone     |
|                | beacon. All      |
|                | configured       |
|                | fields are       |
|                | preserved; you   |
|                | probably want to |
|                | clear this       |
|                | struct before    |
|                | calling this     |
|                | function.        |
+----------------+------------------+
| url\_scheme    | The prefix of    |
|                | the URL; one of  |
|                | the              |
|                | BLE\_EDDYSTONE\_ |
|                | URL\_SCHEME      |
|                | values.          |
+----------------+------------------+
| url\_body      | The middle of    |
|                | the URL. Don't   |
|                | include the      |
|                | suffix if there  |
|                | is a suitable    |
|                | suffix code.     |
+----------------+------------------+
| url\_body\_len | The string       |
|                | length of the    |
|                | url\_body        |
|                | argument.        |
+----------------+------------------+
| url\_suffix    | The suffix of    |
|                | the URL; one of  |
|                | the              |
|                | BLE\_EDDYSTONE\_ |
|                | URL\_SUFFIX      |
|                | values; use      |
|                | BLE\_EDDYSTONE\_ |
|                | URL\_SUFFIX\_NON |
|                | E                |
|                | if the suffix is |
|                | embedded in the  |
|                | body argument.   |
+----------------+------------------+

Returned values
~~~~~~~~~~~~~~~

+------------+----------------+
| *Value*    | *Condition*    |
+============+================+
| 0          | Success.       |
+------------+----------------+
| BLE\_HS\_E | Advertising is |
| BUSY       | in progress.   |
+------------+----------------+
| BLE\_HS\_E | The specified  |
| MSGSIZE    | data is too    |
|            | large to fit   |
|            | in an          |
|            | advertisement. |
+------------+----------------+
| `Core      | Unexpected     |
| return     | error.         |
| code <../. |                |
| ./ble_hs_r |                |
| eturn_code |                |
| s/#return- |                |
| codes-core |                |
| >`__       |                |
+------------+----------------+
