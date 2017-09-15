API for bletiny app
-------------------

"bletiny" is one of the sample applications that come with Mynewt. It is
a shell application which provides a basic interface to the host-side of
the BLE stack. "bletiny" includes all the possible roles
(Central/Peripheral) and they may be run simultaneously. You can run
bletiny on a board and issue commands that make it behave as a central
or a peripheral with different peers.

"bletiny" is a BLE application which uses the 1.0 shell. If you want to
use the 1.1 shell in BLE application please go to `btshell
app <../btshell/btshell_api.html>`__.

Highlighted below are some of the ways you can use the API to establish
connections and discover services and characteristics from peer devices.
For descriptions of the full API, go to the next sections on `GAP in
bletiny <bletiny_GAP.html>`__ and `GATT in bletiny <bletiny_GATT.html>`__.

All bletiny commands are prefixed with ``b``. This prefix distinguished
bletiny commands from other shell commands that are implemented by other
Mynewt packages.

Set device address.
~~~~~~~~~~~~~~~~~~~

On startup, bletiny has the following identity address configuration:

-  Public address: None
-  Random address: None

The below ``set`` commands can be used to change the address
configuration:

::

    b set addr_type=public addr=<device-address>
    b set addr_type=random addr=<device-address>

For example:

::

    b set addr_type=public addr=01:02:03:04:05:06
    b set addr_type=random addr=c1:aa:bb:cc:dd:ee

The address configuration can be viewed with the ``b show addr``
command, as follows:

::

    b show addr
    public_id_addr=01:02:03:04:05:06 random_id_addr=c1:aa:bb:cc:dd:ee

Initiate a direct connection to a device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this case, your board is acting as a central and initiating a
connection with another BLE device. The example assumes you know the
address of the peer, either by scanning for available peers or because
you have set up the peer yourself.

.. code:: hl_lines="1"

    b conn own_addr_type=public peer_addr_type=public peer_addr=d4:f5:13:53:d2:43
    connection established; handle=1 our_ota_addr_type=0 our_ota_addr=0a:0b:0c:0d:0e:0f out_id_addr_type=0 our_id_addr=0a:0b:0c:0d:0e:0f peer_addr_type=0 peer_addr=43:d2:53:13:f5:d4 conn_itvl=40 conn_latency=0 supervision_timeout=256 encrypted=0 authenticated=0 bonded=0

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

Discover and display peer's services, characteristics, and descriptors.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is how you discover and then display the services of the peer you
established earlier across connection-1.

``hl_lines="1 2" b disc full conn=1 b show chr [ts=132425ssb, mod=64 level=2] CONNECTION: handle=1 addr=d4:f5:13:53:d2:43 [ts=132428ssb, mod=64 level=2]     start=1 end=5 uuid=0x1800 [ts=132433ssb, mod=64 level=2]     start=6 end=16 uuid=0x1808 [ts=132437ssb, mod=64 level=2]     start=17 end=31 uuid=0x180a [ts=132441ssb, mod=64 level=2]     start=32 end=65535 uuid=00000000-0000-1000-1000000000000000``

Read an attribute belonging to the peer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    b read conn=1 attr=21

Write to an attribute belonging to the peer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    b write conn=1 attr=3 value=0x01:0x02:0x03

Perform a passive scan
~~~~~~~~~~~~~~~~~~~~~~

This is how you tell your board to listen to all advertisements around
it. The duration is specified in ms.

::

    b scan dur=1000 type=passive filt=no_wl
