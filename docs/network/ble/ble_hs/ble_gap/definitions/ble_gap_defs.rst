GAP events
----------

.. code:: c

    typedef int ble_gap_event_fn(struct ble_gap_event *ctxt, void *arg);

.. code:: c

    #define BLE_GAP_EVENT_CONNECT               0
    #define BLE_GAP_EVENT_DISCONNECT            1
    #define BLE_GAP_EVENT_CONN_CANCEL           2
    #define BLE_GAP_EVENT_CONN_UPDATE           3
    #define BLE_GAP_EVENT_CONN_UPDATE_REQ       4
    #define BLE_GAP_EVENT_L2CAP_UPDATE_REQ      5
    #define BLE_GAP_EVENT_TERM_FAILURE          6
    #define BLE_GAP_EVENT_DISC                  7
    #define BLE_GAP_EVENT_DISC_COMPLETE         8
    #define BLE_GAP_EVENT_ADV_COMPLETE          9
    #define BLE_GAP_EVENT_ENC_CHANGE            10
    #define BLE_GAP_EVENT_PASSKEY_ACTION        11
    #define BLE_GAP_EVENT_NOTIFY_RX             12
    #define BLE_GAP_EVENT_NOTIFY_TX             13
    #define BLE_GAP_EVENT_SUBSCRIBE             14
    #define BLE_GAP_EVENT_MTU                   15
    #define BLE_GAP_EVENT_IDENTITY_RESOLVED     16
    #define BLE_GAP_EVENT_REPEAT_PAIRING        17

.. code:: c

    /**
     * Represents a GAP-related event.  When such an event occurs, the host
     * notifies the application by passing an instance of this structure to an
     * application-specified callback.
     */
    struct ble_gap_event {
        /**
         * Indicates the type of GAP event that occurred.  This is one of the
         * BLE_GAP_EVENT codes.
         */
        uint8_t type;

        /**
         * A discriminated union containing additional details concerning the GAP
         * event.  The 'type' field indicates which member of the union is valid.
         */
        union {
            /**
             * Represents a connection attempt.  Valid for the following event
             * types:
             *     o BLE_GAP_EVENT_CONNECT
             */
            struct {
                /**
                 * The status of the connection attempt;
                 *     o 0: the connection was successfully established.
                 *     o BLE host error code: the connection attempt failed for
                 *       the specified reason.
                 */
                int status;

                /** The handle of the relevant connection. */
                uint16_t conn_handle;
            } connect;

            /**
             * Represents a terminated connection.  Valid for the following event
             * types:
             *     o BLE_GAP_EVENT_DISCONNECT
             */
            struct {
                /**
                 * A BLE host return code indicating the reason for the
                 * disconnect.
                 */
                int reason;

                /** Information about the connection prior to termination. */
                struct ble_gap_conn_desc conn;
            } disconnect;

            /**
             * Represents an advertising report received during a discovery
             * procedure.  Valid for the following event types:
             *     o BLE_GAP_EVENT_DISC
             */
            struct ble_gap_disc_desc disc;

    #if MYNEWT_VAL(BLE_EXT_ADV)
            /**
             * Represents an extended advertising report received during a discovery
             * procedure.  Valid for the following event types:
             *     o BLE_GAP_EVENT_EXT_DISC
             */
            struct ble_gap_ext_disc_desc ext_disc;
    #endif
            /**
             * Represents an attempt to update a connection's parameters.  If the
             * attempt was successful, the connection's descriptor reflects the
             * updated parameters.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_CONN_UPDATE
             */
            struct {
                /**
                 * The result of the connection update attempt;
                 *     o 0: the connection was successfully updated.
                 *     o BLE host error code: the connection update attempt failed
                 *       for the specified reason.
                 */
                int status;

                /** The handle of the relevant connection. */
                uint16_t conn_handle;
            } conn_update;

            /**
             * Represents a peer's request to update the connection parameters.
             * This event is generated when a peer performs any of the following
             * procedures:
             *     o L2CAP Connection Parameter Update Procedure
             *     o Link-Layer Connection Parameters Request Procedure
             *
             * To reject the request, return a non-zero HCI error code.  The value
             * returned is the reject reason given to the controller.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_L2CAP_UPDATE_REQ
             *     o BLE_GAP_EVENT_CONN_UPDATE_REQ
             */
            struct {
                /**
                 * Indicates the connection parameters that the peer would like to
                 * use.
                 */
                const struct ble_gap_upd_params *peer_params;

                /**
                 * Indicates the connection parameters that the local device would
                 * like to use.  The application callback should fill this in.  By
                 * default, this struct contains the requested parameters (i.e.,
                 * it is a copy of 'peer_params').
                 */
                struct ble_gap_upd_params *self_params;

                /** The handle of the relevant connection. */
                uint16_t conn_handle;
            } conn_update_req;

            /**
             * Represents a failed attempt to terminate an established connection.
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_TERM_FAILURE
             */
            struct {
                /**
                 * A BLE host return code indicating the reason for the failure.
                 */
                int status;

                /** The handle of the relevant connection. */
                uint16_t conn_handle;
            } term_failure;

            /**
             * Represents an attempt to change the encrypted state of a
             * connection.  If the attempt was successful, the connection
             * descriptor reflects the updated encrypted state.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_ENC_CHANGE
             */
            struct {
                /**
                 * Indicates the result of the encryption state change attempt;
                 *     o 0: the encrypted state was successfully updated;
                 *     o BLE host error code: the encryption state change attempt
                 *       failed for the specified reason.
                 */
                int status;

                /** The handle of the relevant connection. */
                uint16_t conn_handle;
            } enc_change;

            /**
             * Represents a passkey query needed to complete a pairing procedure.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_PASSKEY_ACTION
             */
            struct {
                /** Contains details about the passkey query. */
                struct ble_gap_passkey_params params;

                /** The handle of the relevant connection. */
                uint16_t conn_handle;
            } passkey;

            /**
             * Represents a received ATT notification or indication.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_NOTIFY_RX
             */
            struct {
                /**
                 * The contents of the notification or indication.  If the
                 * application wishes to retain this mbuf for later use, it must
                 * set this pointer to NULL to prevent the stack from freeing it.
                 */
                struct os_mbuf *om;

                /** The handle of the relevant ATT attribute. */
                uint16_t attr_handle;

                /** The handle of the relevant connection. */
                uint16_t conn_handle;

                /**
                 * Whether the received command is a notification or an
                 * indication;
                 *     o 0: Notification;
                 *     o 1: Indication.
                 */
                uint8_t indication:1;
            } notify_rx;

            /**
             * Represents a transmitted ATT notification or indication, or a
             * completed indication transaction.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_NOTIFY_TX
             */
            struct {
                /**
                 * The status of the notification or indication transaction;
                 *     o 0:                 Command successfully sent;
                 *     o BLE_HS_EDONE:      Confirmation (indication ack) received;
                 *     o BLE_HS_ETIMEOUT:   Confirmation (indication ack) never
                 *                              received;
                 *     o Other return code: Error.
                 */
                int status;

                /** The handle of the relevant connection. */
                uint16_t conn_handle;

                /** The handle of the relevant characterstic value. */
                uint16_t attr_handle;

                /**
                 * Whether the transmitted command is a notification or an
                 * indication;
                 *     o 0: Notification;
                 *     o 1: Indication.
                 */
                uint8_t indication:1;
            } notify_tx;

            /**
             * Represents a state change in a peer's subscription status.  In this
             * comment, the term "update" is used to refer to either a notification
             * or an indication.  This event is triggered by any of the following
             * occurrences:
             *     o Peer enables or disables updates via a CCCD write.
             *     o Connection is about to be terminated and the peer is
             *       subscribed to updates.
             *     o Peer is now subscribed to updates after its state was restored
             *       from persistence.  This happens when bonding is restored.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_SUBSCRIBE
             */
            struct {
                /** The handle of the relevant connection. */
                uint16_t conn_handle;

                /** The value handle of the relevant characteristic. */
                uint16_t attr_handle;

                /** One of the BLE_GAP_SUBSCRIBE_REASON codes. */
                uint8_t reason;

                /** Whether the peer was previously subscribed to notifications. */
                uint8_t prev_notify:1;

                /** Whether the peer is currently subscribed to notifications. */
                uint8_t cur_notify:1;

                /** Whether the peer was previously subscribed to indications. */
                uint8_t prev_indicate:1;

                /** Whether the peer is currently subscribed to indications. */
                uint8_t cur_indicate:1;
            } subscribe;

            /**
             * Represents a change in an L2CAP channel's MTU.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_MTU
             */
            struct {
                /** The handle of the relevant connection. */
                uint16_t conn_handle;

                /**
                 * Indicates the channel whose MTU has been updated; either
                 * BLE_L2CAP_CID_ATT or the ID of a connection-oriented channel.
                 */
                uint16_t channel_id;

                /* The channel's new MTU. */
                uint16_t value;
            } mtu;

            /**
             * Represents a change in peer's identity. This is issued after
             * successful pairing when Identity Address Information was received.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_IDENTITY_RESOLVED
             */
            struct {
                /** The handle of the relevant connection. */
                uint16_t conn_handle;
            } identity_resolved;

            /**
             * Represents a peer's attempt to pair despite a bond already existing.
             * The application has two options for handling this event type:
             *     o Retry: Return BLE_GAP_REPEAT_PAIRING_RETRY after deleting the
             *              conflicting bond.  The stack will verify the bond has
             *              been deleted and continue the pairing procedure.  If
             *              the bond is still present, this event will be reported
             *              again.
             *     o Ignore: Return BLE_GAP_REPEAT_PAIRING_IGNORE.  The stack will
             *               silently ignore the pairing request.
             *
             * Valid for the following event types:
             *     o BLE_GAP_EVENT_REPEAT_PAIRING
             */
            struct ble_gap_repeat_pairing repeat_pairing;

            /**
             * Represents a change of PHY. This is issue after successful
             * change on PHY.
             */
            struct {
                int status;
                uint16_t conn_handle;

                /**
                 * Indicates enabled TX/RX PHY. Possible values:
                 *     o BLE_GAP_LE_PHY_1M
                 *     o BLE_GAP_LE_PHY_2M
                 *     o BLE_GAP_LE_PHY_CODED
                 */
                uint8_t tx_phy;
                uint8_t rx_phy;
            } phy_updated;
        };
    };

.. code:: c

    #define BLE_GAP_CONN_MODE_NON               0
    #define BLE_GAP_CONN_MODE_DIR               1
    #define BLE_GAP_CONN_MODE_UND               2

.. code:: c

    #define BLE_GAP_DISC_MODE_NON               0
    #define BLE_GAP_DISC_MODE_LTD               1
    #define BLE_GAP_DISC_MODE_GEN               2

.. code:: c

    /*** Reason codes for the subscribe GAP event. */

    /** Peer's CCCD subscription state changed due to a descriptor write. */
    #define BLE_GAP_SUBSCRIBE_REASON_WRITE      1

    /** Peer's CCCD subscription state cleared due to connection termination. */
    #define BLE_GAP_SUBSCRIBE_REASON_TERM       2

    /**
     * Peer's CCCD subscription state changed due to restore from persistence
     * (bonding restored).
     */
    #define BLE_GAP_SUBSCRIBE_REASON_RESTORE    3

.. code:: c

    struct ble_gap_sec_state {
        unsigned encrypted:1;
        unsigned authenticated:1;
        unsigned bonded:1;
        unsigned key_size:5;
    };

.. code:: c

    /**
     * conn_mode:                   One of the following constants:
     *                                  o BLE_GAP_CONN_MODE_NON
     *                                      (non-connectable; 3.C.9.3.2).
     *                                  o BLE_GAP_CONN_MODE_DIR
     *                                      (directed-connectable; 3.C.9.3.3).
     *                                  o BLE_GAP_CONN_MODE_UND
     *                                      (undirected-connectable; 3.C.9.3.4).
     * disc_mode:                   One of the following constants:
     *                                  o BLE_GAP_DISC_MODE_NON
     *                                      (non-discoverable; 3.C.9.2.2).
     *                                  o BLE_GAP_DISC_MODE_LTD
     *                                      (limited-discoverable; 3.C.9.2.3).
     *                                  o BLE_GAP_DISC_MODE_GEN
     *                                      (general-discoverable; 3.C.9.2.4).
     */
    struct ble_gap_adv_params {
        /*** Mandatory fields. */
        uint8_t conn_mode;
        uint8_t disc_mode;

        /*** Optional fields; assign 0 to make the stack calculate them. */
        uint16_t itvl_min;
        uint16_t itvl_max;
        uint8_t channel_map;
        uint8_t filter_policy;
        uint8_t high_duty_cycle:1;
    };

.. code:: c

    #define BLE_GAP_ROLE_MASTER                 0
    #define BLE_GAP_ROLE_SLAVE                  1

.. code:: c

    struct ble_gap_conn_desc {
        struct ble_gap_sec_state sec_state;
        ble_addr_t our_id_addr;
        ble_addr_t peer_id_addr;
        ble_addr_t our_ota_addr;
        ble_addr_t peer_ota_addr;
        uint16_t conn_handle;
        uint16_t conn_itvl;
        uint16_t conn_latency;
        uint16_t supervision_timeout;
        uint8_t role;
        uint8_t master_clock_accuracy;
    };

.. code:: c


    struct ble_gap_conn_params {
        uint16_t scan_itvl;
        uint16_t scan_window;
        uint16_t itvl_min;
        uint16_t itvl_max;
        uint16_t latency;
        uint16_t supervision_timeout;
        uint16_t min_ce_len;
        uint16_t max_ce_len;
    };

.. code:: c

    struct ble_gap_ext_disc_params {
        uint16_t itvl;
        uint16_t window;
        uint8_t passive:1;
    };

.. code:: c

    struct ble_gap_disc_params {
        uint16_t itvl;
        uint16_t window;
        uint8_t filter_policy;
        uint8_t limited:1;
        uint8_t passive:1;
        uint8_t filter_duplicates:1;
    };

.. code:: c

    struct ble_gap_upd_params {
        uint16_t itvl_min;
        uint16_t itvl_max;
        uint16_t latency;
        uint16_t supervision_timeout;
        uint16_t min_ce_len;
        uint16_t max_ce_len;
    };

.. code:: c

    struct ble_gap_passkey_params {
        uint8_t action;
        uint32_t numcmp;
    };

.. code:: c

    struct ble_gap_disc_desc {
        /*** Common fields. */
        uint8_t event_type;
        uint8_t length_data;
        ble_addr_t addr;
        int8_t rssi;
        uint8_t *data;

        /***
         * LE direct advertising report fields; direct_addr is BLE_ADDR_ANY if
         * direct address fields are not present.
         */
        ble_addr_t direct_addr;
    };

.. code:: c

    struct ble_gap_repeat_pairing {
        /** The handle of the relevant connection. */
        uint16_t conn_handle;

        /** Properties of the existing bond. */
        uint8_t cur_key_size;
        uint8_t cur_authenticated:1;
        uint8_t cur_sc:1;

        /**
         * Properties of the imminent secure link if the pairing procedure is
         * allowed to continue.
         */
        uint8_t new_key_size;
        uint8_t new_authenticated:1;
        uint8_t new_sc:1;
        uint8_t new_bonding:1;
    };

.. code:: c

    struct ble_gap_disc_desc {
        /*** Common fields. */
        uint8_t event_type;
        uint8_t length_data;
        ble_addr_t addr;
        int8_t rssi;
        uint8_t *data;

        /***
         * LE direct advertising report fields; direct_addr is BLE_ADDR_ANY if
         * direct address fields are not present.
         */
        ble_addr_t direct_addr;
    };

.. code:: c

    struct ble_gap_repeat_pairing {
        /** The handle of the relevant connection. */
        uint16_t conn_handle;

        /** Properties of the existing bond. */
        uint8_t cur_key_size;
        uint8_t cur_authenticated:1;
        uint8_t cur_sc:1;

        /**
         * Properties of the imminent secure link if the pairing procedure is
         * allowed to continue.
         */
        uint8_t new_key_size;
        uint8_t new_authenticated:1;
        uint8_t new_sc:1;
        uint8_t new_bonding:1;
    };
