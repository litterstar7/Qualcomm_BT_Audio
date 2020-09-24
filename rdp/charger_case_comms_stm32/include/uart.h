/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      UART
*/

#ifndef UART_H_
#define UART_H_

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void uart_init(void);
void uart_tx_periodic(void);
void uart_rx_periodic(void);
void uart_tx(char *data);
void uart_dump(void);

#endif /* UART_H_ */
