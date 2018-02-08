// Move to charge_control_priv.h
struct charge_control_read_ev_ctx {
    /* The sensor for which the ev cb should be called */
    struct charge_control *ccrec_charge_control;
    /* The sensor type */
    charge_control_type_t ccrec_type;
};