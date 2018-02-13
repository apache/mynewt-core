/**
 *
 * @defgroup nrfx_spis_config SPIS peripheral driver configuration
 * @{
 * @ingroup nrfx_spis
 */
/** @brief 
 *
 *  Set to 1 to activate.
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_ENABLED
/** @brief Enable SPIS0 instance
 *
 *  Set to 1 to activate.
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS0_ENABLED

/** @brief Enable SPIS1 instance
 *
 *  Set to 1 to activate.
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS1_ENABLED

/** @brief Enable SPIS2 instance
 *
 *  Set to 1 to activate.
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS2_ENABLED

/** @brief Interrupt priority
 *
 *  Following options are available:
 * - 0 - 0 (highest)
 * - 1 - 1
 * - 2 - 2
 * - 3 - 3
 * - 4 - 4 (Software Component only)
 * - 5 - 5 (Software Component only)
 * - 6 - 6 (Software Component only)
 * - 7 - 7 (Software Component only)
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_DEFAULT_CONFIG_IRQ_PRIORITY

/** @brief Mode
 *
 *  Following options are available:
 * - 0 - MODE_0
 * - 1 - MODE_1
 * - 2 - MODE_2
 * - 3 - MODE_3
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_DEFAULT_MODE

/** @brief SPIS default bit order
 *
 *  Following options are available:
 * - 0 - MSB first
 * - 1 - LSB first
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_DEFAULT_BIT_ORDER

/** @brief SPIS default DEF character
 *
 *  Minimum value: 0
 *  Maximum value: 255
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_DEFAULT_DEF

/** @brief SPIS default ORC character
 *
 *  Minimum value: 0
 *  Maximum value: 255
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_DEFAULT_ORC

/** @brief Enables logging in the module.
 *
 *  Set to 1 to activate.
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_CONFIG_LOG_ENABLED
/** @brief Default Severity level
 *
 *  Following options are available:
 * - 0 - Off
 * - 1 - Error
 * - 2 - Warning
 * - 3 - Info
 * - 4 - Debug
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_CONFIG_LOG_LEVEL

/** @brief ANSI escape code prefix.
 *
 *  Following options are available:
 * - 0 - Default
 * - 1 - Black
 * - 2 - Red
 * - 3 - Green
 * - 4 - Yellow
 * - 5 - Blue
 * - 6 - Magenta
 * - 7 - Cyan
 * - 8 - White
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_CONFIG_INFO_COLOR

/** @brief ANSI escape code prefix.
 *
 *  Following options are available:
 * - 0 - Default
 * - 1 - Black
 * - 2 - Red
 * - 3 - Green
 * - 4 - Yellow
 * - 5 - Blue
 * - 6 - Magenta
 * - 7 - Cyan
 * - 8 - White
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_CONFIG_DEBUG_COLOR


/** @brief Enables nRF52 Anomaly 109 workaround for SPIS.
 *
 * The workaround uses a GPIOTE channel to generate interrupts
 * on falling edges detected on the CSN line. This will make
 * the CPU active for the moment when SPIS starts DMA transfers,
 * and this way the transfers will be protected.
 * This workaround uses GPIOTE driver, so this driver must be
 * enabled as well.
 *
 *  Set to 1 to activate.
 *
 * @note This is an NRF_CONFIG macro.
 */
#define NRFX_SPIS_NRF52_ANOMALY_109_WORKAROUND_ENABLED


/** @} */
