#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nrf.h"
#include "nrf_drv_saadc.h"
#include "nrfx_saadc.h"
#include "nrf_drv_ppi.h"
#include "nrf_drv_timer.h"
#include "boards.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "app_util_platform.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_drv_rtc.h"

#include "main.h"
#include "ble_gattc.h"

#define SAADC_CALIBRATION_INTERVAL 5              //Determines how often the SAADC should be calibrated relative to NRF_DRV_SAADC_EVT_DONE event. E.g. value 5 will make the SAADC calibrate every fifth time the NRF_DRV_SAADC_EVT_DONE is received.
#define SAADC_SAMPLES_IN_BUFFER 3                 //Number of SAADC samples in RAM before returning a SAADC event. For low power SAADC set this constant to 1. Otherwise the EasyDMA will be enabled for an extended time which consumes high current.

/**
#define RTC_FREQUENCY 32                          //Determines the RTC frequency and prescaler
#define RTC_CC_VALUE 8                            //Determines the RTC interrupt frequency and thereby the SAADC sampling frequency
*/

extern const nrf_drv_timer_t m_timer;
extern nrf_saadc_value_t     m_buffer_pool[2][SAADC_SAMPLES_IN_BUFFER];
extern nrf_ppi_channel_t     m_ppi_channel;
extern uint32_t              m_adc_evt_counter;
extern volatile bool ble_tx_in_flight;

//static volatile bool is_ready = true;
extern bool m_saadc_calib;
extern float bit8_map_arr[3];

long fastMap(long x, long in_min, long in_max, long out_min, long out_max);
void timer_handler(nrf_timer_event_t event_type, void * p_context);
void saadc_sampling_event_init(void);
void saadc_sampling_event_enable(void);
void saadc_callback(nrf_drv_saadc_evt_t const * p_event);
void saadc_init(void);

/**
void rtc_handler(nrf_drv_rtc_int_type_t int_type);
void lfclk_config(void);
void rtc_config(void);
*/