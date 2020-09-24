/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Device specific code for charger comms.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "main.h"
#include "charger_comms.h"
#include "cli.h"
#include "adc.h"
#include "gpio.h"
#include "power.h"
#include "timer.h"
#include "case.h"
#include "wire.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static void on_complete_impl(void);
static void on_tx_start_impl(uint8_t *buf, uint8_t num_tx_octets);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

earbud_channel left_earbud;
earbud_channel right_earbud;
uint16_t adc_buf_left[512];
uint16_t adc_buf_right[512];

const charger_comms_cfg cfg =
{
    .on_complete = on_complete_impl,
    .on_tx_start = on_tx_start_impl,
    .packet_reply_timeout_ms = 20,
    .adc_threshold = 110
};

bool received_charger_comm_packet = false;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void charger_comms_vreg_high(void)
{
    GPIO_RESET(GPIO_VREG_MOD);
    GPIO_OUTPUT(GPIO_VREG_MOD);
}

void charger_comms_vreg_low(void)
{
    GPIO_INPUT(GPIO_VREG_MOD);
}

void charger_comms_vreg_reset(void)
{
    GPIO_SET(GPIO_VREG_MOD);
    GPIO_OUTPUT(GPIO_VREG_MOD);
}

static void init_earbuds(void)
{
    /* All other fields should be zero initialised by the CRT. */

    left_earbud.current_adc_val = adc_value_ptr(ADC_CURRENT_SENSE_L);
    left_earbud.adc_buf = adc_buf_left;

    right_earbud.current_adc_val = adc_value_ptr(ADC_CURRENT_SENSE_R);
    right_earbud.adc_buf = adc_buf_right;
}

void charger_comms_device_init(void)
{
    charger_comms_init(&cfg);
    init_earbuds();
}

static void on_complete_impl(void)
{
    timer_comms_tick_stop();
    power_clear_run_reason(POWER_RR_CHARGER_COMMS);

    /* No need to listen any more. */

    gpio_disable(GPIO_CURRENT_SENSE_AMP);
    gpio_disable(GPIO_VREG_PFM_PWM);

    if ((left_earbud.data_valid) || (right_earbud.data_valid))
    {
        received_charger_comm_packet = true;
    }
}

static void on_tx_start_impl(uint8_t *buf, uint8_t num_tx_octets)
{
    cli_tx_hex(CLI_BROADCAST, "WIRE->COMMS", buf, num_tx_octets);

    power_set_run_reason(POWER_RR_CHARGER_COMMS);

    /* Must have the current senses switched to receive charger comms messages
     * and the regulator must be in PWM to transmit charger comms. */
    gpio_enable(GPIO_CURRENT_SENSE_AMP);
    gpio_enable(GPIO_VREG_PFM_PWM);

    timer_comms_tick_start();

#ifdef CHARGER_COMMS_FAKE
    earbud_rx(buf, num_tx_octets);
#endif
}

void charger_comms_periodic(void)
{
    if (received_charger_comm_packet)
    {
        uint8_t result[CHARGER_COMMS_MAX_MSG_LEN];
        received_charger_comm_packet = false;

        if(left_earbud.data_valid)
        {
            charger_comms_fetch_rx_data(&left_earbud, result);
            wire_rx(EARBUD_LEFT, result, left_earbud.num_rx_octets);
        }

        if(right_earbud.data_valid)
        {
            charger_comms_fetch_rx_data(&right_earbud, result);
            wire_rx(EARBUD_RIGHT, result, right_earbud.num_rx_octets);
        }
    }

    if(charger_comms_should_read_header())
    {
        charger_comms_read_header();
    }
}
