#include "sdk_common.h"
#include "ble_cus.h"
#include "ble_srv_common.h"
#include "app_error.h"
#include "main.h"


/**@brief Function for handling the Write event.
 *
 * @param[in] p_cus      Custom Service structure.
 * @param[in] p_ble_evt  Event received from the BLE stack.
 */
static void on_write(ble_cus_t * p_cus, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (   (p_evt_write->handle == p_cus->adc_char_handles.value_handle)
        && (p_evt_write->len == 3)
        && (p_cus->adc_write_handler != NULL))
    {
        p_cus->adc_write_handler(p_evt_write->data);

    }
}

void ble_cus_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_cus_t * p_cus = (ble_cus_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTS_EVT_WRITE:
            on_write(p_cus, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


uint32_t ble_cus_init(ble_cus_t * p_cus, const ble_cus_init_t * p_cus_init)
{
    uint32_t              err_code;
    ble_uuid_t            ble_uuid;
    ble_add_char_params_t add_char_params;

    // Initialize service structure.
    p_cus->adc_write_handler = p_cus_init->adc_write_handler;

    // Add service.
    ble_uuid128_t base_uuid = {CUS_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_cus->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_cus->uuid_type;
    ble_uuid.uuid = CUS_UUID_SERVICE;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_cus->service_handle);
    VERIFY_SUCCESS(err_code);

    // Add ADC characteristic.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = CUS_UUID_ADC_CHAR;
    add_char_params.uuid_type         = p_cus->uuid_type;
    add_char_params.init_len          = sizeof(uint8_t);
    add_char_params.max_len           = sizeof(uint8_t);
    add_char_params.char_props.read   = 1;
    add_char_params.char_props.notify = 1;

    add_char_params.read_access       = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;

    err_code = characteristic_add(p_cus->service_handle,
                                  &add_char_params,
                                  &p_cus->adc_char_handles);
    if(err_code != NRF_SUCCESS){
        return err_code;
    }

    // Add Sleep_Shut characteristic.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = CUS_UUID_SS_CHAR;
    add_char_params.uuid_type         = p_cus->uuid_type;
    add_char_params.init_len          = sizeof(uint8_t);
    add_char_params.max_len           = sizeof(uint8_t);
    add_char_params.char_props.read   = 1;
    add_char_params.char_props.notify = 1;

    add_char_params.read_access       = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;

    return characteristic_add(p_cus->service_handle,
                                  &add_char_params,
                                  &p_cus->ss_char_handles);
}


void sleep_notify(uint8_t sleep_signal)
{
    uint16_t len = sizeof(sleep_signal);

    ble_gatts_hvx_params_t hvx_params = {0};
    hvx_params.handle = m_cuss->ss_char_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_data = &sleep_signal;
    hvx_params.p_len  = &len;

    sd_ble_gatts_hvx(m_conn_handle, &hvx_params);
}
