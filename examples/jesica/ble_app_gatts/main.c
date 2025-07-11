#include "nordic_common.h"
#include "nrf_sdm.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "app_util.h"
#include "app_timer.h"
#include "bsp_btn_ble.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "fds.h"
#include "nrf_fstorage.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_scan.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "aadc.h"
#include "main.h"


#define APP_BLE_CONN_CFG_TAG      1                                /**< A tag identifying the SoftDevice BLE configuration. */
#define APP_BLE_OBSERVER_PRIO     3                                /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_SOC_OBSERVER_PRIO     1                                /**< Applications' SoC observer priority. You shouldn't need to modify this value. */

#define SEC_PARAM_BOND            1                                /**< Perform bonding. */
#define SEC_PARAM_MITM            0                                /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC            0                                /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS        0                                /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_NONE             /**< No I/O capabilities. */
#define SEC_PARAM_OOB             0                                /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE    7                                /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE    16                               /**< Maximum encryption key size. */

#define SCAN_DURATION_WITELIST    3000                             /**< Duration of the scanning in units of 10 milliseconds. */

#define TARGET_UUID               BLE_UUID_GATT                    /**< Target device name that application is looking for. */


/**@brief Variable length data encapsulation in terms of length and pointer to data */
typedef struct
{
    uint8_t * p_data;   /**< Pointer to data. */
    uint16_t  data_len; /**< Length of data. */
}data_t;

NRF_BLE_GATT_DEF(m_gatt);                                 /**< GATT module instance. */
NRF_BLE_SCAN_DEF(m_scan);                                 /**< Scanning Module instance. */
  
static uint16_t              m_ser_handle;                /**< Characteristic handle. */
static bool                  m_whitelist_disabled;        /**< True if whitelist has been temporarily disabled. */
static bool                  m_memory_access_in_progress; /**< Flag to keep track of ongoing operations on persistent memory. */
static bool                  m_erase_bonds;               /**< Bool to determine if bonds should be erased before scanning starts. Based on button push upon startup. */
static bool                  device_connected = false;
static bool                  sampling_started = false;
/**< Scan parameters requested for scanning and connection. */
static ble_gap_scan_params_t const m_scan_param =
{
    .active        = 0x01,
    .interval      = NRF_BLE_SCAN_SCAN_INTERVAL,
    .window        = NRF_BLE_SCAN_SCAN_WINDOW,
    .filter_policy = BLE_GAP_SCAN_FP_WHITELIST,
    .timeout       = SCAN_DURATION_WITELIST,
    .scan_phys     = BLE_GAP_PHY_1MBPS,
};

static void scan_start(void);

static char const m_target_periph_name[] = "Nordic_GATTS"; /**< If you want to connect to a peripheral using a given advertising name, type its name here. */
static bool       is_connect_per_addr    = false;          /**< If you want to connect to a peripheral with a given address, set this to true and put the correct address in the variable below. */

static ble_gap_addr_t const m_target_periph_addr =
{
    /* Possible values for addr_type:
       BLE_GAP_ADDR_TYPE_PUBLIC,
       BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
       BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE,
       BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE. */
      .addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
      .addr      = {0x8D, 0xFE, 0x23, 0x86, 0x77, 0xD9}
};

/**@brief Function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num     Line number of the failing ASSERT call.
 * @param[in] p_file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(0xDEADBEEF, line_num, p_file_name);
}


/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{
    pm_handler_on_pm_evt(p_evt);
    pm_handler_disconnect_on_sec_failure(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        /**case PM_EVT_PEERS_DELETE_SUCCEEDED:
            scan_start();
            break;
          */

        default:
            break;
    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ret_code_t            err_code;
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    pm_handler_secure_on_connection(p_ble_evt);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
        {
            ble_gattc_evt_prim_srvc_disc_rsp_t const * p_response =
                &p_ble_evt->evt.gattc_evt.params.prim_srvc_disc_rsp;

            if (p_response->count > 0)
            {
                ble_gattc_service_t const * p_service = &p_response->services[0];

                uint16_t start_handle = p_service->handle_range.start_handle;
                uint16_t end_handle   = p_service->handle_range.end_handle;

                ble_gattc_handle_range_t handle_range = {
                    .start_handle = start_handle,
                    .end_handle = end_handle
                };

                sd_ble_gattc_characteristics_discover(p_ble_evt->evt.gattc_evt.conn_handle, &handle_range);
            }
        } 
        break;

        case BLE_GATTC_EVT_CHAR_DISC_RSP:
        {
            ble_gattc_evt_char_disc_rsp_t const * rsp =
                &p_ble_evt->evt.gattc_evt.params.char_disc_rsp;

            for (uint8_t i = 0; i < rsp->count; i++)
            {
              ble_gattc_char_t characteristic = rsp->chars[i];

              if (characteristic.uuid.uuid == 1 && characteristic.uuid.type == 0) //1 and 0 are placeholders
              {
                  m_char_handle = characteristic.handle_value;
              }
            }
        } 
        break;

        case BLE_GAP_EVT_CONNECTED:
        {
            m_conn_handle = p_gap_evt->conn_handle;

            if (ble_conn_state_central_conn_count() < NRF_SDH_BLE_CENTRAL_LINK_COUNT)
            {
                scan_start();
            }

            device_connected = true;
            sampling_started = false;
            
            sd_ble_gattc_primary_services_discover(m_conn_handle, 0x0001, NULL);

        } break;

        case BLE_GAP_EVT_TIMEOUT:
        {
            if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN)
            {
                NRF_LOG_DEBUG("Connection Request timed out.");
            }
        } break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            // Accepting parameters requested by peer.
            err_code = sd_ble_gap_conn_param_update(p_gap_evt->conn_handle,
                                                    &p_gap_evt->params.conn_param_update_request.conn_params);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_DISCONNECTED:
        {
            NRF_LOG_INFO("Disconnected. conn_handle: 0x%x, reason: 0x%x",
                         p_gap_evt->conn_handle,
                         p_gap_evt->params.disconnected.reason);

            if (ble_conn_state_central_conn_count() < NRF_SDH_BLE_CENTRAL_LINK_COUNT)
            {
                scan_start();
            }

            device_connected = false;

        } break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTC_EVT_TIMEOUT:
        {
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTS_EVT_TIMEOUT:
        {
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
        } break;
        
        case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE:
        {
          ble_tx_in_flight = false;
        }

            
        default:
            break;
    }
}


/**
 * @brief SoftDevice SoC event handler.
 *
 * @param[in] evt_id    SoC event.
 * @param[in] p_context Context.
 */
static void soc_evt_handler(uint32_t evt_id, void * p_context)
{
    switch (evt_id)
    {
        case NRF_EVT_FLASH_OPERATION_SUCCESS:
        /* fall through */
        case NRF_EVT_FLASH_OPERATION_ERROR:

            if (m_memory_access_in_progress)
            {
                m_memory_access_in_progress = false;
                scan_start();
            }
            break;

        /**@todo When using cdc
        case NRF_EVT_POWER_USB_POWER_READY:
        break;
        */

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
  */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);
    
    // Register handlers for BLE and SoC events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
    NRF_SDH_SOC_OBSERVER(m_soc_observer, APP_SOC_OBSERVER_PRIO, soc_evt_handler, NULL);
}


/**@brief Function for disabling the use of whitelist for scanning.
 */
static void whitelist_disable(void)
{
    if (!m_whitelist_disabled)
    {
        NRF_LOG_INFO("Whitelist temporarily disabled.");
        m_whitelist_disabled = true;
        nrf_ble_scan_stop();
        scan_start();
    }
}


/**@brief Function for the Peer Manager initialization.
 *
 * @param[in] erase_bonds  Indicates whether bonding information should be cleared from
 *                         persistent storage during initialization of the Peer Manager.
 */
static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Clear bond information from persistent storage.
 */
static void delete_bonds(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Erase bonds.");
    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);
}


/**@brief Retrive a list of peer manager peer IDs.
 *
 * @param[inout] p_peers   The buffer where to store the list of peer IDs.
 * @param[inout] p_size    In: The size of the @p p_peers buffer.
 *                         Out: The number of peers copied in the buffer.
 */
static void peer_list_get(pm_peer_id_t * p_peers, uint32_t * p_size)
{
    pm_peer_id_t peer_id;
    uint32_t     peers_to_copy;

    peers_to_copy = (*p_size < BLE_GAP_WHITELIST_ADDR_MAX_COUNT) ?
                     *p_size : BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

    peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
    *p_size = 0;

    while ((peer_id != PM_PEER_ID_INVALID) && (peers_to_copy--))
    {
        p_peers[(*p_size)++] = peer_id;
        peer_id              = pm_next_peer_id_get(peer_id);
    }
}


static void whitelist_load(void)
{
    ret_code_t   ret;
    pm_peer_id_t peers[5]; //Max 8 cuz whitelist max is 8
    uint32_t     peer_cnt;

    memset(peers, PM_PEER_ID_INVALID, sizeof(peers));
    peer_cnt = (sizeof(peers) / sizeof(pm_peer_id_t));

    // Load all peers from flash and whitelist them.
    peer_list_get(peers, &peer_cnt);

    ret = pm_whitelist_set(peers, peer_cnt);
    APP_ERROR_CHECK(ret);

    // Setup the device identities list.
    // Some SoftDevices do not support this feature.
    ret = pm_device_identities_list_set(peers, peer_cnt);
    if (ret != NRF_ERROR_NOT_SUPPORTED)
    {
        APP_ERROR_CHECK(ret);
    }
}


static void on_whitelist_req(void)
{
    // Whitelist buffers.
    ble_gap_addr_t whitelist_addrs[8];
    ble_gap_irk_t  whitelist_irks[8];

    memset(whitelist_addrs, 0x00, sizeof(whitelist_addrs));
    memset(whitelist_irks, 0x00, sizeof(whitelist_irks));

    uint32_t addr_cnt = (sizeof(whitelist_addrs) / sizeof(ble_gap_addr_t));
    uint32_t irk_cnt  = (sizeof(whitelist_irks) / sizeof(ble_gap_irk_t));

    // Reload the whitelist and whitelist all peers.
    whitelist_load();

    ret_code_t err_code;

    // Get the whitelist previously set using pm_whitelist_set().
    err_code = pm_whitelist_get(whitelist_addrs, &addr_cnt,
                                whitelist_irks, &irk_cnt);
    APP_ERROR_CHECK(err_code);

    if (((addr_cnt == 0) && (irk_cnt == 0)) ||
        (m_whitelist_disabled))
    {
        // Don't use whitelist.
        err_code = nrf_ble_scan_params_set(&m_scan, NULL);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function to start scanning.
 */
static void scan_start(void)
{
    ret_code_t err_code;

    // If there is any pending write to flash, defer scanning until it completes.
    if (nrf_fstorage_is_busy(NULL))
    {
        m_memory_access_in_progress = true;
        return;
    }

    err_code = nrf_ble_scan_params_set(&m_scan, &m_scan_param);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_scan_start(&m_scan);
    APP_ERROR_CHECK(err_code);

}


/**@brief Function for initializing the GATT module. */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);

    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


static void scan_evt_handler(scan_evt_t const * p_scan_evt)
{
    ret_code_t err_code;
    switch(p_scan_evt->scan_evt_id)
    {
        case NRF_BLE_SCAN_EVT_WHITELIST_REQUEST:
        {
            on_whitelist_req();
            m_whitelist_disabled = false;
        } break;

        case NRF_BLE_SCAN_EVT_CONNECTING_ERROR:
        {
            err_code = p_scan_evt->params.connecting_err.err_code;
            APP_ERROR_CHECK(err_code);
        } break;

        case NRF_BLE_SCAN_EVT_SCAN_TIMEOUT:
        {
            scan_start();
        } break;

        case NRF_BLE_SCAN_EVT_FILTER_MATCH:
            break;
        case NRF_BLE_SCAN_EVT_WHITELIST_ADV_REPORT:
            break;

        default:
          break;
    }
}


/**@brief Function for initializing scanning.
 */
static void scan_init(void)
{
    ret_code_t err_code;
    nrf_ble_scan_init_t init_scan;

    memset(&init_scan, 0, sizeof(init_scan));

    init_scan.p_scan_param     = &m_scan_param;
    init_scan.connect_if_match = true;
    init_scan.conn_cfg_tag     = APP_BLE_CONN_CFG_TAG;

    err_code = nrf_ble_scan_init(&m_scan, &init_scan, scan_evt_handler);
    APP_ERROR_CHECK(err_code);
}


/**@ Function for settings scan filters.
 */
static void scan_filters_set(void)
{
    ret_code_t err_code;
    ble_uuid_t target_uuid = {.uuid = TARGET_UUID, .type = BLE_UUID_TYPE_BLE};

    err_code = nrf_ble_scan_filter_set(&m_scan, SCAN_NAME_FILTER, m_target_periph_name);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_scan_filter_set(&m_scan, SCAN_UUID_FILTER, &target_uuid);
    APP_ERROR_CHECK(err_code);

    if (is_connect_per_addr)
    {
        err_code = nrf_ble_scan_filter_set(&m_scan, SCAN_ADDR_FILTER, m_target_periph_addr.addr);
        APP_ERROR_CHECK(err_code);
        err_code = nrf_ble_scan_filters_enable(&m_scan,
                       NRF_BLE_SCAN_NAME_FILTER | NRF_BLE_SCAN_UUID_FILTER | NRF_BLE_SCAN_ADDR_FILTER,
                       false);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        err_code = nrf_ble_scan_filters_enable(&m_scan,
                       NRF_BLE_SCAN_NAME_FILTER | NRF_BLE_SCAN_UUID_FILTER,
                       false);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for starting a scan, or instead trigger it from peer manager (after
          deleting bonds).

   @param[in] p_erase_bonds Pointer to a bool to determine if bonds will be deleted before scanning.
*/
void scanning_start(bool * p_erase_bonds)
{
    // Start scanning for peripherals and initiate connection
    // with devices that advertise GATT Service UUID.
    if (*p_erase_bonds == true)
    {
        // Scan is started by the PM_EVT_PEERS_DELETE_SUCCEEDED event.
        delete_bonds();
    }
    else
    {
        scan_start();
    }
}

/**@brief Function for initializing all the modules used in this example application.
 */
static void modules_init(void)
{
    // Initialize.
    power_management_init();
    ble_stack_init();
    gatt_init();
    peer_manager_init();
    scan_init();
    scan_filters_set();
}

int main(void)
{
    NRF_POWER->DCDCEN = 1;
    // Initialize.
    modules_init();
    whitelist_load();

    // Start execution.
    scanning_start(&m_erase_bonds);

    // Enter main loop.
    while (1)
    {
        if(device_connected && !sampling_started) //Once device is conencted then start boundary definition.
        {
            saadc_init();
            sampling_started = true;
        }
        
        if(device_connected && m_saadc_calib)
        {
            nrf_drv_saadc_abort();                                  // Abort all ongoing conversions. Calibration cannot be run if SAADC is busy
            while(nrf_drv_saadc_calibrate_offset() != NRF_SUCCESS); //Trigger calibration task
            /**@todo m_saadc_calib = false;
                 */
        }
        
        if (device_connected && sampling_started && !ble_tx_in_flight && !m_saadc_calib)
        {
            nrf_drv_saadc_sample();  
            ble_tx_in_flight = true;
        }
      nrf_pwr_mgmt_run();
    }
}