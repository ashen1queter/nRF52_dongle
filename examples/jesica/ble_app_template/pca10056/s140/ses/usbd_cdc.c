/**
 * Copyright (c) 2017 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "nrf.h"
#include "nrf_drv_usbd.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_power.h"

#include "app_error.h"
#include "app_util.h"
#include "app_usbd_core.h"
#include "app_usbd.h"
#include "app_usbd_string_desc.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"

#include "boards.h"
#include "bsp.h"
#include "keyboard.h"
#include "app_timer.h"
#include "ble_cus.h"
#include "main.h"

#define SLEEP_TIMEOUT_MS 300000

#define LED_USB_RESUME      (BSP_BOARD_LED_0)
#define LED_CDC_ACM_OPEN    (BSP_BOARD_LED_1)
#define LED_CDC_ACM_RX      (BSP_BOARD_LED_2)
#define LED_CDC_ACM_TX      (BSP_BOARD_LED_3)

#define READ_SIZE 8

#define BTN_CDC_DATA_KEY_RELEASE        (bsp_event_t)(BSP_EVENT_KEY_LAST + 1)


#define CDC_ACM_COMM_INTERFACE  0
#define CDC_ACM_COMM_EPIN       NRF_DRV_USBD_EPIN2

#define CDC_ACM_DATA_INTERFACE  1
#define CDC_ACM_DATA_EPIN       NRF_DRV_USBD_EPIN1
#define CDC_ACM_DATA_EPOUT      NRF_DRV_USBD_EPOUT1

static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                    app_usbd_cdc_acm_user_event_t event);

/**
 * @brief CDC_ACM class instance
 * */
APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
                            cdc_acm_user_ev_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_NONE
);
APP_TIMER_DEF(m_sleep_timer);


static char m_rx_buffer[READ_SIZE];
static char m_tx_buffer[64];


typedef enum {
    CDC_STATE_INIT,
    CDC_STATE_WAIT_W,
    CDC_STATE_WAIT_A,
    CDC_STATE_WAIT_S,
    CDC_STATE_WAIT_D,
    CDC_STATE_WAIT_SOCD,
    CDC_STATE_WAIT_RAPID,
    CDC_STATE_DONE
} cdc_input_state_t;


static cdc_input_state_t cdc_state = CDC_STATE_INIT;


static void sleep_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    size_t size = 0;

    size = snprintf(m_tx_buffer, sizeof(m_tx_buffer), "Going to sleep...\r\n");
    ret_code_t ret = app_usbd_cdc_acm_write(&m_app_cdc_acm, m_tx_buffer, size);
    UNUSED_VARIABLE(ret);
    
    sleep_notify(1);
    
    sd_power_system_off();
}

static void timer_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}
  

static void cdc_prompt_next(void)
{
    size_t size = 0;
    ret_code_t ret;

    switch (cdc_state) {
        case CDC_STATE_INIT:
            size = snprintf(m_tx_buffer, sizeof(m_tx_buffer), "Device connected\r\nEnter actuation for W:\r\n");
            cdc_state = CDC_STATE_WAIT_W;
            break;
        case CDC_STATE_WAIT_W:
            size = snprintf(m_tx_buffer, sizeof(m_tx_buffer), "Enter actuation for A:\r\n");
            cdc_state = CDC_STATE_WAIT_A;
            break;
        case CDC_STATE_WAIT_A:
            size = snprintf(m_tx_buffer, sizeof(m_tx_buffer), "Enter actuation for S:\r\n");
            cdc_state = CDC_STATE_WAIT_S;
            break;
        case CDC_STATE_WAIT_S:
            size = snprintf(m_tx_buffer, sizeof(m_tx_buffer), "Enter actuation for D:\r\n");
            cdc_state = CDC_STATE_WAIT_D;
            break;
        case CDC_STATE_WAIT_D:
            size = snprintf(m_tx_buffer, sizeof(m_tx_buffer), "Enable SOCD? (y/n):\r\n");
            cdc_state = CDC_STATE_WAIT_SOCD;
            break;
        case CDC_STATE_WAIT_SOCD:
            size = snprintf(m_tx_buffer, sizeof(m_tx_buffer), "Enable Rapid Trigger? (y/n):\r\n");
            cdc_state = CDC_STATE_WAIT_RAPID;
            break;
        case CDC_STATE_WAIT_RAPID:
            size = snprintf(m_tx_buffer, sizeof(m_tx_buffer), "Setup complete.\r\n");
            cdc_state = CDC_STATE_DONE;
            break;
        case CDC_STATE_DONE:
            return;
    }

    ret = app_usbd_cdc_acm_write(&m_app_cdc_acm, m_tx_buffer, size);
    UNUSED_VARIABLE(ret);

    ret = app_timer_start(m_sleep_timer, APP_TIMER_TICKS(SLEEP_TIMEOUT_MS), sleep_timeout_handler);
    APP_ERROR_CHECK(ret);

}


static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                    app_usbd_cdc_acm_user_event_t event)
{
    ret_code_t ret;
    app_usbd_cdc_acm_t const * p_cdc_acm = app_usbd_cdc_acm_class_get(p_inst);

    switch (event)
    {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
            bsp_board_led_on(LED_CDC_ACM_OPEN);
            cdc_state = CDC_STATE_INIT;
            cdc_prompt_next();
            break;

        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            bsp_board_led_off(LED_CDC_ACM_OPEN);
            break;

        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            break;

        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
        {
            app_timer_stop(m_sleep_timer);
            app_timer_start(m_sleep_timer, APP_TIMER_TICKS(SLEEP_TIMEOUT_MS), sleep_timeout_handler);
            do {
                size_t size = app_usbd_cdc_acm_rx_size(p_cdc_acm);
                ret = app_usbd_cdc_acm_read(&m_app_cdc_acm, m_rx_buffer, READ_SIZE);
            } while (ret == NRF_SUCCESS);

            switch (cdc_state) {
                case CDC_STATE_WAIT_W:
                    keyboard_user_config.rapid_trigger_offset[0] = m_rx_buffer[0];
                    keyboard_user_config.trigger_offset[0] = m_rx_buffer[0];
                    break;
                case CDC_STATE_WAIT_A:
                     keyboard_user_config.rapid_trigger_offset[1] = m_rx_buffer[0];
                    keyboard_user_config.trigger_offset[1] = m_rx_buffer[0];
                    break;
                case CDC_STATE_WAIT_S:
                    keyboard_user_config.rapid_trigger_offset[2] = m_rx_buffer[0];
                    keyboard_user_config.trigger_offset[2] = m_rx_buffer[0];
                    break;
                case CDC_STATE_WAIT_D:
                    keyboard_user_config.rapid_trigger_offset[3] = m_rx_buffer[0];
                    keyboard_user_config.trigger_offset[3] = m_rx_buffer[0];
                    break;
                case CDC_STATE_WAIT_RAPID:
                    if (m_rx_buffer[0] == 'y' || m_rx_buffer[0] == 'Y')
                    {
                      memcpy(keyboard_user_config.rapid_trigger_offset,
                      keyboard_user_config.trigger_offset,
                      sizeof(keyboard_user_config.trigger_offset));

                      memset(keyboard_user_config.trigger_offset, 0,
                      sizeof(keyboard_user_config.trigger_offset));
                    }
                    break;

                case CDC_STATE_WAIT_SOCD:
                if (m_rx_buffer[0] == 'y' || m_rx_buffer[0] == 'Y'){
                    keyboard_user_config.SOCD = true;
                    }
                    break;
                default:
                    break;
            }

            cdc_prompt_next();
            break;
        }

        default:
            break;
    }
}


static void usbd_user_ev_handler(app_usbd_event_type_t event)
{
    switch (event)
    {
        case APP_USBD_EVT_DRV_SUSPEND:
            bsp_board_led_off(LED_USB_RESUME);
            break;
        case APP_USBD_EVT_DRV_RESUME:
            bsp_board_led_on(LED_USB_RESUME);
            break;
        case APP_USBD_EVT_STARTED:
            break;
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            bsp_board_leds_off();
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            //NRF_LOG_INFO("USB power detected");

            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            //NRF_LOG_INFO("USB power removed");
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            //NRF_LOG_INFO("USB ready");
            app_usbd_start();
            break;
        default:
            break;
    }
}



void cdc_init(void)
{
    ret_code_t ret;
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);
    
    nrf_drv_clock_lfclk_request(NULL);

    while(!nrf_drv_clock_lfclk_is_running())
    {
        /* Just waiting */
    }

    app_usbd_serial_num_generate();

    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);
    //NRF_LOG_INFO("USBD CDC ACM example started.");

    app_usbd_class_inst_t const * class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

    if (USBD_POWER_DETECTION)
    {
        ret = app_usbd_power_events_enable();
        APP_ERROR_CHECK(ret);
    }
    else
    {
        //NRF_LOG_INFO("No USB power detection enabled\r\nStarting USB now");

        app_usbd_enable();
        app_usbd_start();
    }
        
}


/**

#include <stdlib.h>

#include "zboss_api.h"
#include "sdk_config.h"

#include "app_usbd.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"
//#include "app_usbd_string_desc.h"

#include "boards.h"
#include "serial_handler.h"

 * @brief Enable power USB detection
 *
 * Configure if example supports USB port connection
#ifndef USBD_POWER_DETECTION
#define USBD_POWER_DETECTION true
#endif

static zb_uint8_t tx = 0;

static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst, app_usbd_cdc_acm_user_event_t event);
static void usbd_user_ev_handler(app_usbd_event_type_t event);

#define CDC_ACM_COMM_INTERFACE  0
#define CDC_ACM_COMM_EPIN       NRF_DRV_USBD_EPIN2

#define CDC_ACM_DATA_INTERFACE  1
#define CDC_ACM_DATA_EPIN       NRF_DRV_USBD_EPIN1
#define CDC_ACM_DATA_EPOUT      NRF_DRV_USBD_EPOUT1


 * @brief CDC_ACM class instance
 *
APP_USBD_CDC_ACM_GLOBAL_DEF(serial_uart,
                            cdc_acm_user_ev_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            //APP_USBD_CDC_COMM_PROTOCOL_AT_V250
                            APP_USBD_CDC_COMM_PROTOCOL_NONE
);

void serial_reset()
{
    serial_command_hdr_t rts;
    rts.size    = 0xC0C0;
    rts.command = SERIAL_CMD_INVALID;
    app_usbd_cdc_acm_write(&serial_uart, &rts, sizeof(rts) );
}

void serial_init()
{
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    ret_code_t ret;

    app_usbd_serial_num_generate();

    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);

    app_usbd_class_inst_t const * class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&serial_uart);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

    EVT_LOG("USBD CDC ACM example started.\n", NULL);
    if (USBD_POWER_DETECTION)
    {
        ret = app_usbd_power_events_enable();
        APP_ERROR_CHECK(ret);
    }
    else
    {
        EVT_LOG("No USB power detection enabled\nStarting USB now\n", NULL);

        app_usbd_enable();
        app_usbd_start();
    }
}

struct _tx_item
{
    void* p;
    size_t s;
};

#define TX_MAX 32

static int _tx_pos  = 0;
static int _tx_next = 0;
static struct _tx_item tx_queue[ TX_MAX ];

static void __send_it(void* hdr, size_t hdr_size, void* buf, size_t blen )
{
    zb_uint8_t* t = (zb_uint8_t*)malloc(hdr_size + blen );
    memcpy( t, hdr, hdr_size);
    if( blen > 0 && buf != NULL )
    {
        memcpy( &t[hdr_size], buf, blen );
    }

    if (tx_queue[_tx_next].p != NULL) 
    {
        // OOM.....
        PRINT("tx_queue is full !!... dropping packet! %d %d\n",  _tx_pos, _tx_next);
        free(t);
        return;
    }
    tx_queue[_tx_next].p = t;
    tx_queue[_tx_next].s = hdr_size + blen;    

    // No on going item send now!
    if( _tx_pos == _tx_next )
    {    
        if( app_usbd_cdc_acm_write(&serial_uart, t, hdr_size + blen ) == NRF_ERROR_INVALID_STATE )
        {           
            // No serial connected, just drop packet!
            tx_queue[_tx_next].p = NULL;
            tx_queue[_tx_next].s = 0;
            free(t);
            return;
        }
    }
    ++_tx_next;
    if( _tx_next >= TX_MAX )
    {
        _tx_next = 0;
    }
}


static cb_command_handler      command_handler     = NULL;
static cb_port_status_handler  port_status_handler = NULL;

void serial_set_command_handler(cb_command_handler cb)
{
    command_handler = cb;
}

void serial_set_port_status_handler(cb_port_status_handler cb)
{
    port_status_handler = cb;
}


static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst, app_usbd_cdc_acm_user_event_t event)
{
    app_usbd_cdc_acm_t const * p_cdc_acm = app_usbd_cdc_acm_class_get(p_inst);

    static char tmp[1024];
    static uint16_t tmp_i     = 0;
    static uint16_t size      = 0;

    switch (event)
    {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
        {        
            tmp_i = 2;
            size  = 0;
            tx = 0;
            memset(tmp,0,sizeof(tmp));
            Setup first transfer
            ret_code_t ret = app_usbd_cdc_acm_read(&serial_uart,
                                                   tmp,
                                                   sizeof(size));
        
            UNUSED_VARIABLE(ret);
            bsp_board_led_on(BSP_BOARD_LED_0);
            if( port_status_handler != NULL ) 
            {
                port_status_handler( true );
            }
            break;
        }
        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            bsp_board_led_off(BSP_BOARD_LED_0);
            if( port_status_handler != NULL ) 
            {
                port_status_handler( false );
            }
            break;
        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            free( tx_queue[_tx_pos].p );
            tx_queue[_tx_pos].p = NULL;
            tx_queue[_tx_pos].s = 0;

            // We should normaly only go once in this loop, but if there is an
            // error sending its most likly due to disconnected serial and we will
            // loop thrue and flush the queue.
            do
            {
                ++_tx_pos;
                if( _tx_pos >= TX_MAX )
                {
                    _tx_pos = 0;
                }
            
                if( _tx_pos != _tx_next )
                {
                    if( app_usbd_cdc_acm_write(&serial_uart, tx_queue[_tx_pos].p, tx_queue[_tx_pos].s ) == NRF_ERROR_INVALID_STATE )
                    {           
                        PRINT( "Drop from queue packet... %d %d\n", _tx_pos, _tx_next);
                        free(tx_queue[_tx_pos].p);
                        tx_queue[_tx_pos].p = NULL;
                        tx_queue[_tx_pos].s = 0;                        
                    }
                    else
                    {
                        break;
                    }
                 }
            }while( _tx_pos != _tx_next );

            break;
        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
        {           
            ret_code_t ret;   
            do
            { 
                //size_t xsize = app_usbd_cdc_acm_rx_size(p_cdc_acm);
                //EVT_LOG("Bytes waiting: %zd %hu:%hu\n", xsize, tmp_i, size );
                if( size == 0 )
                {
                    memcpy(&size, tmp, sizeof(size));
                    ret = app_usbd_cdc_acm_read_any(&serial_uart, &tmp[tmp_i], 1);
                    continue;
                }
                if (tmp_i < size) 
                {
                    ++tmp_i;
                }
                if (size == tmp_i) 
                {
                    if (command_handler != NULL) 
                    {
                        command_handler(tmp);
                    }

                    tmp_i = 2;
                    size  = 0;
                    memset(tmp,0,sizeof(tmp));
                    app_usbd_cdc_acm_read(&serial_uart, tmp, sizeof(size));
                    break;
                }
                ret = app_usbd_cdc_acm_read_any(&serial_uart, &tmp[tmp_i], 1);
                if (ret != NRF_SUCCESS) 
                {
                    break;
                }
            } while (ret == NRF_SUCCESS);
            break;
        }
        default:
            PRINT("Unhandled usb event: %x\n",event);
            break;
    }
}

static void usbd_user_ev_handler(app_usbd_event_type_t event)
{
    switch (event)
    {
        case APP_USBD_EVT_DRV_SUSPEND:
            PRINT("USB_RESUME\n",NULL);
            break;
        case APP_USBD_EVT_DRV_RESUME:
            PRINT("USB_RESUME\n",NULL);
            break;
        case APP_USBD_EVT_STARTED:
            break;
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            bsp_board_leds_off();
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            PRINT("USB power detected\n",NULL);

            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            PRINT("USB power removed\n",NULL);
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            PRINT("USB ready\n",NULL);
            app_usbd_start();
            break;
        default:
            PRINT("USB unhandled event:%x\n", event);
            break;
    }
}

void serial_process()
{
    app_usbd_event_queue_process();
}
*/