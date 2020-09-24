/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      UART
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include "stm32f0xx_usart.h"
#include "main.h"
#include "cli.h"
#include "gpio.h"
#include "timer.h"
#include "power.h"
#include "uart.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define UART_BIT_RATE 115200

/*
* These should always be powers of 2 in order to simplify calculations and
* save memory.
*/
#ifdef VARIANT_CH273
#define UART_RX_BUFFER_SIZE 512
#else
#define UART_RX_BUFFER_SIZE 2048
#endif
#define UART_TX_BUFFER_SIZE 512

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint8_t uart_rx_buf[UART_RX_BUFFER_SIZE] = {0};
static uint8_t uart_tx_buf[UART_TX_BUFFER_SIZE] = {0};

static uint16_t uart_rx_buf_head = 0;
static uint16_t uart_rx_buf_tail = 0;
static uint16_t uart_tx_buf_head = 0;
static uint16_t uart_tx_buf_tail = 0;
static bool in_progress = false;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void uart_init(void)
{
    /*
    * Enable clock for USART1.
    */
    RCC->APB2ENR |= RCC_APB2Periph_USART1;

    /*
    * Set USART1 bit rate.
    */
    USART1->BRR = (SystemCoreClock + (UART_BIT_RATE / 2)) / UART_BIT_RATE;

    /*
    * Enable USART1, enable transmit and receive, enable the interrupts.
    */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE |
        USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_TCIE;
}

void uart_tx(char *data)
{
    /*
    * Put all the data in the buffer to be sent.
    */
    while (*data)
    {
        uint16_t next_head = (uint16_t)((uart_tx_buf_head + 1) & (UART_TX_BUFFER_SIZE-1));

        if (next_head == uart_tx_buf_tail)
        {
            /*
            * Not enough room in the buffer, so give up. Data output will be
            * truncated.
            */
            break;
        }

        uart_tx_buf[uart_tx_buf_head] = (uint8_t)*data;
        uart_tx_buf_head = next_head;
        data++;
    }

    power_set_run_reason(POWER_RR_UART_TX);
}

/*
* Force out everything in the TX buffer, to be used in the event of a fault.
*/
void uart_dump(void)
{
    while (uart_tx_buf_head != uart_tx_buf_tail)
    {
        USART1->TDR = uart_tx_buf[uart_tx_buf_tail];
        while (!(USART1->ISR & USART_ISR_TC));
        USART1->ICR |= USART_ISR_TC;
        uart_tx_buf_tail = (uart_tx_buf_tail + 1) & (UART_TX_BUFFER_SIZE - 1);
    }
}

void uart_tx_periodic(void)
{
    /*
    * Kick off any data to be sent.
    */
    if (!in_progress)
    {
        if (uart_tx_buf_head != uart_tx_buf_tail)
        {
            in_progress = true;
            USART1->TDR = uart_tx_buf[uart_tx_buf_tail];
        }
        else
        {
            power_clear_run_reason(POWER_RR_UART_TX);
        }
    }
}

void uart_rx_periodic(void)
{
	/*
    * Handle received data.
    */
	while (uart_rx_buf_head != uart_rx_buf_tail)
	{
		cli_rx(CLI_SOURCE_UART, (char)uart_rx_buf[uart_rx_buf_tail]);
		uart_rx_buf_tail = (uart_rx_buf_tail + 1) & (UART_RX_BUFFER_SIZE - 1);
	}
    power_clear_run_reason(POWER_RR_UART_RX);
}

void USART1_IRQHandler(void)
{
    uint16_t isr = USART1->ISR;

    USART1->ICR |= isr;

    if (isr & USART_ISR_RXNE)
    {
        power_set_run_reason(POWER_RR_UART_RX);
        uart_rx_buf[uart_rx_buf_head] = (uint8_t)(USART_ReceiveData(USART1) & 0xFF);
        uart_rx_buf_head = (uart_rx_buf_head + 1) & (UART_RX_BUFFER_SIZE - 1);
    }

    if (isr & USART_ISR_TC)
    {
        if (in_progress)
        {
            uart_tx_buf_tail = (uart_tx_buf_tail + 1) & (UART_TX_BUFFER_SIZE - 1);

            if (uart_tx_buf_head != uart_tx_buf_tail)
            {
                USART1->TDR = uart_tx_buf[uart_tx_buf_tail];
            }
            else
            {
                in_progress = false;
            }
        }
    }
}
