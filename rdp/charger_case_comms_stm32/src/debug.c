/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Debug commands for the LD20-11114 board
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "debug.h"
#include "adc.h"
#include "case.h"
#include "charger.h"
#include "charger_comms.h"
#include "cli.h"
#include "gpio.h"
#include "led.h"
#include "power.h"
#include "timer.h"
#include "wire.h"
#include "pfn.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT debug_start(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_led_red(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_led_green(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_led_blue(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_led_disable(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_battery_mv(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_current_sense_en(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_current_sense_read(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_vreg_en(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_vreg_disable(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_vreg_pwm(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_vref_pfm(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_vreg_high(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_vreg_low(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_vreg_reset(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_mcu_stdby(uint8_t cmd_source);
static CLI_RESULT debug_test_packet(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_chg_stdby(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_chg_100(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_chg_500(uint8_t cmd_source __attribute__((unused)));
static CLI_RESULT debug_chg_ilim(uint8_t cmd_source __attribute__((unused)));

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND debug_commands[] =
{
    { "start",              debug_start, 2 },
    { "led_red",            debug_led_red, 2 },
    { "led_green",          debug_led_green, 2 },
    { "led_blue",           debug_led_blue, 2 },
    { "led_disable",        debug_led_disable, 2 },
    { "battery_mv",         debug_battery_mv, 2 },
    { "current_sense_en",   debug_current_sense_en, 2 },
    { "current_sense_read", debug_current_sense_read, 2 },
    { "vreg_en",            debug_vreg_en, 2 },
    { "vreg_disable",       debug_vreg_disable, 2 },
    { "vreg_pwm",           debug_vreg_pwm, 2 },
    { "vref_pfm",           debug_vref_pfm, 2 },
    { "vreg_high",          debug_vreg_high, 2 },
    { "vreg_low",           debug_vreg_low, 2 },
    { "vreg_reset",         debug_vreg_reset, 2 },
    { "mcu_stdby",          debug_mcu_stdby, 2 },
    { "test_packet",        debug_test_packet, 2 },
    { "chg_stdby",          debug_chg_stdby, 2 },
    { "chg_100",            debug_chg_100, 2 },
    { "chg_500",            debug_chg_500, 2 },
    { "chg_ilim",           debug_chg_ilim, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT debug_start(uint8_t cmd_source __attribute__((unused)))
{
    pfn_set_runnable("case", false);
    pfn_set_runnable("led", false);
    power_set_run_reason(POWER_RR_DEBUG);
    return CLI_OK;
}

static CLI_RESULT debug_led_red(uint8_t cmd_source __attribute__((unused)))
{
    led_set_colour(LED_COLOUR_RED);
    return CLI_OK;
}

static CLI_RESULT debug_led_green(uint8_t cmd_source __attribute__((unused)))
{
    led_set_colour(LED_COLOUR_GREEN);
    return CLI_OK;
}

static CLI_RESULT debug_led_blue(uint8_t cmd_source __attribute__((unused)))
{
    led_set_colour(LED_COLOUR_BLUE);
    return CLI_OK;
}

static CLI_RESULT debug_led_disable(uint8_t cmd_source __attribute__((unused)))
{
    led_set_colour(LED_COLOUR_OFF);
    return CLI_OK;
}

static CLI_RESULT debug_battery_mv(uint8_t cmd_source __attribute__((unused)))
{
    gpio_enable(GPIO_VBAT_MONITOR_ON_OFF);
    delay_ms(200);
    adc_start_measuring();
    delay_ms(30);
    PRINTF("%u", (uint32_t)get_vbat_mv());
    gpio_disable(GPIO_VBAT_MONITOR_ON_OFF);

    return CLI_OK;
}

static CLI_RESULT debug_current_sense_en(uint8_t cmd_source __attribute__((unused)))
{
    gpio_enable(GPIO_CURRENT_SENSE_AMP);
    return CLI_OK;
}

static CLI_RESULT debug_current_sense_read(uint8_t cmd_source __attribute__((unused)))
{
    uint16_t left_sense;
    uint16_t right_sense;

    adc_start_measuring();
    delay_ms(30);
    left_sense = adc_read(ADC_CURRENT_SENSE_L);
    right_sense = adc_read(ADC_CURRENT_SENSE_R);

    PRINTF("%u, %u", left_sense, right_sense);
    return CLI_OK;
}

static CLI_RESULT debug_vreg_en(uint8_t cmd_source __attribute__((unused)))
{
    gpio_enable(GPIO_VREG_EN);
    return CLI_OK;
}

static CLI_RESULT debug_vreg_disable(uint8_t cmd_source __attribute__((unused)))
{
    gpio_disable(GPIO_VREG_EN);
    return CLI_OK;
}

static CLI_RESULT debug_vreg_pwm(uint8_t cmd_source __attribute__((unused)))
{
    gpio_enable(GPIO_VREG_PFM_PWM);
    return CLI_OK;
}

static CLI_RESULT debug_vref_pfm(uint8_t cmd_source __attribute__((unused)))
{
    gpio_disable(GPIO_VREG_PFM_PWM);
    return CLI_OK;
}

static CLI_RESULT debug_vreg_high(uint8_t cmd_source __attribute__((unused)))
{
    charger_comms_vreg_high();
    return CLI_OK;
}

static CLI_RESULT debug_vreg_low(uint8_t cmd_source __attribute__((unused)))
{
    charger_comms_vreg_low();
    return CLI_OK;
}

static CLI_RESULT debug_vreg_reset(uint8_t cmd_source __attribute__((unused)))
{
    charger_comms_vreg_reset();
    return CLI_OK;
}

static CLI_RESULT debug_mcu_stdby(uint8_t cmd_source)
{
    return power_cmd_standby(cmd_source);
}

static CLI_RESULT debug_test_packet(uint8_t cmd_source __attribute__((unused)))
{
    uint8_t test_data[2];

    test_data[0] = 0xDE;
    test_data[1] = 0xAD;

    wire_tx(WIRE_DEST_RIGHT, test_data, 2); 
    return CLI_OK;
}

static CLI_RESULT debug_chg_stdby(uint8_t cmd_source __attribute__((unused)))
{
    charger_set_current(CHARGER_CURRENT_MODE_STANDBY);
    return CLI_OK;
}

static CLI_RESULT debug_chg_100(uint8_t cmd_source __attribute__((unused)))
{
    charger_set_current(CHARGER_CURRENT_MODE_100MA);
    return CLI_OK;
}

static CLI_RESULT debug_chg_500(uint8_t cmd_source __attribute__((unused)))
{
    charger_set_current(CHARGER_CURRENT_MODE_500MA);
    return CLI_OK;
}

static CLI_RESULT debug_chg_ilim(uint8_t cmd_source __attribute__((unused)))
{
    charger_set_current(CHARGER_CURRENT_MODE_ILIM);
    return CLI_OK;
}

CLI_RESULT debug_cmd(uint8_t cmd_source __attribute__((unused)))
{
    return cli_process_sub_cmd(debug_commands, cmd_source);
}
