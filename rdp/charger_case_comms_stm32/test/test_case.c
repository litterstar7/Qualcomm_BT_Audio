/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for case.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>

#include "unity.h"

#include "case.c"
#include "cli_txf.c"

#include "mock_gpio.h"
#include "mock_adc.h"
#include "mock_power.h"
#include "mock_cli.h"
#include "mock_ccp.h"
#include "mock_charger.h"
#include "mock_led.h"

/*-----------------------------------------------------------------------------
------------------ DO FUNCTIONS -----------------------------------------------
-----------------------------------------------------------------------------*/

void do_battery_read(uint16_t vbat, uint16_t vref, uint8_t percentage, bool led)
{
    int n;

    gpio_enable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
    case_battery_status_periodic();

    for (n=0; n<CASE_BATTERY_READ_DELAY_TIME; n++)
    {
        case_battery_status_periodic();
    }

    adc_start_measuring_Expect();
    case_battery_status_periodic();

    for (n=0; n<CASE_BATTERY_ADC_DELAY_TIME; n++)
    {
        case_battery_status_periodic();
    }

    adc_read_ExpectAndReturn(ADC_VBAT, vbat);
    adc_read_ExpectAndReturn(ADC_VREF, vref);
    gpio_disable_Expect(GPIO_VBAT_MONITOR_ON_OFF);
    if (led)
    {
        led_indicate_battery_Expect(percentage);
    }
    case_battery_status_periodic();

    TEST_ASSERT_EQUAL(percentage, case_battery_status.current_battery_percent);
}

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
    lid_now = false;
    lid_before = false;
    chg_now = false;
    chg_before = false;
    case_event = true;
    case_dfu_planned = false;
    memset(case_earbud_status, 0, sizeof(case_earbud_status));
    memset(&case_battery_status, 0, sizeof(case_battery_status));
    
    ccp_init_Ignore();
    case_init();
    
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(false);
    power_clear_run_reason_Expect(POWER_RR_CASE_EVENT);
    case_periodic();    
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

void test_case_lid_open(void)
{
    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Lid is opened, interrupt occurs.
    */
    power_set_run_reason_Expect(POWER_RR_CASE_EVENT);    
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed. The lid being
    * opened causes us to send a short status message immediately, read the
    * battery and start a status message exchange.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, true);
    charger_connected_ExpectAndReturn(false);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    ccp_tx_short_status_ExpectAndReturn(true, false, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_CASE_EVENT);
    case_periodic();

    /*
    * Battery is read and its level reported to the LED module.
    */
    do_battery_read(0x0F99, 0x05AB, 100, true);

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Request status of left earbud.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0x21, 1);

    /*
    * Broadcast status message. Attempt to request right earbud status will
    * fail because of the broadcast message being in progress.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(true, false, false, 0x64, 0x21, 0xFF, 0x01, 0x00, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Request status of right earbud.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0x2B, 1);

    /*
    * Broadcast status message.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(true, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Nothing happens.
    */
    case_periodic();
}

void test_case_status(void)
{
    /*
    * It's time to exchange status information.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_status_time(false);
    do_battery_read(0x0F99, 0x05AB, 100, false);

    /*
    * First attempt to request status fails.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, false);
    case_periodic();

    /*
    * Second attempt is successful.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0x21, 1);

    /*
    * Broadcast status message. Attempt to request right earbud status will
    * fail because of the broadcast message being in progress.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, 0x64, 0x21, 0xFF, 0x01, 0x00, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Request status of right earbud.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0x2B, 1);

    /*
    * Attempt to broadcast status message fails.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, false);
    case_periodic();

    /*
    * Broadcast status message.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Nothing happens.
    */
    case_periodic();
}

/*
* Earbuds fail to respond to status requests.
*/
void test_case_status_no_response(void)
{
    /*
    * It's time to exchange status information.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_status_time(false);
    do_battery_read(0x0F99, 0x05AB, 100, false);

    /*
    * Send status request message to left earbud.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_STATUS_REQUEST, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Earbud hasn't responded.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Give up (0)");
    case_give_up(EARBUD_LEFT);

    /*
    * Left earbud goes back to idle state, send status request message to
    * right earbud.
    */
    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);
    TEST_ASSERT_EQUAL(CS_SENT_STATUS_REQUEST, case_earbud_status[EARBUD_RIGHT].state);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Earbud hasn't responded.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Give up (1)");
    case_give_up(EARBUD_RIGHT);

    /*
    * Right earbud goes back to idle state.
    */
    power_clear_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_RIGHT].state);
}

/*
* Broadcast message interrupts status.
*/
void test_case_broadcast_interrupts_status(void)
{
    /*
    * It's time to exchange status information.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_status_time(false);
    do_battery_read(0x0936, 0x0600, 50, false);

    /*
    * Send status request message to left earbud.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_STATUS_REQUEST, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Charger is connected interrupt occurs.
    */
    power_set_run_reason_Expect(POWER_RR_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
    ccp_tx_short_status_ExpectAndReturn(false, true, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_CASE_EVENT);
    case_periodic();

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (0)");
    case_abort(EARBUD_LEFT);

    /*
    * Go back to the STATUS_WANTED state.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_STATUS_WANTED, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Attempt to send status request message to left earbud, but it is rejected
    * because the broadcast is in progress.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, false);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_STATUS_WANTED, case_earbud_status[EARBUD_LEFT].state);

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Now that the broadcast is completed, send status request message to left
    * earbud.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_STATUS_REQUEST, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0x21, 1);

    /*
    * Broadcast status message. Attempt to request right earbud status will
    * fail because of the broadcast message being in progress.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, true, false, 0x32, 0x21, 0xFF, 0x01, 0x00, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Request status of right earbud.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0x2B, 1);

    /*
    * Broadcast status message.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, true, false, 0x32, 0x21, 0x2B, 0x01, 0x01, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Nothing happens.
    */
    case_periodic();
}

/*
* Factory reset sequence.
*/
void test_case_factory_reset(void)
{
    uint8_t n;

    /*
    * Initiate a factory reset. How this will be done in reality is still to
    * decided.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_new_state(EARBUD_LEFT, CS_RESET_WANTED);

    /*
    * First attempt to send the reset message fails.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, false);

    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_WANTED, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Next time around, we do successfully send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud ACKs the reset message.
    */
    case_ack(EARBUD_LEFT);

    /*
    * The case acts on the ACK, and moves to the delay state.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Try to poll the earbud (using status request), but the attempt fails.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, false);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Next time round the poll is successful.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud hasn't responded.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Give up (0)");
    case_give_up(EARBUD_LEFT);

    /*
    * Move to the RESET_DELAY state, to eventually trigger a retry.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll again.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0x21, 1);

    /*
    * Reset complete, go back to idle state.
    */
    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);
}

/*
* No response from earbud when attempting factory reset.
*/
void test_case_factory_reset_no_response(void)
{
    uint8_t n;

    /*
    * Initiate a factory reset. How this will be done in reality is still to
    * decided.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_new_state(EARBUD_LEFT, CS_RESET_WANTED);

    /*
    * Send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud hasn't responded.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Give up (0)");
    case_give_up(EARBUD_LEFT);

    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);
}

/*
* Factory reset sequence is interrupted by broadcast message.
*/
void test_case_broadcast_interrupts_factory_reset_1(void)
{
    uint8_t n;

    /*
    * Initiate a factory reset. How this will be done in reality is still to
    * decided.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_new_state(EARBUD_LEFT, CS_RESET_WANTED);

    /*
    * Send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Charger is connected, interrupt occurs.
    */
    power_set_run_reason_Expect(POWER_RR_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
    ccp_tx_short_status_ExpectAndReturn(false, true, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_CASE_EVENT);
    case_periodic();

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (0)");
    case_abort(EARBUD_LEFT);

    /*
    * Go to the RESET_WANTED state.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_WANTED, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Attempt to send the reset message, but it is rejected because the
    * broadcast is in progress.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, false);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_WANTED, case_earbud_status[EARBUD_LEFT].state);

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Next time around, we do successfully send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud ACKs the reset message.
    */
    case_ack(EARBUD_LEFT);

    /*
    * The case acts on the ACK, and moves to the delay state.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll the earbud (using status request).
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0x21, 1);

    /*
    * Reset complete, go back to idle state.
    */
    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);
}

/*
* Factory reset sequence is interrupted by broadcast message.
*/
void test_case_broadcast_interrupts_factory_reset_2(void)
{
    uint8_t n;

    /*
    * Initiate a factory reset. How this will be done in reality is still to
    * decided.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_new_state(EARBUD_LEFT, CS_RESET_WANTED);

    /*
    * Send the reset message.
    */
    ccp_tx_reset_ExpectAndReturn(EARBUD_LEFT, true, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_SENT_RESET, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Earbud ACKs the reset message.
    */
    case_ack(EARBUD_LEFT);

    /*
    * The case acts on the ACK, and moves to the delay state.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll the earbud (using status request).
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Charger is connected, interrupt occurs.
    */
    power_set_run_reason_Expect(POWER_RR_CASE_EVENT);
    case_event_occurred();

    /*
    * We read the GPIO pins and detect that things have changed, so a short
    * status message is sent.
    */
    gpio_active_ExpectAndReturn(GPIO_MAG_SENSOR, false);
    charger_connected_ExpectAndReturn(true);
    ccp_tx_short_status_ExpectAndReturn(false, true, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_CASE_EVENT);
    case_periodic();

    /*
    * Notification of abort because of the broadcast.
    */
    cli_tx_Expect(CLI_BROADCAST, true, "Abort (0)");
    case_abort(EARBUD_LEFT);

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Move to the RESET_DELAY state, to eventually trigger a retry.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens for a bit.
    */
    for (n=0; n<CASE_RESET_DELAY_TIME; n++)
    {
        case_periodic();
        TEST_ASSERT_EQUAL(CS_RESET_DELAY, case_earbud_status[EARBUD_LEFT].state);
    }

    /*
    * Poll the earbud (using status request).
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Nothing happens.
    */
    case_periodic();
    TEST_ASSERT_EQUAL(CS_RESETTING, case_earbud_status[EARBUD_LEFT].state);

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0x21, 1);

    /*
    * Reset complete, go back to idle state.
    */
    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();
    TEST_ASSERT_EQUAL(CS_IDLE, case_earbud_status[EARBUD_LEFT].state);
}

/*
* Battery percentage calculation.
*/
void test_case_battery_percentage(void)
{
    adc_read_ExpectAndReturn(ADC_VBAT, 0x0FFF);
    adc_read_ExpectAndReturn(ADC_VREF, 0x0600);
    TEST_ASSERT_EQUAL(100, case_battery_percentage());

    adc_read_ExpectAndReturn(ADC_VBAT, 0x0A2E);
    adc_read_ExpectAndReturn(ADC_VREF, 0x0600);
    TEST_ASSERT_EQUAL(100, case_battery_percentage());

    adc_read_ExpectAndReturn(ADC_VBAT, 0x0936);
    adc_read_ExpectAndReturn(ADC_VREF, 0x0600);
    TEST_ASSERT_EQUAL(50, case_battery_percentage());

    adc_read_ExpectAndReturn(ADC_VBAT, 0x083E);
    adc_read_ExpectAndReturn(ADC_VREF, 0x0600);
    TEST_ASSERT_EQUAL(0, case_battery_percentage());

    adc_read_ExpectAndReturn(ADC_VBAT, 0x0000);
    adc_read_ExpectAndReturn(ADC_VREF, 0x0600);
    TEST_ASSERT_EQUAL(0, case_battery_percentage());

    adc_read_ExpectAndReturn(ADC_VBAT, 0x0FFF);
    adc_read_ExpectAndReturn(ADC_VREF, 0x0000);
    TEST_ASSERT_EQUAL(100, case_battery_percentage());
}

/*
* Firmware updates occur, disturbing the periodic status information exchange.
*/
void test_case_interrupted_by_dfu(void)
{
    /*
    * We allow a DFU to take place, because nothing is happening.
    */
    TEST_ASSERT_TRUE(case_allow_dfu());

    /*
    * It's time to exchange status information, but we don't because a DFU
    * is in progress.
    */
    case_status_time(false);
    TEST_ASSERT_TRUE(case_dfu_planned);

    /*
    * DFU is finished (ie failed, because when successful we reset).
    */
    case_dfu_finished();
    TEST_ASSERT_FALSE(case_dfu_planned);

    /*
    * It's time to exchange status information.
    */
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_status_time(false);
    do_battery_read(0x0F99, 0x05AB, 100, false);

    /*
    * Do not allow the requested DFU, because the left earbud is not in the
    * idle state.
    */
    TEST_ASSERT_FALSE(case_allow_dfu());
    TEST_ASSERT_TRUE(case_dfu_planned);

    /*
    * First attempt to request status fails.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, false);
    case_periodic();

    /*
    * Second attempt is successful.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_LEFT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_L);
    case_periodic();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_LEFT, 0, 0x21, 1);

    /*
    * Broadcast status message. Attempt to request right earbud status will
    * fail because of the broadcast message being in progress.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, 0x64, 0x21, 0xFF, 0x01, 0x00, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_STATUS_L);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, false);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Request status of right earbud.
    */
    ccp_tx_status_request_ExpectAndReturn(EARBUD_RIGHT, true);
    power_set_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();

    /*
    * Do not allow the requested DFU, because the right earbud is not in the
    * idle state.
    */
    TEST_ASSERT_FALSE(case_allow_dfu());
    TEST_ASSERT_TRUE(case_dfu_planned);

    /*
    * Response from earbud received.
    */
    case_rx_earbud_status(EARBUD_RIGHT, 0, 0x2B, 1);

    /*
    * Attempt to broadcast status message fails.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, false);
    case_periodic();

    /*
    * Broadcast status message.
    */
    charger_is_charging_ExpectAndReturn(false);
    ccp_tx_status_ExpectAndReturn(false, false, false, 0x64, 0x21, 0x2B, 0x01, 0x01, true);
    power_set_run_reason_Expect(POWER_RR_BROADCAST);
    power_clear_run_reason_Expect(POWER_RR_STATUS_R);
    case_periodic();

    /*
    * We are informed that the broadcast of the status message is completed.
    */
    power_clear_run_reason_Expect(POWER_RR_BROADCAST);
    case_broadcast_finished();

    /*
    * Nothing happens.
    */
    case_periodic();

    /*
    * Now we allow the DFU to take place.
    */
    TEST_ASSERT_TRUE(case_allow_dfu());

    /*
    * DFU is finished.
    */
    case_dfu_finished();
    TEST_ASSERT_FALSE(case_dfu_planned);
}
