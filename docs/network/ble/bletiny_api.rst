API for bletiny app
-------------------

"bletiny" is one of the sample applications that come with Mynewt. It is
a simple shell application which provides a basic interface to the
host-side of the BLE stack. "bletiny" includes all the possible roles
(Central/Peripheral) and they may be run simultaneously. You can run
bletiny on a board and issue commands that make it behave as a central
or a peripheral with different peers.

Highlighted below are some of the ways you can use the API to establish
connections and discover services and characteristics from peer devices.
For descriptions of the full API, go to the next sections on `GAP in
bletiny <bletiny/bletiny_GAP.html>`__ and `GATT in
bletiny <bletiny/bletiny_GATT.html>`__.

Set device public address.
~~~~~~~~~~~~~~~~~~~~~~~~~~

Currently the device public address is hardcoded to
``0a:0b:0c:0d:0e:0f`` in ``bletiny`` app but you can change it by going
into its source code and initializing it to the desired value as
described in the section on how to `initialize device
addr <ini_stack/ble_devadd.html>`__.

Initiate a direct connection to a device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this case, your board is acting as a central and initiating a
connection with another BLE device. The example assumes you know the
address of the peer, either by scanning for available peers or because
you have set up the peer yourself.

.. code:: hl_lines="1"

    b conn addr_type=public addr=d4:f5:13:53:d2:43
    [ts=118609ssb, mod=64 level=2] connection complete; status=0 handle=1 peer_addr_type=0 peer_addr=0x43:0xd2:0x53:0x13:0xf5:0xd4 conn_itvl=40 conn_latency=0 supervision_timeout=256

The ``handle=1`` in the output indicates that it is connection-1.

Configure advertisements to include device name
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this case, your board is acting as a peripheral.

::

    b set adv_data name=<your-device-name>

Begin sending undirected general advertisements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this case, your board is acting as a peripheral.

::

    b adv conn=und disc=gen

Show established connections.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    b show conn

Discover and display peer's services.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is how you discover and then display the services of the peer you
established earlier across connection-1.

``hl_lines="1 2" b disc svc conn=1 b show chr [ts=132425ssb, mod=64 level=2] CONNECTION: handle=1 addr=d4:f5:13:53:d2:43 [ts=132428ssb, mod=64 level=2]     start=1 end=5 uuid=0x1800 [ts=132433ssb, mod=64 level=2]     start=6 end=16 uuid=0x1808 [ts=132437ssb, mod=64 level=2]     start=17 end=31 uuid=0x180a [ts=132441ssb, mod=64 level=2]     start=32 end=65535 uuid=00000000-0000-1000-1000000000000000``

Discover characteristics for each service on peer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following examples show how to find the characteristics for each
service available on the peer device across connection-1. The start and
end values depend on the specific services discovered using the previous
command ``b show chr``. Continuing with the example above, you can
discover the characteristics of the first service and display it using
the following commands.

``hl_lines="1 2" b disc chr conn=1 start=1 end=5 b show chr [ts=163063ssb, mod=64 level=2] CONNECTION: handle=1 addr=d4:f5:13:53:d2:43 [ts=163067ssb, mod=64 level=2]     start=1 end=5 uuid=0x1800 [ts=163071ssb, mod=64 level=2]         def_handle=2 val_handle=3 properties=0x02 uuid=0x2a00 [ts=163078ssb, mod=64 level=2]         def_handle=4 val_handle=5 properties=0x02 uuid=0x2a01 [ts=163085ssb, mod=64 level=2]     start=6 end=16 uuid=0x1808 [ts=163089ssb, mod=64 level=2]     start=17 end=31 uuid=0x180a [ts=163094ssb, mod=64 level=2]     start=32 end=65535 uuid=00000000-0000-1000-1000000000000000``

You can next discover characteristics for the second service and
display.

``hl_lines="1 2" b disc chr conn=1 start=6 end=16 b show chr [ts=180631ssb, mod=64 level=2] CONNECTION: handle=1 addr=d4:f5:13:53:d2:43 [ts=180634ssb, mod=64 level=2]     start=1 end=5 uuid=0x1800 [ts=180639ssb, mod=64 level=2]         def_handle=2 val_handle=3 properties=0x02 uuid=0x2a00 [ts=180646ssb, mod=64 level=2]         def_handle=4 val_handle=5 properties=0x02 uuid=0x2a01 [ts=180653ssb, mod=64 level=2]     start=6 end=16 uuid=0x1808 [ts=180657ssb, mod=64 level=2]         def_handle=7 val_handle=8 properties=0x10 uuid=0x2a18 [ts=180664ssb, mod=64 level=2]         def_handle=10 val_handle=11 properties=0x02 uuid=0x2a51 [ts=180672ssb, mod=64 level=2]         def_handle=12 val_handle=13 properties=0x28 uuid=0x2a52 [ts=180679ssb, mod=64 level=2]         def_handle=15 val_handle=16 properties=0x02 uuid=0x2a08 [ts=180686ssb, mod=64 level=2]     start=17 end=31 uuid=0x180a [ts=180691ssb, mod=64 level=2]     start=32 end=65535 uuid=00000000-0000-1000-1000000000000000``

Discover descriptors for each characteristic on peer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Just as before, the start and end values depend on the specific
characteristics discovered.

::

    b disc dsc conn=1 start=1 end=5
    b disc dsc conn=1 start=6 end=16
    b disc dsc conn=1 start=17 end=31

Read an attribute belonging to the peer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    b read conn=1 attr=18 attr=21

Write to an attribute belonging to the peer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    b write conn=1 attr=3 value=0x01:0x02:0x03

Perform a passive scan
~~~~~~~~~~~~~~~~~~~~~~

This is how you tell your board to listen to all advertisements around
it. The duration is specified in ms.

::

    b scan dur=1000 disc=gen type=passive filt=no_wl


