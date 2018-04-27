
Callbacks
~~~c
void gnss_callback(int type, void *data) {
    switch(type) {
    case GNSS_EVENT_NMEA: {
	struct gnss_nmea_message *msg = (struct gnss_nmea_message *)data;
	gnss_nmea_log(msg);
	break;
    }
    }
}

void gnss_error_callback(gnss_t *ctx, int error) {
     switch(error) {
     case GNSS_ERROR_WRONG_BAUD_RATE:
         // Low driver uart has been closed (os_dev_close)
	 //  by the gnss_uart_rx_char helper
	 // HERE: Need to change baud rate and call os_dev_open
         break;
     }
}
~~~


Initialisation
~~~c
    gnss_t xa1110;
       
    struct gnss_uart gnss_uart = {
	.device         = "uart1",
    };
    struct gnss_nmea gnss_nmea = {
    };
    struct gnss_mediatek gnss_mediatek = {
	.wakeup_pin     = -1,
	.reset_pin      = -1,
	.cmd_delay      = 10,
    };

    gnss_init(&xa1110, gnss_callback, gnss_error_callback);
    gnss_uart_init(&xa1110, &gnss_uart);	  /* Transport: UART     */
    gnss_nmea_init(&xa1110, &gnss_nmea);	  /* Protocol : NMEA     */
    gnss_mediatek_init(&xa1110, &gnss_mediatek);  /* Driver   : MediaTek */
    gnss_start_rx(&xa1110);
~~~


Sending command:
~~~c
    gnss_nmea_send_cmd(&xa1110, "PMTK285,4,100");      // PPS Always
~~~