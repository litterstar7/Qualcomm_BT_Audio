/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Timers
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx_tim.h"
#include "main.h"
#include "cli.h"
#include "charger_comms.h"
#include "adc.h"
#include "case.h"
#include "power.h"
#include "timer.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define FAST_TIMER_PERIOD_US 20
#define STATUS_PERIOD_MS 60000
#define WATCHDOG_KICK_PERIOD_MS 1000

#define PRESCALER_US 1000000
#define PRESCALER_MS 1000

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

#ifdef FAST_TIMER_INTERRUPT
volatile uint64_t global_time_us = 0;
#endif

volatile bool systick_has_ticked = false;
uint32_t slow_count = 0;
volatile uint32_t ticks = 0;
static uint8_t tim17_ctr = 0;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static void timer_setup(
    TIM_TypeDef *tim,
    uint32_t prescaler,
    uint32_t period_us)
{
    TIM_TimeBaseInitTypeDef init_struct;

    TIM_TimeBaseStructInit(&init_struct);
    init_struct.TIM_Prescaler = (uint16_t)((SystemCoreClock / prescaler) - 1);
    init_struct.TIM_Period = period_us - 1;
    TIM_TimeBaseInit(tim, &init_struct);

    /*
    * Enable the timer.
    */
    tim->CR1 |= TIM_CR1_CEN;

    /*
    * Enable and clear the timer interrupt.
    */
    if (period_us)
    {
        tim->SR = 0;
        tim->DIER |= TIM_DIER_UIE;
    }
}

void timer_init(void)
{
	RCC->APB1ENR |= RCC_APB1Periph_TIM14 | RCC_APB1Periph_TIM3;
    RCC->APB2ENR |= RCC_APB2Periph_TIM17;

#ifdef FAST_TIMER_INTERRUPT
    timer_setup(TIM14, PRESCALER_US, FAST_TIMER_PERIOD_US);
#else
    timer_setup(TIM14, PRESCALER_US, 0);
#endif
    timer_setup(TIM3, PRESCALER_US, 100);
    timer_setup(TIM17, PRESCALER_MS, WATCHDOG_KICK_PERIOD_MS);

    SysTick_Config(SystemCoreClock / TIMER_FREQUENCY_HZ);
}

CLI_RESULT timer_cmd(uint8_t cmd_source)
{
#ifdef FAST_TIMER_INTERRUPT
    PRINTF("%dms %dms",
        (uint32_t)(global_time_us / 1000), ticks);
#else
    PRINTF("%04x %dcs", TIM14->CNT, ticks);
#endif
    return CLI_OK;
}

void SysTick_Handler(void)
{
    ticks++;
    systick_has_ticked = true;
}

void TIM17_IRQHandler(void)
{
    if (TIM17->SR & TIM_SR_UIF)
    {
        TIM17->SR &= ~TIM_SR_UIF;

        /*
        * We don't want to kick the watchdog at this point, because we will
        * always keep coming back here even if stuck in a periodic function
        * or lower-priority interrupt. Instead, force run mode so that
        * wdog_periodic() will eventually do it.
        */
        power_set_run_reason(POWER_RR_WATCHDOG);

        /*
        * Might also want to initiate the case status sequence.
        */
        tim17_ctr = (tim17_ctr+1) % (STATUS_PERIOD_MS / WATCHDOG_KICK_PERIOD_MS);
        if (!tim17_ctr)
        {
            case_status_time(false);
        }
    }
}

#ifdef FAST_TIMER_INTERRUPT
void TIM14_IRQHandler(void)
{
	if (TIM14->SR & TIM_SR_UIF)
	{
		TIM14->SR &= ~TIM_SR_UIF;
		global_time_us += FAST_TIMER_PERIOD_US;
	}
}
#endif

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_UIF)
    {
        TIM3->SR &= ~TIM_SR_UIF;
        if(charger_comms_is_active())
        {
            adc_start_measuring();
            charger_comms_tick();
        }
    }
}

void delay_ms(int ms)
{
#ifdef FAST_TIMER_INTERRUPT
    uint64_t end = global_time_us +  ms * 1000;

    while (global_time_us < end);
#else
    uint32_t end = ticks +  ms / 10;

    while (ticks < end);
#endif
}

void timer_sleep(void)
{
    timer_comms_tick_stop();
    TIM14->CR1 &= ~TIM_CR1_CEN;
#ifdef FAST_TIMER_INTERRUPT
    TIM14->DIER &= ~TIM_DIER_UIE;
#endif
}

void timer_wake(void)
{
    TIM14->CR1 |= TIM_CR1_CEN;
#ifdef FAST_TIMER_INTERRUPT
    TIM14->DIER |= TIM_DIER_UIE;
#endif
}

void timer_comms_tick_start(void)
{
    TIM3->DIER |= TIM_DIER_UIE;
}

void timer_comms_tick_stop(void)
{
    TIM3->DIER &= ~TIM_DIER_UIE;
}

/*
* Return the value of a counter, to be used as a seed for rand().
*/
uint16_t timer_seed_value(void)
{
    return TIM17->CNT;
}
