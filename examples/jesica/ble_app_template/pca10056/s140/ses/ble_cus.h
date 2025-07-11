#ifndef BLE_CUS_H__
#define BLE_CUS_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_CUS_BLE_OBSERVER_PRIO 2
void ble_cus_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);

/**@brief   Macro for defining a ble_cus instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_CUS_DEF(_name)                                                                          \
static ble_cus_t _name;                                                                             \
NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                                 \
                     BLE_CUS_BLE_OBSERVER_PRIO,                                                     \
                     ble_cus_on_ble_evt, &_name)

#define CUS_UUID_BASE        {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, \
                              0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00}
#define CUS_UUID_SERVICE     0x1523
#define CUS_UUID_ADC_CHAR    0x1524

// Forward declaration of the ble_cus_t type.
typedef struct ble_cus_s ble_cus_t;

typedef void (*ble_cus_adc_write_handler_t) (const uint8_t *new_state);

/** @brief Custom Service init structure. This structure contains all options and data needed for
 *        initialization of the service.*/
typedef struct
{
    ble_cus_adc_write_handler_t adc_write_handler; /**< Event handler to be called when the ADC Characteristic is written. */
} ble_cus_init_t;

/**@brief Custom Service structure. This structure contains various status information for the service. */
struct ble_cus_s
{
    uint16_t                    service_handle;      /**< Handle of Custom Service. */
    ble_gatts_char_handles_t    adc_char_handles;    /**< Handles related to the ADC Characteristic. */
    uint8_t                     uuid_type;           /**< UUID type for the Custom Service. */
    ble_cus_adc_write_handler_t adc_write_handler;   /**< Event handler to be called when the ADC Characteristic is written. */
};


/**@brief Function for initializing the Custom Service.
 *
 * @param[out] p_cus      Custom Service structure. This structure must be supplied by
 *                        the application. It is initialized by this function and will later
 *                        be used to identify this particular service instance.
 * @param[in] p_cus_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was initialized successfully. Otherwise, an error code is returned.
 */
uint32_t ble_cus_init(ble_cus_t * p_cus, const ble_cus_init_t * p_cus_init);

#ifdef __cplusplus
}
#endif

#endif // BLE_CUS_H__

/** @} */