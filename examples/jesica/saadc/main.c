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
#include "aadc.h"

/**
void rtc_handler(nrf_drv_rtc_int_type_t int_type)
{
    uint32_t err_code;
	
    if (int_type == NRF_DRV_RTC_INT_COMPARE0)
    {
        nrf_drv_saadc_sample();                                        //Trigger the SAADC SAMPLE task
			
        LEDS_INVERT(BSP_LED_0_MASK);                                   //Toggle LED1 to indicate SAADC sampling start
		
        err_code = nrf_drv_rtc_cc_set(&rtc, 0, rtc_ticks, true);       //Set RTC compare value. This needs to be done every time as the nrf_drv_rtc clears the compare register on every compare match
        APP_ERROR_CHECK(err_code);
        nrf_drv_rtc_counter_clear(&rtc);                               //Clear the RTC counter to start count from zero
    }
}

void lfclk_config(void)
{
    ret_code_t err_code = nrf_drv_clock_init();                        //Initialize the clock source specified in the nrf_drv_config.h file, i.e. the CLOCK_CONFIG_LF_SRC constant
    APP_ERROR_CHECK(err_code);
    nrf_drv_clock_lfclk_request(NULL);
}

void rtc_config(void)
{

    uint32_t err_code;

    //Initialize RTC instance
    nrf_drv_rtc_config_t rtc_config;
    rtc_config.prescaler = RTC_FREQ_TO_PRESCALER(RTC_FREQUENCY);
    err_code = nrf_drv_rtc_init(&rtc, &rtc_config, rtc_handler);                //Initialize the RTC with callback function rtc_handler. The rtc_handler must be implemented in this applicaiton. Passing NULL here for RTC configuration means that configuration will be taken from the sdk_config.h file.
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_rtc_cc_set(&rtc, 0, rtc_ticks, true);                    //Set RTC compare value to trigger interrupt. Configure the interrupt frequency by adjust RTC_CC_VALUE and RTC_FREQUENCY constant in top of main.c
    APP_ERROR_CHECK(err_code);

    //Power on RTC instance
    nrf_drv_rtc_enable(&rtc);                                                   //Enable RTC
}
*/

long fastMap(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}




void timer_handler(nrf_timer_event_t event_type, void * p_context)
{

}


void saadc_sampling_event_init(void)
{
    ret_code_t err_code;

    err_code = nrf_drv_ppi_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;
    timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_32;
    err_code = nrf_drv_timer_init(&m_timer, &timer_cfg, timer_handler);
    APP_ERROR_CHECK(err_code);

    /* setup m_timer for compare event every 1ms */
    uint32_t ticks = nrf_drv_timer_ms_to_ticks(&m_timer, 1);
    nrf_drv_timer_extended_compare(&m_timer,
                                   NRF_TIMER_CC_CHANNEL0,
                                   ticks,
                                   NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
                                   false);
    nrf_drv_timer_enable(&m_timer);

    uint32_t timer_compare_event_addr = nrf_drv_timer_compare_event_address_get(&m_timer,
                                                                                NRF_TIMER_CC_CHANNEL0);
    uint32_t saadc_sample_task_addr   = nrf_drv_saadc_sample_task_get();

    /* setup ppi channel so that timer compare event is triggering sample task in SAADC */
    err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_assign(m_ppi_channel,
                                          timer_compare_event_addr,
                                          saadc_sample_task_addr);
    APP_ERROR_CHECK(err_code);
}


void saadc_sampling_event_enable(void)
{
    ret_code_t err_code = nrf_drv_ppi_channel_enable(m_ppi_channel);

    APP_ERROR_CHECK(err_code);
}


void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    ret_code_t err_code;
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        if(m_adc_evt_counter % SAADC_CALIBRATION_INTERVAL == 0)
        {
            m_saadc_calib = true;
        }
        
        for (int i = 0; i < p_event->data.done.size; i++)
        {
            fastMap_arr[i] = fastMap(p_event->data.done.p_buffer[i], 0, 3756, 0, 10);
            /**@todo Write to the custom char on a custom service of nrf53840 Dongle
                      ble_gattc_write_params_t write_params = {
                      .write_op = BLE_GATT_OP_WRITE_CMD,  // Write Without Response
                      .flags    = 0,
                      .handle   = m_char_handle,       // 🔁 Replace with discovered char handle
                      .offset   = 0,
                      .len      = sizeof(fastMap_arr),
                      .p_value  = fastMap_arr
                      };

                    err_code = sd_ble_gattc_write(conn_handle, &write_params);
                    APP_ERROR_CHECK(err_code);
            */

           // NRF_LOG_INFO("%d\r\n", p_event->data.done.p_buffer[i]); 
        }  

        if(m_saadc_calib == false)
        {
            //Do this after write
            err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, SAADC_SAMPLES_IN_BUFFER);  //Set buffer so the SAADC can write to it again. 
            APP_ERROR_CHECK(err_code);
        }
        
        m_adc_evt_counter++;
    }
    else if (p_event->type == NRF_DRV_SAADC_EVT_CALIBRATEDONE)
    {
        err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[0], SAADC_SAMPLES_IN_BUFFER);             //Set buffer so the SAADC can write to it again. 
        APP_ERROR_CHECK(err_code);
        err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[1], SAADC_SAMPLES_IN_BUFFER);             //Need to setup both buffers, as they were both removed with the call to nrf_drv_saadc_abort before calibration.
        APP_ERROR_CHECK(err_code);
        /**@todo m_saadc_calib = false;
        */                                         
    }
}

void saadc_init(void)
{
    ret_code_t err_code;
    nrf_drv_saadc_config_t saadc_config;
    nrf_saadc_channel_config_t channel_config;
    nrf_saadc_channel_config_t channel_configs[3];
	
    //Configure SAADC
    saadc_config.low_power_mode = true;                                                   //Enable low power mode.
    saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;                                 //Set SAADC resolution to 12-bit. This will make the SAADC output values from 0 (when input voltage is 0V) to 2^12=4096 (when input voltage is 3.6V for channel gain setting of 1/6).
    saadc_config.oversample = NRF_SAADC_OVERSAMPLE_DISABLED; //NRF_SAADC_OVERSAMPLE_4X    Set oversample to 4x. This will make the SAADC output a single averaged value when the SAMPLE task is triggered 4 times.
    saadc_config.interrupt_priority = APP_IRQ_PRIORITY_LOW;                               //Set SAADC interrupt to low priority.
	
    //Initialize SAADC
    err_code = nrf_drv_saadc_init(&saadc_config, saadc_callback);                         //Initialize the SAADC with configuration and callback function. The application must then implement the saadc_callback function, which will be called when SAADC interrupt is triggered
    APP_ERROR_CHECK(err_code);
		
    //Configure SAADC channel
    channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL;                              //Set internal reference of fixed 0.6 volts
    channel_config.gain = NRF_SAADC_GAIN1_6;                                              //Set input gain to 1/6. The maximum SAADC input voltage is then 0.6V/(1/6)=3.6V. The single ended input range is then 0V-3.6V
    channel_config.acq_time = NRF_SAADC_ACQTIME_10US;                                     //Set acquisition time. Set low acquisition time to enable maximum sampling frequency of 200kHz. Set high acquisition time to allow maximum source resistance up to 800 kohm, see the SAADC electrical specification in the PS. 
    channel_config.mode = NRF_SAADC_MODE_SINGLE_ENDED;                                    //Set SAADC as single ended. This means it will only have the positive pin as input, and the negative pin is shorted to ground (0V) internally.
    if(1)
    {
        channel_config.burst = NRF_SAADC_BURST_ENABLED;                                   //Configure burst mode for channel 0. Burst is useful together with oversampling. When triggering the SAMPLE task in burst mode, the SAADC will sample "Oversample" number of times as fast as it can and then output a single averaged value to the RAM buffer. If burst mode is not enabled, the SAMPLE task needs to be triggered "Oversample" number of times to output a single averaged value to the RAM buffer.		
    }
    channel_config.pin_n = NRF_SAADC_INPUT_DISABLED;                                      //Since the SAADC is single ended, the negative pin is disabled. The negative pin is shorted to ground internally.
    channel_config.resistor_p = NRF_SAADC_RESISTOR_DISABLED;                              //Disable pullup resistor on the input pin
    channel_config.resistor_n = NRF_SAADC_RESISTOR_DISABLED;                              //Disable pulldown resistor on the input pin

    
    nrf_saadc_input_t inputs[3] = {
    NRF_SAADC_INPUT_AIN0,
    NRF_SAADC_INPUT_AIN1,
    NRF_SAADC_INPUT_AIN2
    };

    for (int i = 0; i < 3; i++)
    {
    channel_configs[i] = channel_config;
    channel_configs[i].pin_p = inputs[i];
    err_code = nrf_drv_saadc_channel_init(i, &channel_configs[i]);
    APP_ERROR_CHECK(err_code);
    }

    err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[0], SAADC_SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[1], SAADC_SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);

}

/**
static void power_init(void)
{
    ret_code_t err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}
*/


/**@brief Function for main application entry.
int main(void)
{
    NRF_POWER->DCDCEN = 1;

    power_init();
    
    lfclk_config();                                   //Configure low frequency 32kHz clock
    rtc_config();                                    //Configure RTC. The RTC will generate periodic interrupts. Requires 32kHz clock to operate.

    saadc_init();
    saadc_sampling_event_init();
    saadc_sampling_event_enable();

    while (1)
    {
        if(m_saadc_calib == true)
        {
            nrf_drv_saadc_abort();                                  // Abort all ongoing conversions. Calibration cannot be run if SAADC is busy
            while(nrf_drv_saadc_calibrate_offset() != NRF_SUCCESS); //Trigger calibration task
            @todo m_saadc_calib = false;\

        }
        nrf_pwr_mgmt_run();
    }

}
*/