#ifndef __HID_H
#define __HID_H

#include "keyboard.h"
#include <stdint.h>
#include "app_usbd_hid_kbd.h"
#include "app_usbd.h"
#include "app_usbd_core.h"
#include "nrf_drv_clock.h"
#include "app_error.h"
#include "nrf_pwr_mgmt.h"

void hid_press_key(struct key *key, uint8_t layer);
void hid_release_key(struct key *key, uint8_t layer);
void hid_init(void);

#endif /* __HID_H */
