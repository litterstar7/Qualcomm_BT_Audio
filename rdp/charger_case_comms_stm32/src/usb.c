/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx_hal.h"
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "main.h"
#include "power.h"
#include "usb.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* These should always be powers of 2 in order to simplify calculations and
* save memory.
*/
#define USB_RX_BUFFER_SIZE 2048
#define USB_TX_BUFFER_SIZE 512

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static bool usb_is_ready = false;
static bool usb_data_to_send = false;

static uint8_t usb_rx_buf[USB_RX_BUFFER_SIZE] = {0};
static uint8_t usb_tx_buf[USB_TX_BUFFER_SIZE] = {0};

static uint16_t usb_rx_buf_head = 0;
static uint16_t usb_rx_buf_tail = 0;
static uint16_t usb_tx_buf_head = 0;
static uint16_t usb_tx_buf_tail = 0;

USBD_HandleTypeDef hUsbDeviceFS;
extern PCD_HandleTypeDef hpcd_USB_FS;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void SystemClock_Config(void)
{
    /* Configure the USB clock source */
    RCC->CFGR3 |= RCC_CFGR3_USBSW;
}

void usb_init(void)
{
    SystemClock_Config();

    if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) == USBD_OK)
    {
        if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) == USBD_OK)
        {
            if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) == USBD_OK)
            {
                if (USBD_Start(&hUsbDeviceFS) == USBD_OK)
                {
                    PRINT_B("USBD_Start OK");
                }
            }
        }
    }
}

void usb_tx(char *data)
{
    usb_data_to_send = true;

    /*
    * Put all the data in the buffer to be sent.
    */
    while (*data)
    {
        uint16_t next_head = (uint16_t)((usb_tx_buf_head + 1) & (USB_TX_BUFFER_SIZE-1));

        if (next_head == usb_tx_buf_tail)
        {
            /*
            * Not enough room in the buffer, so give up. Data output will be
            * truncated.
            */
            break;
        }

        usb_tx_buf[usb_tx_buf_head] = (uint8_t)*data;
        usb_tx_buf_head = next_head;
        data++;
    }

    if (usb_is_ready)
    {
        power_set_run_reason(POWER_RR_USB_TX);
    }
}

void usb_rx(uint8_t *data, uint32_t len)
{
    uint32_t n;

    power_set_run_reason(POWER_RR_USB_RX);
    for (n=0; n<len; n++)
    {
        uint16_t next_head = (usb_rx_buf_head + 1) & (USB_RX_BUFFER_SIZE - 1);

        if (next_head == usb_rx_buf_tail)
        {
            /*
            * Not enough room in the buffer, so give up.
            */
            break;
        }
        else
        {
            usb_rx_buf[usb_rx_buf_head] = data[n];
            usb_rx_buf_head = next_head;
        }
    }
}

CLI_RESULT usb_cmd(uint8_t cmd_source)
{
    PRINTF("CNTR = 0x%04x", USB->CNTR);
    PRINTF("BTABLE = 0x%04x", USB->BTABLE);
    PRINTF("EP0R = 0x%04x", USB->EP0R);
    PRINTF("EP1R = 0x%04x", USB->EP1R);
    PRINTF("DADDR = 0x%04x", USB->DADDR);

    return CLI_OK;
}

void usb_tx_complete(void)
{
    usb_data_to_send = false;
    power_clear_run_reason(POWER_RR_USB_TX);
}

void usb_tx_periodic(void)
{
    uint16_t head = usb_tx_buf_head;
    uint16_t tail = usb_tx_buf_tail;

    /*
    * Kick off any data to be sent.
    */
    if (head != tail)
    {
        uint16_t len;

        if (tail > head)
        {
            len = USB_TX_BUFFER_SIZE - tail;
        }
        else
        {
            len = head - tail;
        }

        if (CDC_Transmit_FS((uint8_t *)&usb_tx_buf[tail], len)==USBD_OK)
        {
            usb_tx_buf_tail = (usb_tx_buf_tail + len) & (USB_TX_BUFFER_SIZE - 1);
        }
    }
}

void usb_rx_periodic(void)
{
    /*
    * Handle received data.
    */
    while (usb_rx_buf_head != usb_rx_buf_tail)
    {
        cli_rx(CLI_SOURCE_USB, (char)usb_rx_buf[usb_rx_buf_tail]);
        usb_rx_buf_tail = (usb_rx_buf_tail + 1) & (USB_RX_BUFFER_SIZE - 1);
    }
    power_clear_run_reason(POWER_RR_USB_RX);
}

void Error_Handler(void)
{
}

void USB_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_FS);
}

void usb_ready(void)
{
    if (!usb_is_ready)
    {
        PRINT_B("USB ready");
        usb_is_ready = true;

        if (usb_data_to_send)
        {
            power_set_run_reason(POWER_RR_USB_TX);
        }
    }
}

void usb_not_ready(void)
{
    if (usb_is_ready)
    {
        PRINT_B("USB not ready");
        power_clear_run_reason(POWER_RR_USB_TX);
        usb_is_ready = false;
    }
}
