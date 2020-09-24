/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB
*/

#ifndef USB_H_
#define USB_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void usb_init(void);
void usb_tx_periodic(void);
void usb_rx_periodic(void);
void usb_tx_complete(void);
void usb_tx(char *data);
void usb_rx(uint8_t *data, uint32_t len);
CLI_RESULT usb_cmd(uint8_t cmd_source);
void usb_ready(void);
void usb_not_ready(void);

#endif /* USB_H_ */
