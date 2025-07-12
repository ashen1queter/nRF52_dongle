#include "hid.h"
#include "keyboard.h"
#include <stdlib.h>
#include "usbd_cdc.h"


extern struct user_config keyboard_user_config;

static uint8_t keycodes[6] = {0};


APP_USBD_HID_KBD_GLOBAL_DEF(m_keeb, 0, NRF_DRV_USBD_EPIN1, NULL, APP_USBD_HID_SUBCLASS_BOOT);

void hid_init(void)
{
    ret_code_t err_code;

    err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_clock_lfclk_request(NULL);

    
    while(!nrf_drv_clock_lfclk_is_running())
    {
        /* Just waiting */
    }

    
    err_code = app_usbd_init(NULL);
    APP_ERROR_CHECK(err_code);

    
    app_usbd_class_inst_t const * class_inst_kbd = app_usbd_hid_kbd_class_inst_get(&m_keeb);
    err_code = app_usbd_class_append(class_inst_kbd);
    APP_ERROR_CHECK(err_code);

    
    if (USBD_POWER_DETECTION)
    {
        err_code = app_usbd_power_events_enable();
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        //NRF_LOG_INFO("No USB power detection enabled\r\nStarting USB now");

        app_usbd_enable();
        app_usbd_start();
    }
}



void hid_press_key(struct key *key, uint8_t layer) {
  switch (key->layers[layer].type) {
  case KEY_TYPE_MODIFIER:
    //modifiers |= key->layers[layer].value;
    //should_send_keyboard_report = 1;
    break;

  case KEY_TYPE_NORMAL:
    for (uint8_t i = 0; i < 6; i++) {
      if (keycodes[i] == 0) {
        keycodes[i] = key->layers[layer].value;
        // if the key is violently pressed, automatically add the MAJ modifier :)
        // if (is_screaming) {
        //   is_screaming = 0;
        //   modifiers &= ~get_bitmask_for_modifier(HID_KEY_SHIFT_LEFT);
        // } else if (i == 0 && key->state.velocity > keyboard_user_config.screaming_velocity_trigger) {
        //   is_screaming = 1;
        //   modifiers |= get_bitmask_for_modifier(HID_KEY_SHIFT_LEFT);
        // }
        //should_send_keyboard_report = 1;
        app_usbd_hid_kbd_key_control(&m_keeb, keycodes[i], true);
        break;
      }
    }
    break;

  case KEY_TYPE_CONSUMER_CONTROL:
    //consumer_report = key->layers[layer].value;
    //should_send_consumer_report = 1;
    break;

  default:
    break;
  }
}

void hid_release_key(struct key *key, uint8_t layer) {
  switch (key->layers[layer].type) {
  case KEY_TYPE_MODIFIER:
    //modifiers &= ~key->layers[layer].value;
    //should_send_keyboard_report = 1;
    break;

  case KEY_TYPE_NORMAL:
    for (uint8_t i = 0; i < 6; i++) {
        // if the key is violently pressed, automatically add the MAJ modifier :)
        // if (is_screaming) {
        //   is_screaming = 0;
        //   modifiers &= ~get_bitmask_for_modifier(HID_KEY_SHIFT_LEFT);
        // } else if (i == 0 && key->state.velocity > keyboard_user_config.screaming_velocity_trigger) {
        //   is_screaming = 1;
        //   modifiers |= get_bitmask_for_modifier(HID_KEY_SHIFT_LEFT);
        // }
        //should_send_keyboard_report = 1;
        app_usbd_hid_kbd_key_control(&m_keeb, keycodes[i], false);
      }
    break;

  case KEY_TYPE_CONSUMER_CONTROL:
    //consumer_report = 0;
    //should_send_consumer_report = 1;
    break;

  default:
    break;
  }
}