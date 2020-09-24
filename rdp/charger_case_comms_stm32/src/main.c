/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger case application
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include "stm32f0xx.h"
#include "main.h"
#include "cli.h"
#include "adc.h"
#include "gpio.h"
#include "interrupt.h"
#include "uart.h"
#include "timer.h"
#include "led.h"
#include "ccp.h"
#include "pfn.h"
#include "wdog.h"
#include "wire.h"
#include "charger_comms.h"
#include "power.h"
#include "case.h"
#include "charger_comms_device.h"
#include "memory.h"
#include "flash.h"
#include "usb.h"
#include "dfu.h"
#include "clock.h"

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

int main(void)
{
    mem_init();

    /*
    * Wait in standby if configured to do so.
    */
    if (mem_cfg_standby())
    {
        gpio_init();
        interrupt_init();
        power_enter_standby();
        clock_init();
    }

    /*
    * Initialisation.
    */
    wdog_init();
    gpio_init();
    uart_init();
    cli_init();
    timer_init();
    led_init();
    adc_init();
    case_init();
    charger_comms_device_init();
    interrupt_init();
#ifndef VARIANT_CH273
    flash_init();
    dfu_init();
#endif
#ifdef USB_ENABLED
    usb_init();
#endif

    gpio_disable(GPIO_VREG_PFM_PWM);
    gpio_enable(GPIO_VREG_EN);

    adc_start_measuring();

    charger_comms_vreg_high();

    PRINT_BU("\x1B[2J\x1B[H");
    PRINT_B("-------------------------------------------------------------------------------");
    PRINT_B("QUALCOMM " VARIANT_NAME);
    PRINTF_B("Build time %s %s", __DATE__, __TIME__);
    PRINT_B("-------------------------------------------------------------------------------");

    /*
    * Main loop.
    */
    while (1)
    {
        while (!systick_has_ticked);
        systick_has_ticked = false;

        /*
        * Call all the periodic functions.
        */
        pfn_periodic();

        if (systick_has_ticked)
        {
            slow_count++;
        }
    }
}
