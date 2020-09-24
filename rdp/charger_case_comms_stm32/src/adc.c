/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      ADC
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx_adc.h"
#include "stm32f0xx_dma.h"
#include "main.h"
#include "timer.h"
#include "adc.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    char *name;
    uint32_t channel;
}
ADC_CONFIG;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static bool adc_in_progress = false;

static const ADC_CONFIG adc_config[NO_OF_ADCS] =
{
#ifdef VARIANT_CH273
    { "L",       ADC_Channel_1 },
    { "R",       ADC_Channel_2 },
    { "VBAT",    ADC_Channel_3 },
#else
    { "R",       ADC_Channel_1 },
    { "L",       ADC_Channel_3 },
    { "VBAT",    ADC_Channel_4 },
#endif
    { "VREFINT", ADC_Channel_17 }
};

static volatile uint16_t adc_value[NO_OF_ADCS] = {0};

TIMER_DEBUG_VARIABLES(adc)

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void adc_sleep(void)
{
    /*
    * Disable ADC peripheral clock.
    */
    RCC->APB2ENR &= ~RCC_APB2Periph_ADC1;
    RCC->AHBENR &= ~RCC_AHBPeriph_DMA1;
}

void adc_wake(void)
{
    /*
    * Enable ADC and DMA peripheral clocks.
    */
    RCC->APB2ENR |= RCC_APB2Periph_ADC1;
    RCC->AHBENR |= RCC_AHBPeriph_DMA1;

    adc_in_progress = false;
}

void adc_init(void)
{
    uint8_t n;

    adc_wake();

    /*
    * DMA initialisation.
    */
    DMA1_Channel1->CPAR = (uint32_t)(&ADC1->DR);
    DMA1_Channel1->CMAR = (uint32_t)adc_value;
    DMA1_Channel1->CNDTR = NO_OF_ADCS;
    DMA1_Channel1->CCR =  DMA_CCR_CIRC | DMA_CCR_MINC |
        DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0 | DMA_CCR_EN;

    /*
    * ADC initialisation.
    */
    ADC1->CFGR1 = ADC_CFGR1_DMAEN;

    /*
    * Enable VREFINT.
    */
    ADC->CCR |= (uint32_t)ADC_CCR_VREFEN;

    /*
    * Clear and enable EOS interrupt.
    */
    ADC1->ISR = 0xFFFFFFFF;
    ADC1->IER |= ADC_IT_EOSEQ;

    /*
    * Channel and sampling time configuration.
    */
    for (n=0; n<NO_OF_ADCS; n++)
    {
        ADC1->CHSELR |= adc_config[n].channel;
    }
    ADC1->SMPR = (uint32_t)ADC_SampleTime_71_5Cycles;

    /*
    * Enable ADC.
    */
    ADC1->CR |= (uint32_t)ADC_CR_ADEN;
}

void adc_start_measuring(void)
{
    if (!adc_in_progress)
    {
        adc_in_progress = true;
        TIMER_DEBUG_START(adc)

        /*
        * Start conversion.
        */
        ADC1->CR |= (uint32_t)ADC_CR_ADSTART;
    }
}

CLI_RESULT adc_cmd(uint8_t cmd_source)
{
    uint8_t n;

    for (n=0; n<NO_OF_ADCS; n++)
    {
        PRINTF("%-7s  %03x", adc_config[n].name, adc_value[n]);
    }

    PRINTF("%d %d %d %d", adc_no_of_measurements,
        (uint32_t)adc_total_time_taken,
        (uint32_t)(adc_total_time_taken / adc_no_of_measurements),
        (uint32_t)adc_slowest_time);

    return CLI_OK;
}

volatile uint16_t *adc_value_ptr(ADC_NO adc_no)
{
    return &adc_value[adc_no];
}

uint16_t adc_read(ADC_NO adc_no)
{
	return adc_value[adc_no];
}

void ADC1_IRQHandler(void)
{
    volatile uint32_t isr = ADC1->ISR;

    ADC1->ISR = isr;

    if (isr & ADC_ISR_EOS)
    {
        TIMER_DEBUG_STOP(adc);
        adc_in_progress = false;
    }
}

