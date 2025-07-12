#ifndef __USBD_CDC
#define __USBD_CDC

#include <stdint.h>

/**
 * @brief Enable power USB detection
 *
 * Configure if example supports USB port connection
 */
#ifndef USBD_POWER_DETECTION
#define USBD_POWER_DETECTION true
#endif

void cdc_init(void);

#endif __USBD_CDC