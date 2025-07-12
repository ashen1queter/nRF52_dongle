#ifndef CLI_CDC_H__
#define CLI_CDC_H__

//#include "nrf_cli_usb.h"
#include "nrf_cli.h"
#include "app_usbd_cdc_acm.h"
#include "keyboard.h"
#include "app_usbd_cdc_types.h"


#ifdef __cplusplus
extern "C" {
#endif

//NRF_CLI_USB_DEF(m_cli_usb);
#define CDC_ACM_COMM_INTERFACE   0  // Communication Interface
#define CDC_ACM_DATA_INTERFACE   1  // Data Interface

#define CDC_ACM_COMM_EPIN        NRF_DRV_USBD_EPIN2  // Interrupt IN endpoint for notifications
#define CDC_ACM_DATA_EPIN        NRF_DRV_USBD_EPIN1  // Bulk IN endpoint (device → host)
#define CDC_ACM_DATA_EPOUT       NRF_DRV_USBD_EPOUT1 // Bulk OUT endpoint (host → device)

static nrf_cli_t m_cli_usb;

void cli_init(void);

/** @} */


#ifdef __cplusplus
}
#endif

#endif // CLI_CDC_H__