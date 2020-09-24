/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Case status
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include "main.h"
#include "gpio.h"
#include "adc.h"
#include "power.h"
#include "ccp.h"
#include "led.h"
#include "case.h"
#include "charger.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* CASE_RESET_DELAY_TIME: Time in ticks that we wait following the earbud's
* acknowledgement of a reset command before starting to poll it.
*/
#define CASE_RESET_DELAY_TIME 100

/**
 * Time in ticks that we must wait for the VBAT monitor to settle before taking
 * a measurement from it.
 */
#define CASE_BATTERY_READ_DELAY_TIME 200

/**
 * Time in ticks to wait for the ADC reading to take place. 
 */
#define CASE_BATTERY_ADC_DELAY_TIME 30

#define VREFINT_CAL_ADDR (0x1FFFF7BA)

/*
* Define the battery levels that we will represent as 100% and 0%.
*/
#define BATTERY_MV_100_PERCENT 4200
#define BATTERY_MV_0_PERCENT 3400

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CS_IDLE,
    CS_STATUS_WANTED,
    CS_SENT_STATUS_REQUEST,
    CS_RESET_WANTED,
    CS_SENT_RESET,
    CS_RESET_DELAY,
    CS_RESETTING
}
CASE_STATE;

typedef struct
{
    CASE_STATE state;
    uint16_t state_time;
    bool valid;
    uint8_t pp;
    uint8_t battery;
    uint8_t charging;
    bool ack;
    bool give_up;
    bool abort;
}
CASE_EARBUD_STATUS;

typedef enum
{
    CASE_BATTERY_IDLE,
    CASE_BATTERY_START_READING,
    CASE_BATTERY_READING,
    CASE_BATTERY_STOP_READING,
    CASE_BATTERY_DONE
}
CASE_BATTERY_STATE;

typedef struct
{
    CASE_BATTERY_STATE state;
    uint8_t current_battery_percent;
    uint16_t delay_ticks;
    bool led;
}
CASE_BATTERY_STATUS;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static void case_rx_earbud_status(uint8_t earbud, uint8_t pp, uint8_t battery, uint8_t charging);
static void case_ack(uint8_t earbud);
static void case_give_up(uint8_t earbud);
static void case_abort(uint8_t earbud);
static void case_broadcast_finished(void);
static CLI_RESULT case_cmd_status(uint8_t cmd_source);
static CLI_RESULT case_cmd_reset(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const uint8_t case_earbud_rr[NO_OF_EARBUDS] =
    { POWER_RR_STATUS_L, POWER_RR_STATUS_R };

static bool lid_now = false;
static bool lid_before = false;
static bool chg_now = false;
static bool chg_before = false;
static volatile bool case_event = true;
static bool case_dfu_planned = false;
static CASE_EARBUD_STATUS case_earbud_status[NO_OF_EARBUDS] = {0};
static CASE_BATTERY_STATUS case_battery_status = { .state = CASE_BATTERY_IDLE };

static const CCP_USER_CB case_ccp_cb =
{
    case_rx_earbud_status,
    case_ack,
    case_give_up,
    case_abort,
    case_broadcast_finished
};

static const CLI_COMMAND case_command[] =
{
    { "status", case_cmd_status, 2 },
    { "reset",  case_cmd_reset,  2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

uint16_t get_vbat_mv(void)
{
#ifdef TEST
    uint16_t cal = 0x600;
#else
    uint16_t cal = (*((uint16_t*)VREFINT_CAL_ADDR));
#endif
    uint16_t vbat = adc_read(ADC_VBAT);
    uint16_t ref = adc_read(ADC_VREF);

    /*
    * Prevent a possible divide by zero in the upcoming voltage calculation.
    */
    if (ref==0)
    {
        ref = cal;
    }

    float volts = 6.6f * (cal/(1.0f * ref)) * (vbat/4095.0f);
    return (uint16_t)(volts * 1000);
}

uint8_t case_battery_percentage(void)
{
    uint16_t mv = get_vbat_mv();
    uint8_t ret = 0;

    if (mv > BATTERY_MV_0_PERCENT)
    {
        if (mv < BATTERY_MV_100_PERCENT)
        {
            ret = (uint8_t)((100 * (mv - BATTERY_MV_0_PERCENT) +
                (BATTERY_MV_100_PERCENT - BATTERY_MV_0_PERCENT) / 2) /
                (BATTERY_MV_100_PERCENT - BATTERY_MV_0_PERCENT));
        }
        else
        {
            ret = 100;
        }
    }

    return ret;
}

void case_event_occurred(void)
{
    power_set_run_reason(POWER_RR_CASE_EVENT);
    case_event = true;
}

void case_init(void)
{
    case_earbud_status[EARBUD_LEFT].battery = 0xFF;
    case_earbud_status[EARBUD_RIGHT].battery = 0xFF;

    ccp_init(&case_ccp_cb);
}

/*
* An earbud has responded to our status request.
*/
static void case_rx_earbud_status(
    uint8_t earbud,
    uint8_t pp,
    uint8_t battery,
    uint8_t charging)
{
    CASE_EARBUD_STATUS *ces = &case_earbud_status[earbud];

    ces->valid = true;
    ces->pp = pp;
    ces->battery = battery;
    ces->charging = charging;
}

/*
* Call to change state.
*/
static void case_new_state(uint8_t earbud, CASE_STATE new_state)
{
    CASE_EARBUD_STATUS *ces = &case_earbud_status[earbud];

    /*PRINTF_B("case state (%d) %d->%d", earbud, ces->state, new_state);*/

    ces->state = new_state;
    ces->state_time = 0;

    if (ces->state==CS_IDLE)
    {
        power_clear_run_reason(case_earbud_rr[earbud]);
    }
    else
    {
        power_set_run_reason(case_earbud_rr[earbud]);
    }
}

/*
* Start the status sequence on the specified earbud, if possible.
*/
static bool case_start_status_sequence(uint8_t earbud)
{
    if (case_earbud_status[earbud].state==CS_IDLE)
    {
        case_new_state(earbud, CS_STATUS_WANTED);
        return true;
    }

    return false;
}

/*
* Initiate requesting/sending status messages.
*/
void case_status_time(bool led)
{
    if (!case_dfu_planned)
    {
        if ((case_battery_status.state==CASE_BATTERY_IDLE) ||
            (case_battery_status.state==CASE_BATTERY_DONE))
        {
            /*
            * No battery read in progress, so start one.
            */
            case_battery_status.state = CASE_BATTERY_START_READING;
            case_battery_status.led = led;
        }
        else if (led)
        {
            /*
            * Set the led flag, so that the battery read already in progress
            * will report to the LED module at the end.
            */
            case_battery_status.led = true;
        }

        /*
        * We will do one earbud after the other.
        */
        if (!case_start_status_sequence(EARBUD_LEFT))
        {
            case_start_status_sequence(EARBUD_RIGHT);
        }
    }
}

static void case_ack(uint8_t earbud)
{
    case_earbud_status[earbud].ack = true;
}

static void case_give_up(uint8_t earbud)
{
    PRINTF_B("Give up (%d)", earbud);
    case_earbud_status[earbud].give_up = true;
}

static void case_abort(uint8_t earbud)
{
    PRINTF_B("Abort (%d)", earbud);
    case_earbud_status[earbud].abort = true;
}

static void case_broadcast_finished(void)
{
    power_clear_run_reason(POWER_RR_BROADCAST);
}

bool case_allow_dfu(void)
{
    bool ret = false;

    if ((case_earbud_status[EARBUD_LEFT].state == CS_IDLE) &&
        (case_earbud_status[EARBUD_RIGHT].state == CS_IDLE))
    {
        ret = true;
    }

    case_dfu_planned = true;
    return ret;
}

void case_dfu_finished(void)
{
    case_dfu_planned = false;
}

static void case_battery_status_periodic(void)
{
    /* Ensure that we read the case battery before initiating the status
     * sequence with the earbuds. */
    switch (case_battery_status.state)
    {
        case CASE_BATTERY_START_READING:
            /* We must enable the VBAT monitor circuit and then wait for it
             * to settle before taking a measurement. */
            gpio_enable(GPIO_VBAT_MONITOR_ON_OFF);
            case_battery_status.delay_ticks = CASE_BATTERY_READ_DELAY_TIME; 
            case_battery_status.state = CASE_BATTERY_READING;
            break;

        case CASE_BATTERY_READING:
            /* Wait and then instruct the ADC to take the measurement. */
            if (!case_battery_status.delay_ticks)
            {
                adc_start_measuring();
                case_battery_status.delay_ticks = CASE_BATTERY_ADC_DELAY_TIME; 
                case_battery_status.state = CASE_BATTERY_STOP_READING;
            }
            else 
            {
                case_battery_status.delay_ticks--;
            }
            break;

        case CASE_BATTERY_STOP_READING:
            /* Wait for the ADC to take the measurement. */
            if (!case_battery_status.delay_ticks)
            {
                case_battery_status.current_battery_percent = case_battery_percentage();
                /* We no longer need the VBAT monitor HW to be powered */
                gpio_disable(GPIO_VBAT_MONITOR_ON_OFF);
                case_battery_status.state = CASE_BATTERY_DONE;
                if (case_battery_status.led)
                {
                    led_indicate_battery(case_battery_status.current_battery_percent);
                }
            }
            else 
            {
                case_battery_status.delay_ticks--;
            }
            break;

        case CASE_BATTERY_IDLE:
        case CASE_BATTERY_DONE:
        default:
            break;
    }
}

void case_periodic(void)
{
    case_earbud_status[EARBUD_LEFT].state_time++;
    case_earbud_status[EARBUD_RIGHT].state_time++;

    if (case_event)
    {
        case_event = false;
        lid_now = gpio_active(GPIO_MAG_SENSOR);
        chg_now = charger_connected();

        if (lid_now && (lid_now != lid_before))
        {
            /*
            * Lid is opened. Initiate a status exchange and request an
            * LED indication of the battery level.
            */
            case_status_time(true);
        }

        if ((lid_now != lid_before) || (chg_now != chg_before))
        {
            if (ccp_tx_short_status(lid_now, chg_now))
            {
                power_set_run_reason(POWER_RR_BROADCAST);
                lid_before = lid_now;
                chg_before = chg_now;
            }
        }

        /*
        * It's possible that another event might have occurred in the meantime,
        * so check the event flag before clearing the run reason.
        */
        if (!case_event)
        {
            power_clear_run_reason(POWER_RR_CASE_EVENT);
        }
    }
    else
    {
        uint8_t e;

        /* Handle the battery reading first before attempting to handle
         * the status of each earbud */
        case_battery_status_periodic();

        for (e=0; e<NO_OF_EARBUDS; e++)
        {
            CASE_EARBUD_STATUS *ces = &case_earbud_status[e];

            switch (ces->state)
            {
                default:
                case CS_IDLE:
                    break;

                case CS_RESET_WANTED:
                    if (ccp_tx_reset(e, true))
                    {
                        case_new_state(e, CS_SENT_RESET);
                    }
                    break;

                case CS_SENT_RESET:
                    if (ces->ack)
                    {
                        /*
                        * The reset command was accepted. Clear any previously
                        * received status information.
                        */
                        ces->valid = false;
                        ces->battery = 0xFF;
                        case_new_state(e, CS_RESET_DELAY);
                    }
                    else if (ces->abort)
                    {
                        case_new_state(e, CS_RESET_WANTED);
                    }
                    else if (ces->give_up)
                    {
                        case_new_state(e, CS_IDLE);
                    }
                    break;

                case CS_RESET_DELAY:
                    /*
                    * After initiating a reset, wait for a bit before polling
                    * the earbud.
                    */
                    if (ces->state_time > CASE_RESET_DELAY_TIME)
                    {
                        if (ccp_tx_status_request(e))
                        {
                            case_new_state(e, CS_RESETTING);
                        }
                    }
                    break;

                case CS_RESETTING:
                    if (ces->valid)
                    {
                        /*
                        * The valid flag being set means that we got a response
                        * from the earbud, so the reset is complete.
                        */
                        case_new_state(e, CS_IDLE);
                    }
                    else if ((ces->give_up) || (ces->abort))
                    {
                        case_new_state(e, CS_RESET_DELAY);
                    }
                    break;

                case CS_STATUS_WANTED:
                    ces->valid = false;
                    ces->battery = 0xFF;

                    if (ccp_tx_status_request(e))
                    {
                        case_new_state(e, CS_SENT_STATUS_REQUEST);
                    }
                    break;

                case CS_SENT_STATUS_REQUEST:
                    if (ces->valid && case_battery_status.state == CASE_BATTERY_DONE)
                    {
                        if (ccp_tx_status(lid_now, chg_now, charger_is_charging(),
                            case_battery_status.current_battery_percent,
                            case_earbud_status[EARBUD_LEFT].battery,
                            case_earbud_status[EARBUD_RIGHT].battery,
                            case_earbud_status[EARBUD_LEFT].charging,
                            case_earbud_status[EARBUD_RIGHT].charging))
                        {
                            power_set_run_reason(POWER_RR_BROADCAST);
                            case_new_state(e, CS_IDLE);

                            if (e==EARBUD_LEFT)
                            {
                                case_start_status_sequence(EARBUD_RIGHT);
                            }
                        }
                    }
                    else if (ces->abort)
                    {
                        case_new_state(e, CS_STATUS_WANTED);
                    }
                    else if (ces->give_up)
                    {
                        case_new_state(e, CS_IDLE);

                        if (e==EARBUD_LEFT)
                        {
                            case_start_status_sequence(EARBUD_RIGHT);
                        }
                    }
                    break;
            }

            ces->ack = false;
            ces->abort = false;
            ces->give_up = false;
        }
    }
}

static CLI_RESULT case_cmd_status(uint8_t cmd_source __attribute__((unused)))
{
    /*
    * Initiate a status exchange and request an LED indication of the
    * battery level.
    */
    case_status_time(true);
    return CLI_OK;
}

static CLI_RESULT case_cmd_reset(uint8_t cmd_source __attribute__((unused)))
{
    long int earbud;

    if (!case_dfu_planned)
    {
        /*
        * Initiate earbud reset.
        */
        if (cli_get_next_parameter(&earbud, 10))
        {
            case_new_state(earbud, CS_RESET_WANTED);
        }
    }

    return CLI_OK;
}

CLI_RESULT case_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(case_command, cmd_source);
}
