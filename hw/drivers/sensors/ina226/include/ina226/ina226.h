#ifndef __INA226_H__
#define __INA226_H__

#include <stdint.h>
#include <os/mynewt.h>
#include <stats/stats.h>
#include <sensor/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INA226_CONFIGURATION_REG_ADDR       0x00
#define INA226_SHUNT_VOLTAGE_REG_ADDR       0x01
#define INA226_BUS_VOLTAGE_REG_ADDR         0x02
#define INA226_POWER_REG_ADDR               0x03
#define INA226_CURRENT_REG_ADDR             0x04
#define INA226_CALIBRATION_REG_ADDR         0x05
#define INA226_MASK_ENABLE_REG_ADDR         0x06
#define INA226_ALERT_LIMIT_REG_ADDR         0x07
#define INA226_MFG_ID_REG_ADDR              0xFE
#define INA226_DIE_ID_REG_ADDR              0xFF

#define INA226_CONF_OPER_MODE_Pos           0u
#define INA226_CONF_OPER_MODE_Msk           (7u << INA226_CONF_OPER_MODE_Pos)
#define INA226_CONF_VSHCT_Pos               3u
#define INA226_CONF_VSHCT_Msk               (7u << INA226_CONF_VSHCT_Pos)
#define INA226_CONF_VBUSCT_Pos              6u
#define INA226_CONF_VBUSCT_Msk              (7u << INA226_CONF_VBUSCT_Pos)
#define INA226_CONF_AVG_Pos                 9u
#define INA226_CONF_AVG_Msk                 (7u << INA226_CONF_AVG_Pos)
#define INA226_CONF_RST_Pos                 15u
#define INA226_CONF_RST_Msk                 (1u << INA226_CONF_RST_Pos)

#define INA226_MANUFACTURER_ID              0x5449

#define INA226_MASK_ENABLE_LEN              0x0001
#define INA226_MASK_ENABLE_APOL             0x0002
#define INA226_MASK_ENABLE_OVF              0x0004
#define INA226_MASK_ENABLE_CVRF             0x0008
#define INA226_MASK_ENABLE_AFF              0x0010
#define INA226_MASK_ENABLE_CNVR             0x0400
#define INA226_MASK_ENABLE_POL              0x0800
#define INA226_MASK_ENABLE_BUL              0x1000
#define INA226_MASK_ENABLE_BOL              0x2000
#define INA226_MASK_ENABLE_SUL              0x4000
#define INA226_MASK_ENABLE_SOL              0x8000

/* 2500 nV */
#define INA226_SHUNT_VOLTAGE_LSB            2500
/* 1250 uV */
#define INA226_BUS_VOLTAGE_LSB              1250

/* INA226 operating modes */
enum ina226_avg_mode {
    INA226_AVG_1,
    INA226_AVG_4,
    INA226_AVG_16,
    INA226_AVG_64,
    INA226_AVG_128,
    INA226_AVG_256,
    INA226_AVG_512,
    INA226_AVG_1024,
};

/* Bus/shunt voltage conversion time */
enum ina226_ct {
    INA226_CT_140_US,
    INA226_CT_204_US,
    INA226_CT_332_US,
    INA226_CT_588_US,
    INA226_CT_1100_US,
    INA226_CT_2116_US,
    INA226_CT_4156_US,
    INA226_CT_8244_US,
};

/* INA226 operating modes */
enum ina226_oper_mode {
    INA226_OPER_POWER_DOWN                  = 0,
    INA226_OPER_SHUNT_VOLTAGE_TRIGGERED     = 1,
    INA226_OPER_BUS_VOLTAGE_TRIGGERED       = 2,
    INA226_OPER_SHUNT_AND_BUS_TRIGGERED     = 3,
    INA226_OPER_CONTINUOUS_MODE             = 4,
    INA226_OPER_SHUNT_VOLTAGE_CONTINUOUS    = 5,
    INA226_OPER_BUS_VOLTAGE_CONTINUOUS      = 6,
    INA226_OPER_SHUNT_AND_BUS_CONTINUOUS    = 7,
};

struct ina226_hw_cfg {
    struct sensor_itf itf;
    /** Shunt resistance in mOhm. */
    uint32_t shunt_resistance;
    /** Interrupt pin number, -1 if not used. */
    int16_t interrupt_pin;
    /** Interrupt pin requires pull-up. Set to false if external pull up resistor is present. */
    bool interrupt_pullup;
};

struct ina226_cfg {
    /** VBus conversion time */
    enum ina226_ct vbusct;
    /** Shunt conversion time */
    enum ina226_ct vshct;
    /** Averaging mode */
    enum ina226_avg_mode avg_mode;
};

/* Define the stats section and records */
STATS_SECT_START(ina226_stat_section)
    STATS_SECT_ENTRY(read_count)
    STATS_SECT_ENTRY(write_count)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

struct ina226_dev {
    struct os_dev dev;
    struct sensor sensor;
    /* Hardware wiring config, (pin, shunt, i2c) */
    struct ina226_hw_cfg hw_cfg;

    uint16_t config_reg;
    uint16_t mask_enable_reg;
    STATS_SECT_DECL(ina226_stat_section) stats;

    uint32_t conversion_time;
    struct os_sem conversion_done;
};

/**
 * Initialize the ina226. this function is normally called by sysinit.
 *
 * @param ina226  Pointer to the ina226_dev device descriptor.
 * @param arg     Pointer to struct ina226_hw_cfg.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_init(struct os_dev *ina226, void *arg);

/**
 * Reset sensor.
 *
 * @param ina226    Pointer to the ina226 device.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_reset(struct ina226_dev *ina226);

/**
 * Sets operating mode configuration.
 *
 * @param  ina226      Pointer to the ina226 device.
 * @param  ina226_cfg  Pointer to the ina226 configuration.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_config(struct ina226_dev *ina226, const struct ina226_cfg *cfg);

/**
 * Reads shunt voltage.
 *
 * @param ina226    Pointer to ina226 device.
 * @param voltage   Pointer to shunt voltage in nV.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_read_shunt_voltage(struct ina226_dev *ina226, int32_t *voltage);

/**
 * Reads bus voltage.
 *
 * @param ina226    Pointer to ina226 device.
 * @param voltage   Pointer to output voltage in uV.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_read_bus_voltage(struct ina226_dev *ina226, uint32_t *voltage);

/**
 * Reads shunt voltage and returns current.
 *
 * @param ina226    Pointer to ina226 device.
 * @param voltage   Pointer to output current in uA.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_read_current(struct ina226_dev *ina226, int32_t *current);

/**
 * Reads manufactured and die id.
 *
 * @param ina226    Pointer to ina226 device.
 * @param mfg       Pointer to manufacturer id.
 * @param die       Pointer to die id.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_read_id(struct ina226_dev *ina226, uint16_t *mfg, uint16_t *die);

/**
 * Reads current, shunt and bus voltage.
 *
 * @param ina226    Pointer to ina226 device.
 * @param current   Pointer to output current in uA (can be NULL).
 * @param vbus      Pointer to bus voltage in uV (can be NULL).
 * @param vshunt    Pointer to shunt voltage in nV (can be NULL).
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_one_shot_read(struct ina226_dev *ina226, int32_t *current, uint32_t *vbus, int32_t *vshunt);

/**
 * Waits for interrupt to fire and read values.
 * Intended to be used with continuous read mode.
 *
 * @param ina226    Pointer to ina226 device.
 * @param current   Pointer to output current in uA (can be NULL).
 * @param vbus      Pointer to bus voltage in uV (can be NULL).
 * @param vshunt    Pointer to shunt voltage in nV (can be NULL).
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_wait_and_read(struct ina226_dev *ina226, int32_t *current, uint32_t *vbus, int32_t *vshunt);

/**
 * Start continuous read mode.
 *
 * @param ina226    Pointer to ina226 device.
 * @param mode      shunt and/or vbus mode.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_start_continuous_mode(struct ina226_dev *ina226, enum ina226_oper_mode mode);

/**
 * Stop continuous read mode.
 *
 * @param ina226    Pointer to ina226 device.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_stop_continuous_mode(struct ina226_dev *ina226);

#if MYNEWT_VAL(INA226_CLI)
/**
 * Initialize the INA226 shell extensions.
 *
 * @return 0 on success, non-zero on failure.
 */
int ina226_shell_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __INA226_H__ */

