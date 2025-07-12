#ifndef __MAIN_H
#define __MAIN_H

#include "stdint.h"
#include "ble_cus.h"

extern uint8_t adc_values[3];
extern uint8_t cus_actuation[3]; //If enter in CDC then use default
extern uint16_t m_conn_handle;
extern ble_cus_t *m_cuss;
extern uint8_t usbd_status;

#endif __MAIN_H