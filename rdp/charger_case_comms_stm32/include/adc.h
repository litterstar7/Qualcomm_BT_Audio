/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      ADC
*/

#ifndef ADC_H_
#define ADC_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli.h"

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
#ifdef VARIANT_CH273
	ADC_CURRENT_SENSE_L,
	ADC_CURRENT_SENSE_R,
    ADC_VBAT,
#else
    ADC_CURRENT_SENSE_R,
    ADC_CURRENT_SENSE_L,
    ADC_VBAT,
#endif
    ADC_VREF,
	NO_OF_ADCS
}
ADC_NO;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void adc_init(void);
void adc_sleep(void);
void adc_wake(void);
void adc_start_measuring(void);
CLI_RESULT adc_cmd(uint8_t cmd_source);
uint16_t adc_read(ADC_NO adc_no);
volatile uint16_t *adc_value_ptr(ADC_NO adc_no);

#endif /* ADC_H_ */
