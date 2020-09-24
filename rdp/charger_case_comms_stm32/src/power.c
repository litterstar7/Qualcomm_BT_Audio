/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Power modes
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include "stm32f0xx.h"
#include "main.h"
#include "timer.h"
#include "gpio.h"
#include "adc.h"
#include "led.h"
#include "memory.h"
#include "power.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

CLI_RESULT power_cmd_status(uint8_t cmd_source);
CLI_RESULT power_cmd_on(uint8_t cmd_source);
CLI_RESULT power_cmd_off(uint8_t cmd_source);
CLI_RESULT power_cmd_sleep(uint8_t cmd_source);
CLI_RESULT power_cmd_stop(uint8_t cmd_source);
CLI_RESULT power_cmd_standby(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint16_t power_reason_to_run = 0;
uint32_t sleep_count = 0;
static void (*low_power_fn)(void) = power_enter_sleep;

static const CLI_COMMAND power_command[] =
{
    { "",        power_cmd_status,  2 },
    { "on",      power_cmd_on,      2 },
    { "off",     power_cmd_off,     2 },
    { "sleep",   power_cmd_sleep,   2 },
    { "stop",    power_cmd_stop,    2 },
    { "standby", power_cmd_standby, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void power_resume(void)
{
    /* Resume Tick interrupt if disabled prior to sleep mode entry*/
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;

    /* Re-enable peripherals */
    gpio_clock_enable();
    timer_wake();
    led_wake();
    adc_wake();
}

/*
* Common code when going into sleep or stop modes.
*/
static void power_begin_sleep_or_stop(void)
{
    led_sleep();
    adc_sleep();
    timer_sleep();

    /* Regulator PFM/PWM. */
    gpio_disable(GPIO_VREG_PFM_PWM);

    gpio_clock_disable();

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
}

void power_enter_sleep(void)
{
    __disable_irq();

    sleep_count++;

    power_begin_sleep_or_stop();

    /* Request to enter SLEEP mode */
    /* Clear SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    __enable_irq();

    /* Request Wait For Interrupt */
    __WFI();

    power_resume();
}

void power_enter_stop(void)
{
    power_begin_sleep_or_stop();

    /* Request to enter STOP mode */

    PWR->CR &= ~PWR_CR_PDDS;
    PWR->CR |= PWR_CR_LPDS;

    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    /* Request Wait For Interrupt */
    __WFI();

    /* Reset SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    power_resume();
}

static void power_reset_to_standby(void)
{
    mem_cfg_standby_set();
    NVIC_SystemReset();
}

void power_enter_standby(void)
{
    /*Disable all used wakeup sources: Pin1(PA.0)*/
    PWR->CSR &= ~PWR_CSR_EWUP1;

    /*Clear all related wakeup flags*/
    PWR->CR |= (PWR_FLAG_WU << 2);

    /*Re-enable all used wakeup sources: Pin1(PA.0)*/
    PWR->CSR |= PWR_CSR_EWUP1;

    /* Select STANDBY mode */
    PWR->CR |= PWR_CR_PDDS;

    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    /* Request Wait For Interrupt */
    __WFI();
}

void power_set_run_reason(uint16_t reason)
{
    __disable_irq();
    power_reason_to_run |= reason;
    __enable_irq();
}

void power_clear_run_reason(uint16_t reason)
{
    __disable_irq();
    power_reason_to_run &= ~reason;
    __enable_irq();
}

void power_periodic(void)
{
    if (!power_reason_to_run)
    {
        /*
        * No reason left to stay in run mode, so go to a lower power mode.
        */
        low_power_fn();
    }
}

CLI_RESULT power_cmd_on(uint8_t cmd_source __attribute__((unused)))
{
    power_set_run_reason(POWER_RR_FORCE_ON);
    return CLI_OK;
}

CLI_RESULT power_cmd_off(uint8_t cmd_source __attribute__((unused)))
{
    power_clear_run_reason(POWER_RR_FORCE_ON);
    return CLI_OK;
}

CLI_RESULT power_cmd_sleep(uint8_t cmd_source __attribute__((unused)))
{
    low_power_fn = power_enter_sleep;
    return CLI_OK;
}

CLI_RESULT power_cmd_stop(uint8_t cmd_source __attribute__((unused)))
{
    low_power_fn = power_enter_stop;
    return CLI_OK;
}

CLI_RESULT power_cmd_standby(uint8_t cmd_source __attribute__((unused)))
{
    low_power_fn = power_reset_to_standby;
    return CLI_OK;
}

CLI_RESULT power_cmd_status(uint8_t cmd_source)
{
    PRINTF("0x%04x", power_reason_to_run);
    return CLI_OK;
}

CLI_RESULT power_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(power_command, cmd_source);
}
