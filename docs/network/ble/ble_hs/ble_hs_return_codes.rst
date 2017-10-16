NimBLE Host Return Codes
------------------------

-  `Introduction <#introduction>`__

   -  `Summary <#summary>`__
   -  `Example <#example>`__

-  `Return Code Reference <#return-code-reference>`__

   -  `Return codes - Core <#return-codes-core>`__
   -  `Return codes - ATT <#return-codes-att>`__
   -  `Return codes - HCI <#return-codes-hci>`__
   -  `Return codes - L2CAP <#return-codes-l2cap>`__
   -  `Return codes - Security manager
      (us) <#return-codes-security-manager-(us)>`__
   -  `Return codes - Security manager
      (peer) <#return-codes-security-manager-(peer)>`__

Introduction
~~~~~~~~~~~~

Summary
^^^^^^^

The NimBLE host reports status to the application via a set of return
codes. The host encompasses several layers of the Bluetooth
specification that each defines its own set of status codes. Rather than
"abstract away" information from lower layers that the application
developer might find useful, the NimBLE host aims to indicate precisely
what happened when something fails. Consequently, the host utilizes a
rather large set of return codes.

A return code of 0 indicates success. For failure conditions, the return
codes are partitioned into five separate sets:

+--------+----------------+
| Set    | Condition      |
+========+================+
| Core   | Errors         |
|        | detected       |
|        | internally by  |
|        | the NimBLE     |
|        | host.          |
+--------+----------------+
| ATT    | The ATT server |
|        | has reported a |
|        | failure via    |
|        | the            |
|        | transmission   |
|        | of an ATT      |
|        | Error          |
|        | Response. The  |
|        | return code    |
|        | corresponds to |
|        | the value of   |
|        | the Error Code |
|        | field in the   |
|        | response.      |
+--------+----------------+
| HCI    | The controller |
|        | has reported   |
|        | an error to    |
|        | the host via a |
|        | command        |
|        | complete or    |
|        | command status |
|        | HCI event. The |
|        | return code    |
|        | corresponds to |
|        | the value of   |
|        | the Status     |
|        | field in the   |
|        | event.         |
+--------+----------------+
| L2CAP  | An L2CAP       |
|        | signaling      |
|        | procedure has  |
|        | failed and an  |
|        | L2CAP Command  |
|        | Reject was     |
|        | sent as a      |
|        | result. The    |
|        | return code    |
|        | corresponds to |
|        | the value of   |
|        | the Reason     |
|        | field in the   |
|        | command.       |
+--------+----------------+
| Securi | The host       |
| ty     | detected an    |
| manage | error during a |
| r      | security       |
| (us)   | manager        |
|        | procedure and  |
|        | sent a Pairing |
|        | Failed command |
|        | to the peer.   |
|        | The return     |
|        | code           |
|        | corresponds to |
|        | the value of   |
|        | the Reason     |
|        | field in the   |
|        | Pairing Failed |
|        | command.       |
+--------+----------------+
| Securi | A security     |
| ty     | manager        |
| manage | procedure      |
| r      | failed because |
| (peer) | the peer sent  |
|        | us a Pairing   |
|        | Failed         |
|        | command. The   |
|        | return code    |
|        | corresponds to |
|        | the value of   |
|        | the Reason     |
|        | field in the   |
|        | Pairing Failed |
|        | command.       |
+--------+----------------+

The return codes in the core set are defined by the NimBLE Host. The
other sets are defined in the Bluetooth specification; the codes in this
latter group are referred to as *formal status codes*. As defined in the
Bluetooth specification, the formal status code sets are not disjoint.
That is, they overlap. For example, the spec defines a status code of 1
to have all of the following meanings:

+---------+----------------------------+
| Layer   | Meaning                    |
+=========+============================+
| ATT     | Invalid handle.            |
+---------+----------------------------+
| HCI     | Unknown HCI command.       |
+---------+----------------------------+
| L2CAP   | Signalling MTU exceeded.   |
+---------+----------------------------+
| SM      | Passkey entry failed.      |
+---------+----------------------------+

Clearly, the host can't just return an unadorned formal status code and
expect the application to make sense of it. To resolve this ambiguity,
the NimBLE host divides the full range of an int into several subranges.
Each subrange corresponds to one of the five return code sets. For
example, the ATT set is mapped onto the subrange *[0x100, 0x200)*. To
indicate an ATT error of 3 (write not permitted), the NimBLE host
returns a value 0x103 to the application.

The host defines a set of convenience macros for converting from a
formal status code to NimBLE host status code. These macros are
documented in the table below.

+----------------------------+---------------------------+--------------+
| Macro                      | Status code set           | Base value   |
+============================+===========================+==============+
| BLE\_HS\_ATT\_ERR()        | ATT                       | 0x100        |
+----------------------------+---------------------------+--------------+
| BLE\_HS\_HCI\_ERR()        | HCI                       | 0x200        |
+----------------------------+---------------------------+--------------+
| BLE\_HS\_L2C\_ERR()        | L2CAP                     | 0x300        |
+----------------------------+---------------------------+--------------+
| BLE\_HS\_SM\_US\_ERR()     | Security manager (us)     | 0x400        |
+----------------------------+---------------------------+--------------+
| BLE\_HS\_SM\_PEER\_ERR()   | Security manager (peer)   | 0x500        |
+----------------------------+---------------------------+--------------+

Example
^^^^^^^

The following example demonstrates how an application might determine
which error is being reported by the host. In this example, the
application performs the GAP encryption procedure and checks the return
code. To simplify the example, the application uses a hypothetical
*my\_blocking\_enc\_proc()* function, which blocks until the pairing
operation has completed.

.. code:: c

    void
    encrypt_connection(uint16_t conn_handle)
    {
        int rc;

        /* Perform a blocking GAP encryption procedure. */
        rc = my_blocking_enc_proc(conn_handle);
        switch (rc) {
        case 0:
            console_printf("success - link successfully encrypted\n");
            break;

        case BLE_HS_ENOTCONN:
            console_printf("failure - no connection with handle %d\n",
                           conn_handle);
            break;

        case BLE_HS_ERR_SM_US_BASE(BLE_SM_ERR_CONFIRM_MISMATCH):
            console_printf("failure - mismatch in peer's confirm and random "
                           "commands.\n");
            break;

        case BLE_HS_ERR_SM_PEER_BASE(BLE_SM_ERR_CONFIRM_MISMATCH):
            console_printf("failure - peer reports mismatch in our confirm and "
                           "random commands.\n");
            break;

        default:
            console_printf("failure - other error: 0x%04x\n", rc);
            break;
        }
    }

Return Code Reference
~~~~~~~~~~~~~~~~~~~~~

Header
^^^^^^

All NimBLE host return codes are made accessible by including the
following header:

.. code:: c

    #include "host/ble_hs.h"

Return codes - Core
^^^^^^^^^^^^^^^^^^^

The precise meaning of each of these error codes depends on the function
that returns it. The API reference for a particular function indicates
the conditions under which each of these codes are returned.

+----------+-------------------+-------------------------------------------------+
| Value    | Name              | Condition                                       |
+==========+===================+=================================================+
| 0x00     | *N/A*             | Success                                         |
+----------+-------------------+-------------------------------------------------+
| 0x01     | BLE\_HS\_EAGAIN   | Temporary failure; try again.                   |
+----------+-------------------+-------------------------------------------------+
| 0x02     | BLE\_HS\_EALREADY | Operation already in progress or completed.     |
+----------+-------------------+-------------------------------------------------+
| 0x03     | BLE\_HS\_EINVAL   | One or more arguments are invalid.              |
+----------+-------------------+-------------------------------------------------+
| 0x04     | BLE\_HS\_EMSGSIZE | The provided buffer is too small.               |
+----------+-------------------+-------------------------------------------------+
| 0x05     | BLE\_HS\_ENOENT   | No entry matching the specified criteria.       |
+----------+-------------------+-------------------------------------------------+
| 0x06     | BLE\_HS\_ENOMEM   | Operation failed due to resource exhaustion.    |
+----------+-------------------+-------------------------------------------------+
| 0x07     | BLE\_HS\_ENOTCONN | No open connection with the specified handle.   |
+----------+-------------------+-------------------------------------------------+
| 0x08     | BLE\_HS\_ENOTSUP  | Operation disabled at compile time.             |
+----------+-------------------+-------------------------------------------------+
| 0x09     | BLE\_HS\_EAPP     | Application callback behaved unexpectedly.      |
+----------+-------------------+-------------------------------------------------+
| 0x0a     | BLE\_HS\_EBADDATA | Command from peer is invalid.                   |
+----------+-------------------+-------------------------------------------------+
| 0x0b     | BLE\_HS\_EOS      | Mynewt OS error.                                |
+----------+-------------------+-------------------------------------------------+
| 0x0c     | BLE\_HS\_ECONTROL | Event from controller is invalid.               |
|          | LER               |                                                 |
+----------+-------------------+-------------------------------------------------+
| 0x0d     | BLE\_HS\_ETIMEOUT | Operation timed out.                            |
+----------+-------------------+-------------------------------------------------+
| 0x0e     | BLE\_HS\_EDONE    | Operation completed successfully.               |
+----------+-------------------+-------------------------------------------------+
| 0x0f     | BLE\_HS\_EBUSY    | Operation cannot be performed until procedure   |
|          |                   | completes.                                      |
+----------+-------------------+-------------------------------------------------+
| 0x10     | BLE\_HS\_EREJECT  | Peer rejected a connection parameter update     |
|          |                   | request.                                        |
+----------+-------------------+-------------------------------------------------+
| 0x11     | BLE\_HS\_EUNKNOWN | Unexpected failure; catch all.                  |
+----------+-------------------+-------------------------------------------------+
| 0x12     | BLE\_HS\_EROLE    | Operation requires different role (e.g.,        |
|          |                   | central vs. peripheral).                        |
+----------+-------------------+-------------------------------------------------+
| 0x13     | BLE\_HS\_ETIMEOUT | HCI request timed out; controller unresponsive. |
|          | \_HCI             |                                                 |
+----------+-------------------+-------------------------------------------------+
| 0x14     | BLE\_HS\_ENOMEM\_ | Controller failed to send event due to memory   |
|          | EVT               | exhaustion (combined host-controller only).     |
+----------+-------------------+-------------------------------------------------+
| 0x15     | BLE\_HS\_ENOADDR  | Operation requires an identity address but none |
|          |                   | configured.                                     |
+----------+-------------------+-------------------------------------------------+
| 0x16     | BLE\_HS\_ENOTSYNC | Attempt to use the host before it is synced     |
|          | ED                | with controller.                                |
+----------+-------------------+-------------------------------------------------+
| 0x17     | BLE\_HS\_EAUTHEN  | Insufficient authentication.                    |
+----------+-------------------+-------------------------------------------------+
| 0x18     | BLE\_HS\_EAUTHOR  | Insufficient authorization.                     |
+----------+-------------------+-------------------------------------------------+
| 0x19     | BLE\_HS\_EENCRYPT | Insufficient encryption level.                  |
+----------+-------------------+-------------------------------------------------+
| 0x1a     | BLE\_HS\_EENCRYPT | Insufficient key size.                          |
|          | \_KEY\_SZ         |                                                 |
+----------+-------------------+-------------------------------------------------+
| 0x1b     | BLE\_HS\_ESTORE\_ | Storage at capacity.                            |
|          | CAP               |                                                 |
+----------+-------------------+-------------------------------------------------+
| 0x1c     | BLE\_HS\_ESTORE\_ | Storage IO error.                               |
|          | FAIL              |                                                 |
+----------+-------------------+-------------------------------------------------+

Return codes - ATT
^^^^^^^^^^^^^^^^^^

+-----------------+-----------------+-----------+----------------+
| NimBLE Value    | Formal Value    | Name      | Condition      |
+=================+=================+===========+================+
| 0x0101          | 0x01            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_INV | handle given   |
|                 |                 | ALID\_HAN | was not valid  |
|                 |                 | DLE       | on this        |
|                 |                 |           | server.        |
+-----------------+-----------------+-----------+----------------+
| 0x0102          | 0x02            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_REA | cannot be      |
|                 |                 | D\_NOT\_P | read.          |
|                 |                 | ERMITTED  |                |
+-----------------+-----------------+-----------+----------------+
| 0x0103          | 0x03            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_WRI | cannot be      |
|                 |                 | TE\_NOT\_ | written.       |
|                 |                 | PERMITTED |                |
+-----------------+-----------------+-----------+----------------+
| 0x0104          | 0x04            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_INV | PDU was        |
|                 |                 | ALID\_PDU | invalid.       |
+-----------------+-----------------+-----------+----------------+
| 0x0105          | 0x05            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_INS | requires       |
|                 |                 | UFFICIENT | authentication |
|                 |                 | \_AUTHEN  | before it can  |
|                 |                 |           | be read or     |
|                 |                 |           | written.       |
+-----------------+-----------------+-----------+----------------+
| 0x0106          | 0x06            | BLE\_ATT\ | Attribute      |
|                 |                 | _ERR\_REQ | server does    |
|                 |                 | \_NOT\_SU | not support    |
|                 |                 | PPORTED   | the request    |
|                 |                 |           | received from  |
|                 |                 |           | the client.    |
+-----------------+-----------------+-----------+----------------+
| 0x0107          | 0x07            | BLE\_ATT\ | Offset         |
|                 |                 | _ERR\_INV | specified was  |
|                 |                 | ALID\_OFF | past the end   |
|                 |                 | SET       | of the         |
|                 |                 |           | attribute.     |
+-----------------+-----------------+-----------+----------------+
| 0x0108          | 0x08            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_INS | requires       |
|                 |                 | UFFICIENT | authorization  |
|                 |                 | \_AUTHOR  | before it can  |
|                 |                 |           | be read or     |
|                 |                 |           | written.       |
+-----------------+-----------------+-----------+----------------+
| 0x0109          | 0x09            | BLE\_ATT\ | Too many       |
|                 |                 | _ERR\_PRE | prepare writes |
|                 |                 | PARE\_QUE | have been      |
|                 |                 | UE\_FULL  | queued.        |
+-----------------+-----------------+-----------+----------------+
| 0x010a          | 0x0a            | BLE\_ATT\ | No attribute   |
|                 |                 | _ERR\_ATT | found within   |
|                 |                 | R\_NOT\_F | the given      |
|                 |                 | OUND      | attribute      |
|                 |                 |           | handle range.  |
+-----------------+-----------------+-----------+----------------+
| 0x010b          | 0x0b            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_ATT | cannot be read |
|                 |                 | R\_NOT\_L | or written     |
|                 |                 | ONG       | using the Read |
|                 |                 |           | Blob Request.  |
+-----------------+-----------------+-----------+----------------+
| 0x010c          | 0x0c            | BLE\_ATT\ | The Encryption |
|                 |                 | _ERR\_INS | Key Size used  |
|                 |                 | UFFICIENT | for encrypting |
|                 |                 | \_KEY\_SZ | this link is   |
|                 |                 |           | insufficient.  |
+-----------------+-----------------+-----------+----------------+
| 0x010d          | 0x0d            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_INV | value length   |
|                 |                 | ALID\_ATT | is invalid for |
|                 |                 | R\_VALUE\ | the operation. |
|                 |                 | _LEN      |                |
+-----------------+-----------------+-----------+----------------+
| 0x010e          | 0x0e            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_UNL | request that   |
|                 |                 | IKELY     | was requested  |
|                 |                 |           | has            |
|                 |                 |           | encountered an |
|                 |                 |           | error that was |
|                 |                 |           | unlikely, and  |
|                 |                 |           | therefore      |
|                 |                 |           | could not be   |
|                 |                 |           | completed as   |
|                 |                 |           | requested.     |
+-----------------+-----------------+-----------+----------------+
| 0x010f          | 0x0f            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_INS | requires       |
|                 |                 | UFFICIENT | encryption     |
|                 |                 | \_ENC     | before it can  |
|                 |                 |           | be read or     |
|                 |                 |           | written.       |
+-----------------+-----------------+-----------+----------------+
| 0x0110          | 0x10            | BLE\_ATT\ | The attribute  |
|                 |                 | _ERR\_UNS | type is not a  |
|                 |                 | UPPORTED\ | supported      |
|                 |                 | _GROUP    | grouping       |
|                 |                 |           | attribute as   |
|                 |                 |           | defined by a   |
|                 |                 |           | higher layer   |
|                 |                 |           | specification. |
+-----------------+-----------------+-----------+----------------+
| 0x0111          | 0x11            | BLE\_ATT\ | Insufficient   |
|                 |                 | _ERR\_INS | Resources to   |
|                 |                 | UFFICIENT | complete the   |
|                 |                 | \_RES     | request.       |
+-----------------+-----------------+-----------+----------------+

Return codes - HCI
^^^^^^^^^^^^^^^^^^

+-----------------+-----------------+-----------+----------------+
| NimBLE Value    | Formal Value    | Name      | Condition      |
+=================+=================+===========+================+
| 0x0201          | 0x01            | BLE\_ERR\ | Unknown HCI    |
|                 |                 | _UNKNOWN\ | Command        |
|                 |                 | _HCI\_CMD |                |
+-----------------+-----------------+-----------+----------------+
| 0x0202          | 0x02            | BLE\_ERR\ | Unknown        |
|                 |                 | _UNK\_CON | Connection     |
|                 |                 | N\_ID     | Identifier     |
+-----------------+-----------------+-----------+----------------+
| 0x0203          | 0x03            | BLE\_ERR\ | Hardware       |
|                 |                 | _HW\_FAIL | Failure        |
+-----------------+-----------------+-----------+----------------+
| 0x0204          | 0x04            | BLE\_ERR\ | Page Timeout   |
|                 |                 | _PAGE\_TM |                |
|                 |                 | O         |                |
+-----------------+-----------------+-----------+----------------+
| 0x0205          | 0x05            | BLE\_ERR\ | Authentication |
|                 |                 | _AUTH\_FA | Failure        |
|                 |                 | IL        |                |
+-----------------+-----------------+-----------+----------------+
| 0x0206          | 0x06            | BLE\_ERR\ | PIN or Key     |
|                 |                 | _PINKEY\_ | Missing        |
|                 |                 | MISSING   |                |
+-----------------+-----------------+-----------+----------------+
| 0x0207          | 0x07            | BLE\_ERR\ | Memory         |
|                 |                 | _MEM\_CAP | Capacity       |
|                 |                 | ACITY     | Exceeded       |
+-----------------+-----------------+-----------+----------------+
| 0x0208          | 0x08            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_SP | Timeout        |
|                 |                 | VN\_TMO   |                |
+-----------------+-----------------+-----------+----------------+
| 0x0209          | 0x09            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_LI | Limit Exceeded |
|                 |                 | MIT       |                |
+-----------------+-----------------+-----------+----------------+
| 0x020a          | 0x0a            | BLE\_ERR\ | Synchronous    |
|                 |                 | _SYNCH\_C | Connection     |
|                 |                 | ONN\_LIMI | Limit To A     |
|                 |                 | T         | Device         |
|                 |                 |           | Exceeded       |
+-----------------+-----------------+-----------+----------------+
| 0x020b          | 0x0b            | BLE\_ERR\ | ACL Connection |
|                 |                 | _ACL\_CON | Already Exists |
|                 |                 | N\_EXISTS |                |
+-----------------+-----------------+-----------+----------------+
| 0x020c          | 0x0c            | BLE\_ERR\ | Command        |
|                 |                 | _CMD\_DIS | Disallowed     |
|                 |                 | ALLOWED   |                |
+-----------------+-----------------+-----------+----------------+
| 0x020d          | 0x0d            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_RE | Rejected due   |
|                 |                 | J\_RESOUR | to Limited     |
|                 |                 | CES       | Resources      |
+-----------------+-----------------+-----------+----------------+
| 0x020e          | 0x0e            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_RE | Rejected Due   |
|                 |                 | J\_SECURI | To Security    |
|                 |                 | TY        | Reasons        |
+-----------------+-----------------+-----------+----------------+
| 0x020f          | 0x0f            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_RE | Rejected due   |
|                 |                 | J\_BD\_AD | to             |
|                 |                 | DR        | Unacceptable   |
|                 |                 |           | BD\_ADDR       |
+-----------------+-----------------+-----------+----------------+
| 0x0210          | 0x10            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_AC | Accept Timeout |
|                 |                 | CEPT\_TMO | Exceeded       |
+-----------------+-----------------+-----------+----------------+
| 0x0211          | 0x11            | BLE\_ERR\ | Unsupported    |
|                 |                 | _UNSUPPOR | Feature or     |
|                 |                 | TED       | Parameter      |
|                 |                 |           | Value          |
+-----------------+-----------------+-----------+----------------+
| 0x0212          | 0x12            | BLE\_ERR\ | Invalid HCI    |
|                 |                 | _INV\_HCI | Command        |
|                 |                 | \_CMD\_PA | Parameters     |
|                 |                 | RMS       |                |
+-----------------+-----------------+-----------+----------------+
| 0x0213          | 0x13            | BLE\_ERR\ | Remote User    |
|                 |                 | _REM\_USE | Terminated     |
|                 |                 | R\_CONN\_ | Connection     |
|                 |                 | TERM      |                |
+-----------------+-----------------+-----------+----------------+
| 0x0214          | 0x14            | BLE\_ERR\ | Remote Device  |
|                 |                 | _RD\_CONN | Terminated     |
|                 |                 | \_TERM\_R | Connection due |
|                 |                 | ESRCS     | to Low         |
|                 |                 |           | Resources      |
+-----------------+-----------------+-----------+----------------+
| 0x0215          | 0x15            | BLE\_ERR\ | Remote Device  |
|                 |                 | _RD\_CONN | Terminated     |
|                 |                 | \_TERM\_P | Connection due |
|                 |                 | WROFF     | to Power Off   |
+-----------------+-----------------+-----------+----------------+
| 0x0216          | 0x16            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_TE | Terminated By  |
|                 |                 | RM\_LOCAL | Local Host     |
+-----------------+-----------------+-----------+----------------+
| 0x0217          | 0x17            | BLE\_ERR\ | Repeated       |
|                 |                 | _REPEATED | Attempts       |
|                 |                 | \_ATTEMPT |                |
|                 |                 | S         |                |
+-----------------+-----------------+-----------+----------------+
| 0x0218          | 0x18            | BLE\_ERR\ | Pairing Not    |
|                 |                 | _NO\_PAIR | Allowed        |
|                 |                 | ING       |                |
+-----------------+-----------------+-----------+----------------+
| 0x0219          | 0x19            | BLE\_ERR\ | Unknown LMP    |
|                 |                 | _UNK\_LMP | PDU            |
+-----------------+-----------------+-----------+----------------+
| 0x021a          | 0x1a            | BLE\_ERR\ | Unsupported    |
|                 |                 | _UNSUPP\_ | Remote Feature |
|                 |                 | REM\_FEAT | / Unsupported  |
|                 |                 | URE       | LMP Feature    |
+-----------------+-----------------+-----------+----------------+
| 0x021b          | 0x1b            | BLE\_ERR\ | SCO Offset     |
|                 |                 | _SCO\_OFF | Rejected       |
|                 |                 | SET       |                |
+-----------------+-----------------+-----------+----------------+
| 0x021c          | 0x1c            | BLE\_ERR\ | SCO Interval   |
|                 |                 | _SCO\_ITV | Rejected       |
|                 |                 | L         |                |
+-----------------+-----------------+-----------+----------------+
| 0x021d          | 0x1d            | BLE\_ERR\ | SCO Air Mode   |
|                 |                 | _SCO\_AIR | Rejected       |
|                 |                 | \_MODE    |                |
+-----------------+-----------------+-----------+----------------+
| 0x021e          | 0x1e            | BLE\_ERR\ | Invalid LMP    |
|                 |                 | _INV\_LMP | Parameters /   |
|                 |                 | \_LL\_PAR | Invalid LL     |
|                 |                 | M         | Parameters     |
+-----------------+-----------------+-----------+----------------+
| 0x021f          | 0x1f            | BLE\_ERR\ | Unspecified    |
|                 |                 | _UNSPECIF | Error          |
|                 |                 | IED       |                |
+-----------------+-----------------+-----------+----------------+
| 0x0220          | 0x20            | BLE\_ERR\ | Unsupported    |
|                 |                 | _UNSUPP\_ | LMP Parameter  |
|                 |                 | LMP\_LL\_ | Value /        |
|                 |                 | PARM      | Unsupported LL |
|                 |                 |           | Parameter      |
|                 |                 |           | Value          |
+-----------------+-----------------+-----------+----------------+
| 0x0221          | 0x21            | BLE\_ERR\ | Role Change    |
|                 |                 | _NO\_ROLE | Not Allowed    |
|                 |                 | \_CHANGE  |                |
+-----------------+-----------------+-----------+----------------+
| 0x0222          | 0x22            | BLE\_ERR\ | LMP Response   |
|                 |                 | _LMP\_LL\ | Timeout / LL   |
|                 |                 | _RSP\_TMO | Response       |
|                 |                 |           | Timeout        |
+-----------------+-----------------+-----------+----------------+
| 0x0223          | 0x23            | BLE\_ERR\ | LMP Error      |
|                 |                 | _LMP\_COL | Transaction    |
|                 |                 | LISION    | Collision      |
+-----------------+-----------------+-----------+----------------+
| 0x0224          | 0x24            | BLE\_ERR\ | LMP PDU Not    |
|                 |                 | _LMP\_PDU | Allowed        |
+-----------------+-----------------+-----------+----------------+
| 0x0225          | 0x25            | BLE\_ERR\ | Encryption     |
|                 |                 | _ENCRYPTI | Mode Not       |
|                 |                 | ON\_MODE  | Acceptable     |
+-----------------+-----------------+-----------+----------------+
| 0x0226          | 0x26            | BLE\_ERR\ | Link Key       |
|                 |                 | _LINK\_KE | cannot be      |
|                 |                 | Y\_CHANGE | Changed        |
+-----------------+-----------------+-----------+----------------+
| 0x0227          | 0x27            | BLE\_ERR\ | Requested QoS  |
|                 |                 | _UNSUPP\_ | Not Supported  |
|                 |                 | QOS       |                |
+-----------------+-----------------+-----------+----------------+
| 0x0228          | 0x28            | BLE\_ERR\ | Instant Passed |
|                 |                 | _INSTANT\ |                |
|                 |                 | _PASSED   |                |
+-----------------+-----------------+-----------+----------------+
| 0x0229          | 0x29            | BLE\_ERR\ | Pairing With   |
|                 |                 | _UNIT\_KE | Unit Key Not   |
|                 |                 | Y\_PAIRIN | Supported      |
|                 |                 | G         |                |
+-----------------+-----------------+-----------+----------------+
| 0x022a          | 0x2a            | BLE\_ERR\ | Different      |
|                 |                 | _DIFF\_TR | Transaction    |
|                 |                 | ANS\_COLL | Collision      |
+-----------------+-----------------+-----------+----------------+
| 0x022c          | 0x2c            | BLE\_ERR\ | QoS            |
|                 |                 | _QOS\_PAR | Unacceptable   |
|                 |                 | M         | Parameter      |
+-----------------+-----------------+-----------+----------------+
| 0x022d          | 0x2d            | BLE\_ERR\ | QoS Rejected   |
|                 |                 | _QOS\_REJ |                |
|                 |                 | ECTED     |                |
+-----------------+-----------------+-----------+----------------+
| 0x022e          | 0x2e            | BLE\_ERR\ | Channel        |
|                 |                 | _CHAN\_CL | Classification |
|                 |                 | ASS       | Not Supported  |
+-----------------+-----------------+-----------+----------------+
| 0x022f          | 0x2f            | BLE\_ERR\ | Insufficient   |
|                 |                 | _INSUFFIC | Security       |
|                 |                 | IENT\_SEC |                |
+-----------------+-----------------+-----------+----------------+
| 0x0230          | 0x30            | BLE\_ERR\ | Parameter Out  |
|                 |                 | _PARM\_OU | Of Mandatory   |
|                 |                 | T\_OF\_RA | Range          |
|                 |                 | NGE       |                |
+-----------------+-----------------+-----------+----------------+
| 0x0232          | 0x32            | BLE\_ERR\ | Role Switch    |
|                 |                 | _PENDING\ | Pending        |
|                 |                 | _ROLE\_SW |                |
+-----------------+-----------------+-----------+----------------+
| 0x0234          | 0x34            | BLE\_ERR\ | Reserved Slot  |
|                 |                 | _RESERVED | Violation      |
|                 |                 | \_SLOT    |                |
+-----------------+-----------------+-----------+----------------+
| 0x0235          | 0x35            | BLE\_ERR\ | Role Switch    |
|                 |                 | _ROLE\_SW | Failed         |
|                 |                 | \_FAIL    |                |
+-----------------+-----------------+-----------+----------------+
| 0x0236          | 0x36            | BLE\_ERR\ | Extended       |
|                 |                 | _INQ\_RSP | Inquiry        |
|                 |                 | \_TOO\_BI | Response Too   |
|                 |                 | G         | Large          |
+-----------------+-----------------+-----------+----------------+
| 0x0237          | 0x37            | BLE\_ERR\ | Secure Simple  |
|                 |                 | _SEC\_SIM | Pairing Not    |
|                 |                 | PLE\_PAIR | Supported By   |
|                 |                 |           | Host           |
+-----------------+-----------------+-----------+----------------+
| 0x0238          | 0x38            | BLE\_ERR\ | Host Busy -    |
|                 |                 | _HOST\_BU | Pairing        |
|                 |                 | SY\_PAIR  |                |
+-----------------+-----------------+-----------+----------------+
| 0x0239          | 0x39            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_RE | Rejected due   |
|                 |                 | J\_CHANNE | to No Suitable |
|                 |                 | L         | Channel Found  |
+-----------------+-----------------+-----------+----------------+
| 0x023a          | 0x3a            | BLE\_ERR\ | Controller     |
|                 |                 | _CTLR\_BU | Busy           |
|                 |                 | SY        |                |
+-----------------+-----------------+-----------+----------------+
| 0x023b          | 0x3b            | BLE\_ERR\ | Unacceptable   |
|                 |                 | _CONN\_PA | Connection     |
|                 |                 | RMS       | Parameters     |
+-----------------+-----------------+-----------+----------------+
| 0x023c          | 0x3c            | BLE\_ERR\ | Directed       |
|                 |                 | _DIR\_ADV | Advertising    |
|                 |                 | \_TMO     | Timeout        |
+-----------------+-----------------+-----------+----------------+
| 0x023d          | 0x3d            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_TE | Terminated due |
|                 |                 | RM\_MIC   | to MIC Failure |
+-----------------+-----------------+-----------+----------------+
| 0x023e          | 0x3e            | BLE\_ERR\ | Connection     |
|                 |                 | _CONN\_ES | Failed to be   |
|                 |                 | TABLISHME | Established    |
|                 |                 | NT        |                |
+-----------------+-----------------+-----------+----------------+
| 0x023f          | 0x3f            | BLE\_ERR\ | MAC Connection |
|                 |                 | _MAC\_CON | Failed         |
|                 |                 | N\_FAIL   |                |
+-----------------+-----------------+-----------+----------------+
| 0x0240          | 0x40            | BLE\_ERR\ | Coarse Clock   |
|                 |                 | _COARSE\_ | Adjustment     |
|                 |                 | CLK\_ADJ  | Rejected but   |
|                 |                 |           | Will Try to    |
|                 |                 |           | Adjust Using   |
|                 |                 |           | Clock Dragging |
+-----------------+-----------------+-----------+----------------+

Return codes - L2CAP
^^^^^^^^^^^^^^^^^^^^

+-----------------+-----------------+-----------+----------------+
| NimBLE Value    | Formal Value    | Name      | Condition      |
+=================+=================+===========+================+
| 0x0300          | 0x00            | BLE\_L2CA | Invalid or     |
|                 |                 | P\_SIG\_E | unsupported    |
|                 |                 | RR\_CMD\_ | incoming L2CAP |
|                 |                 | NOT\_UNDE | sig command.   |
|                 |                 | RSTOOD    |                |
+-----------------+-----------------+-----------+----------------+
| 0x0301          | 0x01            | BLE\_L2CA | Incoming       |
|                 |                 | P\_SIG\_E | packet too     |
|                 |                 | RR\_MTU\_ | large.         |
|                 |                 | EXCEEDED  |                |
+-----------------+-----------------+-----------+----------------+
| 0x0302          | 0x02            | BLE\_L2CA | No channel     |
|                 |                 | P\_SIG\_E | with specified |
|                 |                 | RR\_INVAL | ID.            |
|                 |                 | ID\_CID   |                |
+-----------------+-----------------+-----------+----------------+

Return codes - Security manager (us)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

+-----------------+-----------------+-----------+----------------+
| NimBLE Value    | Formal Value    | Name      | Condition      |
+=================+=================+===========+================+
| 0x0401          | 0x01            | BLE\_SM\_ | The user input |
|                 |                 | ERR\_PASS | of passkey     |
|                 |                 | KEY       | failed, for    |
|                 |                 |           | example, the   |
|                 |                 |           | user cancelled |
|                 |                 |           | the operation. |
+-----------------+-----------------+-----------+----------------+
| 0x0402          | 0x02            | BLE\_SM\_ | The OOB data   |
|                 |                 | ERR\_OOB  | is not         |
|                 |                 |           | available.     |
+-----------------+-----------------+-----------+----------------+
| 0x0403          | 0x03            | BLE\_SM\_ | The pairing    |
|                 |                 | ERR\_AUTH | procedure      |
|                 |                 | REQ       | cannot be      |
|                 |                 |           | performed as   |
|                 |                 |           | authentication |
|                 |                 |           | requirements   |
|                 |                 |           | cannot be met  |
|                 |                 |           | due to IO      |
|                 |                 |           | capabilities   |
|                 |                 |           | of one or both |
|                 |                 |           | devices.       |
+-----------------+-----------------+-----------+----------------+
| 0x0404          | 0x04            | BLE\_SM\_ | The confirm    |
|                 |                 | ERR\_CONF | value does not |
|                 |                 | IRM\_MISM | match the      |
|                 |                 | ATCH      | calculated     |
|                 |                 |           | compare value. |
+-----------------+-----------------+-----------+----------------+
| 0x0405          | 0x05            | BLE\_SM\_ | Pairing is not |
|                 |                 | ERR\_PAIR | supported by   |
|                 |                 | \_NOT\_SU | the device.    |
|                 |                 | PP        |                |
+-----------------+-----------------+-----------+----------------+
| 0x0406          | 0x06            | BLE\_SM\_ | The resultant  |
|                 |                 | ERR\_ENC\ | encryption key |
|                 |                 | _KEY\_SZ  | size is        |
|                 |                 |           | insufficient   |
|                 |                 |           | for the        |
|                 |                 |           | security       |
|                 |                 |           | requirements   |
|                 |                 |           | of this        |
|                 |                 |           | device.        |
+-----------------+-----------------+-----------+----------------+
| 0x0407          | 0x07            | BLE\_SM\_ | The SMP        |
|                 |                 | ERR\_CMD\ | command        |
|                 |                 | _NOT\_SUP | received is    |
|                 |                 | P         | not supported  |
|                 |                 |           | on this        |
|                 |                 |           | device.        |
+-----------------+-----------------+-----------+----------------+
| 0x0408          | 0x08            | BLE\_SM\_ | Pairing failed |
|                 |                 | ERR\_UNSP | due to an      |
|                 |                 | ECIFIED   | unspecified    |
|                 |                 |           | reason.        |
+-----------------+-----------------+-----------+----------------+
| 0x0409          | 0x09            | BLE\_SM\_ | Pairing or     |
|                 |                 | ERR\_REPE | authentication |
|                 |                 | ATED      | procedure is   |
|                 |                 |           | disallowed     |
|                 |                 |           | because too    |
|                 |                 |           | little time    |
|                 |                 |           | has elapsed    |
|                 |                 |           | since last     |
|                 |                 |           | pairing        |
|                 |                 |           | request or     |
|                 |                 |           | security       |
|                 |                 |           | request.       |
+-----------------+-----------------+-----------+----------------+
| 0x040a          | 0x0a            | BLE\_SM\_ | The Invalid    |
|                 |                 | ERR\_INVA | Parameters     |
|                 |                 | L         | error code     |
|                 |                 |           | indicates that |
|                 |                 |           | the command    |
|                 |                 |           | length is      |
|                 |                 |           | invalid or     |
|                 |                 |           | that a         |
|                 |                 |           | parameter is   |
|                 |                 |           | outside of the |
|                 |                 |           | specified      |
|                 |                 |           | range.         |
+-----------------+-----------------+-----------+----------------+
| 0x040b          | 0x0b            | BLE\_SM\_ | Indicates to   |
|                 |                 | ERR\_DHKE | the remote     |
|                 |                 | Y         | device that    |
|                 |                 |           | the DHKey      |
|                 |                 |           | Check value    |
|                 |                 |           | received       |
|                 |                 |           | doesn’t match  |
|                 |                 |           | the one        |
|                 |                 |           | calculated by  |
|                 |                 |           | the local      |
|                 |                 |           | device.        |
+-----------------+-----------------+-----------+----------------+
| 0x040c          | 0x0c            | BLE\_SM\_ | Indicates that |
|                 |                 | ERR\_NUMC | the confirm    |
|                 |                 | MP        | values in the  |
|                 |                 |           | numeric        |
|                 |                 |           | comparison     |
|                 |                 |           | protocol do    |
|                 |                 |           | not match.     |
+-----------------+-----------------+-----------+----------------+
| 0x040d          | 0x0d            | BLE\_SM\_ | Indicates that |
|                 |                 | ERR\_ALRE | the pairing    |
|                 |                 | ADY       | over the LE    |
|                 |                 |           | transport      |
|                 |                 |           | failed due to  |
|                 |                 |           | a Pairing      |
|                 |                 |           | Request sent   |
|                 |                 |           | over the       |
|                 |                 |           | BR/EDR         |
|                 |                 |           | transport in   |
|                 |                 |           | process.       |
+-----------------+-----------------+-----------+----------------+
| 0x040e          | 0x0e            | BLE\_SM\_ | Indicates that |
|                 |                 | ERR\_CROS | the BR/EDR     |
|                 |                 | S\_TRANS  | Link Key       |
|                 |                 |           | generated on   |
|                 |                 |           | the BR/EDR     |
|                 |                 |           | transport      |
|                 |                 |           | cannot be used |
|                 |                 |           | to derive and  |
|                 |                 |           | distribute     |
|                 |                 |           | keys for the   |
|                 |                 |           | LE transport.  |
+-----------------+-----------------+-----------+----------------+

Return codes - Security manager (peer)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

+-----------------+-----------------+-----------+----------------+
| NimBLE Value    | Formal Value    | Name      | Condition      |
+=================+=================+===========+================+
| 0x0501          | 0x01            | BLE\_SM\_ | The user input |
|                 |                 | ERR\_PASS | of passkey     |
|                 |                 | KEY       | failed, for    |
|                 |                 |           | example, the   |
|                 |                 |           | user cancelled |
|                 |                 |           | the operation. |
+-----------------+-----------------+-----------+----------------+
| 0x0502          | 0x02            | BLE\_SM\_ | The OOB data   |
|                 |                 | ERR\_OOB  | is not         |
|                 |                 |           | available.     |
+-----------------+-----------------+-----------+----------------+
| 0x0503          | 0x03            | BLE\_SM\_ | The pairing    |
|                 |                 | ERR\_AUTH | procedure      |
|                 |                 | REQ       | cannot be      |
|                 |                 |           | performed as   |
|                 |                 |           | authentication |
|                 |                 |           | requirements   |
|                 |                 |           | cannot be met  |
|                 |                 |           | due to IO      |
|                 |                 |           | capabilities   |
|                 |                 |           | of one or both |
|                 |                 |           | devices.       |
+-----------------+-----------------+-----------+----------------+
| 0x0504          | 0x04            | BLE\_SM\_ | The confirm    |
|                 |                 | ERR\_CONF | value does not |
|                 |                 | IRM\_MISM | match the      |
|                 |                 | ATCH      | calculated     |
|                 |                 |           | compare value. |
+-----------------+-----------------+-----------+----------------+
| 0x0505          | 0x05            | BLE\_SM\_ | Pairing is not |
|                 |                 | ERR\_PAIR | supported by   |
|                 |                 | \_NOT\_SU | the device.    |
|                 |                 | PP        |                |
+-----------------+-----------------+-----------+----------------+
| 0x0506          | 0x06            | BLE\_SM\_ | The resultant  |
|                 |                 | ERR\_ENC\ | encryption key |
|                 |                 | _KEY\_SZ  | size is        |
|                 |                 |           | insufficient   |
|                 |                 |           | for the        |
|                 |                 |           | security       |
|                 |                 |           | requirements   |
|                 |                 |           | of this        |
|                 |                 |           | device.        |
+-----------------+-----------------+-----------+----------------+
| 0x0507          | 0x07            | BLE\_SM\_ | The SMP        |
|                 |                 | ERR\_CMD\ | command        |
|                 |                 | _NOT\_SUP | received is    |
|                 |                 | P         | not supported  |
|                 |                 |           | on this        |
|                 |                 |           | device.        |
+-----------------+-----------------+-----------+----------------+
| 0x0508          | 0x08            | BLE\_SM\_ | Pairing failed |
|                 |                 | ERR\_UNSP | due to an      |
|                 |                 | ECIFIED   | unspecified    |
|                 |                 |           | reason.        |
+-----------------+-----------------+-----------+----------------+
| 0x0509          | 0x09            | BLE\_SM\_ | Pairing or     |
|                 |                 | ERR\_REPE | authentication |
|                 |                 | ATED      | procedure is   |
|                 |                 |           | disallowed     |
|                 |                 |           | because too    |
|                 |                 |           | little time    |
|                 |                 |           | has elapsed    |
|                 |                 |           | since last     |
|                 |                 |           | pairing        |
|                 |                 |           | request or     |
|                 |                 |           | security       |
|                 |                 |           | request.       |
+-----------------+-----------------+-----------+----------------+
| 0x050a          | 0x0a            | BLE\_SM\_ | The Invalid    |
|                 |                 | ERR\_INVA | Parameters     |
|                 |                 | L         | error code     |
|                 |                 |           | indicates that |
|                 |                 |           | the command    |
|                 |                 |           | length is      |
|                 |                 |           | invalid or     |
|                 |                 |           | that a         |
|                 |                 |           | parameter is   |
|                 |                 |           | outside of the |
|                 |                 |           | specified      |
|                 |                 |           | range.         |
+-----------------+-----------------+-----------+----------------+
| 0x050b          | 0x0b            | BLE\_SM\_ | Indicates to   |
|                 |                 | ERR\_DHKE | the remote     |
|                 |                 | Y         | device that    |
|                 |                 |           | the DHKey      |
|                 |                 |           | Check value    |
|                 |                 |           | received       |
|                 |                 |           | doesn’t match  |
|                 |                 |           | the one        |
|                 |                 |           | calculated by  |
|                 |                 |           | the local      |
|                 |                 |           | device.        |
+-----------------+-----------------+-----------+----------------+
| 0x050c          | 0x0c            | BLE\_SM\_ | Indicates that |
|                 |                 | ERR\_NUMC | the confirm    |
|                 |                 | MP        | values in the  |
|                 |                 |           | numeric        |
|                 |                 |           | comparison     |
|                 |                 |           | protocol do    |
|                 |                 |           | not match.     |
+-----------------+-----------------+-----------+----------------+
| 0x050d          | 0x0d            | BLE\_SM\_ | Indicates that |
|                 |                 | ERR\_ALRE | the pairing    |
|                 |                 | ADY       | over the LE    |
|                 |                 |           | transport      |
|                 |                 |           | failed due to  |
|                 |                 |           | a Pairing      |
|                 |                 |           | Request sent   |
|                 |                 |           | over the       |
|                 |                 |           | BR/EDR         |
|                 |                 |           | transport in   |
|                 |                 |           | process.       |
+-----------------+-----------------+-----------+----------------+
| 0x050e          | 0x0e            | BLE\_SM\_ | Indicates that |
|                 |                 | ERR\_CROS | the BR/EDR     |
|                 |                 | S\_TRANS  | Link Key       |
|                 |                 |           | generated on   |
|                 |                 |           | the BR/EDR     |
|                 |                 |           | transport      |
|                 |                 |           | cannot be used |
|                 |                 |           | to derive and  |
|                 |                 |           | distribute     |
|                 |                 |           | keys for the   |
|                 |                 |           | LE transport.  |
+-----------------+-----------------+-----------+----------------+
