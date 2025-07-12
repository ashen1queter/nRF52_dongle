#include "cli_cdc.h"


static void app_cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst,
                                        app_usbd_cdc_acm_user_event_t event);

APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
    app_cdc_acm_user_ev_handler,
    CDC_ACM_COMM_INTERFACE,
    CDC_ACM_DATA_INTERFACE,
    CDC_ACM_COMM_EPIN,
    CDC_ACM_DATA_EPIN,
    CDC_ACM_DATA_EPOUT,
    APP_USBD_CDC_COMM_PROTOCOL_AT_V250);



static void app_cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst,
                                        app_usbd_cdc_acm_user_event_t event)
{
    switch (event)
    {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
            // Ready to receive CLI input
            break;
        default:
            break;
    }
}


void cli_init(void)
{
    ret_code_t err;

    // Init CLI over USB
    nrf_cli_init(&m_cli_usb, NULL, true, NRF_LOG_SEVERITY_ERROR);
    nrf_cli_start(&m_cli_usb);
}

static void cmd_set_actuation(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc != 3) {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Usage: set_act <key> <value>\n");
        return;
    }

    char key = argv[1][0];
    uint8_t value = atoi(argv[2]);

    uint8_t idx;
    switch (key) {
        case 'W': idx = 1; break;
        case 'A': idx = 0; break;
        case 'S': idx = 1; break; // Tap layer
        case 'D': idx = 2; break;
        default:
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Invalid key\n");
            return;
    }

    keyboard_user_config.trigger_offset[idx] = value;
    keyboard_keys[idx].actuation.trigger_offset = value;

    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Set %c actuation to %d\n", key, value);
}

static void cmd_socd(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc != 2) {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Usage: socd <0|1>\n");
        return;
    }

    bool enable = atoi(argv[1]);

    for (int i = 0; i < 3; i++) {
        if (keyboard_keys[i].socd_key_pair) {
            keyboard_keys[i].actuation.socd = enable ? SOCD_KEY : 0;
        }
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "SOCD trigger %s\n", enable ? "enabled" : "disabled");
}

static void cmd_rapid(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if (argc != 2) {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "Usage: rapid <0|1>\n");
        return;
    }

    bool enable = atoi(argv[1]);

    for (int i = 0; i < 3; i++) {
        keyboard_keys[i].actuation.rapid_trigger_offset = enable ? 20 : 0;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Rapid trigger %s\n", enable ? "enabled" : "disabled");
}


NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_cmds)
{
    NRF_CLI_CMD(set_act, NULL, "Set actuation offset: set_act <W|A|S|D> <val>", cmd_set_actuation),
    NRF_CLI_CMD(socd, NULL, "Enable/disable SOCD: socd <1|0>", cmd_socd),
    NRF_CLI_CMD(rapid, NULL, "Enable/disable rapid trigger: rapid <1|0>", cmd_rapid),
    NRF_CLI_SUBCMD_SET_END
};

NRF_CLI_CMD_REGISTER(keycfg, &m_cmds, "Keyboard config commands", NULL);
