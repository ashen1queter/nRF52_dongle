#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "nrf.h"
#include "app_util_platform.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_power.h"
#include "app_usbd.h"
#include "app_usbd_core.h"
#include "app_usbd_hid_kbd.h"
#include "boards.h"
#include "bsp.h"

/**
 * @brief Enable USB power detection
 */
#ifndef USBD_POWER_DETECTION
#define USBD_POWER_DETECTION true
#endif

static bool key_sent = false;

/**
 * @brief USBD library specific event handler.
 *
 * @param event     USBD library event.
 * */
static void usbd_user_ev_handler(app_usbd_event_type_t event)
{
    switch (event)
    {
        case APP_USBD_EVT_DRV_SOF:
            break;
        case APP_USBD_EVT_DRV_RESET:
            break;
        case APP_USBD_EVT_DRV_SUSPEND:
            app_usbd_suspend_req();
            bsp_board_led_off(BSP_LED_0);
            break;
        case APP_USBD_EVT_DRV_RESUME:
            bsp_board_led_on(BSP_LED_0);
            break;
        case APP_USBD_EVT_STARTED:
            bsp_board_led_on(BSP_LED_0);
            break;
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            bsp_board_led_off(BSP_LED_0);
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            app_usbd_start();
            break;
        default:
            break;
    }
}

APP_USBD_HID_KBD_GLOBAL_DEF(m_keeb, 0, NRF_DRV_USBD_EPIN2, NULL, APP_USBD_HID_SUBCLASS_BOOT);

void trigger_keycode_press(void)
{
    app_usbd_hid_kbd_key_control(&m_keeb, APP_USBD_HID_KBD_A, 1);
}

void trigger_keycode_release(void)
{
    app_usbd_hid_kbd_key_control(&m_keeb, APP_USBD_HID_KBD_A, 0);
}

void button_led_init(void)
{
    bsp_board_init(BSP_INIT_BUTTONS | BSP_INIT_LEDS);
}

bool button_is_pressed(void)
{
    return bsp_board_button_state_get(0);
}

int main(void)
{
    button_led_init();
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    ret_code_t err_code;
    err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    //nrf_drv_clock_lfclk_request(NULL);

    /**
    while(!nrf_drv_clock_lfclk_is_running())
    {
        Just waiting
    }
    */

    app_usbd_init(&usbd_config);
    //app_usbd_init(NULL);

    app_usbd_class_inst_t const * class_inst_kbd;
    class_inst_kbd = app_usbd_hid_kbd_class_inst_get(&m_keeb);

    err_code = app_usbd_class_append(class_inst_kbd);
    APP_ERROR_CHECK(err_code);

    if (USBD_POWER_DETECTION)
    {
        err_code = app_usbd_power_events_enable();
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        app_usbd_enable();
        app_usbd_start();
    }

    while (true)
    {
        while (app_usbd_event_queue_process());

        bool pressed = button_is_pressed();

        if (pressed && !key_sent)
        {
            trigger_keycode_press();
            key_sent = true;
        }
        else if (!pressed && key_sent)
        {
            trigger_keycode_release();
            key_sent = false;
        }

        __WFE();
    }
}