/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of radio test commands for the Basic Rate / Enhanced Data Rate (BREDR) 
            in the device test service.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_service_commands_helper.h"
#include "device_test_parse.h"

#include <bdaddr.h>
#include <power_manager.h>
#include <pio_monitor.h>
#include <connection.h>

#include <stdio.h>
#include <logging.h>

#ifdef INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2

#include <test2.h>

static void rftest_bredr_task_handler(Task task, MessageId id, Message message);

static TaskData rftest_bredr_task = {.handler = rftest_bredr_task_handler };
#define RFTEST_BREDR_TASK() (&rftest_bredr_task)

typedef enum
{
    RFTEST_BREDR_INTERNAL_CARRIER_WAVE = INTERNAL_MESSAGE_BASE,
    RFTEST_BREDR_INTERNAL_TXSTART,
    RFTEST_BREDR_INTERNAL_DUTMODE,

    RFTEST_BREDR_INTERNAL_TEST_TIMEOUT,
} rftest_bredr_internal_message_t;
LOGGING_PRESERVE_MESSAGE_TYPE(rftest_bredr_internal_message_t)

/*! Timeout in milli-seconds to delay a command after sending an OK 
    response. This time should allow for an OK response to be sent
    before performing any other activity */
#define deviceTestService_CommandDelayMs()  100

/*! Number of bits in a hex digit */
#define HEXDIGIT_IN_BITS          4
#define HexDigitsForNumberOfBits(_bits)   (((_bits) + 3) / HEXDIGIT_IN_BITS)

/*! State of the RFTest portion of device test service */
static struct deviceTestServiceRfTestBredrState
{
    bool test_running;
    /*! Anonymous structure used to collate whether settings have been configured */
    struct
    {
        bool channel:1;
        bool packet:1;
        bool address:1;
        bool power:1;
        bool stop_pio:1;
        bool stop_time:1;
    } configured;
    /*! Anonymous structure used to collate settings  */
    struct
    {
        uint8 channel;
        uint8 cw_channel;
        uint16 packet_type;
        uint16 packet_payload;
        uint16 packet_length;
        uint16 test_timeout;
        uint8  lt_addr;
        uint8 power;
        uint8 test_reboot;
        uint8 test_stop_pio;
        bdaddr address;
    } configuration;
} deviceTestService_rf_test_bredr_state = {0};

/*! \brief Helper macro to see if an RF test is running */
#define RUNNING() deviceTestService_rf_test_bredr_state.test_running
/*! \brief Helper macro to access the configuration status of a named setting */
#define CONFIGURED(_field) deviceTestService_rf_test_bredr_state.configured._field
/*! \brief Helper macro to access storage for a named setting */
#define SETTING(_field) deviceTestService_rf_test_bredr_state.configuration._field


/*! Check if BREDR channel parameter is legal */
#define bredr_channel_valid(_channel) (((unsigned)(_channel)) <= 78)


static void deviceTestService_RfTestBredr_SetupForTestCompletion(void)
{
    if (CONFIGURED(stop_time))
    {
        MessageSendLater(RFTEST_BREDR_TASK(),
                         RFTEST_BREDR_INTERNAL_TEST_TIMEOUT, NULL,
                         SETTING(test_timeout));
    }
    if (CONFIGURED(stop_pio))
    {
        PioMonitorRegisterTask(RFTEST_BREDR_TASK(), SETTING(test_stop_pio));
    }
}

static void deviceTestService_RfTestBredr_TearDownOnTestCompletion(void)
{
    RUNNING() = FALSE;

    MessageCancelAll(RFTEST_BREDR_TASK(), RFTEST_BREDR_INTERNAL_TEST_TIMEOUT);

    if (CONFIGURED(stop_pio))
    {
        PioMonitorUnregisterTask(RFTEST_BREDR_TASK(), SETTING(test_stop_pio));
    }
}

static void deviceTestSerice_RfTestBredr_CarrierTest(void)
{
    bool response;

    response = Test2CwTransmit(SETTING(cw_channel), SETTING(power));
    RUNNING() = response;

    DEBUG_LOG("deviceTestSerice_RfTestBredr_CarrierTest Carrier Wave command returned %d", response);

    deviceTestService_RfTestBredr_SetupForTestCompletion();
}

static void deviceTestSerice_RfTestBredr_TxStart(void)
{
    bool response;
    HopChannels channels = { SETTING(channel),
                             SETTING(channel),
                             SETTING(channel),
                             SETTING(channel),
                             SETTING(channel)};

    response = Test2TxData(&channels, SETTING(power), FALSE,
                           SETTING(packet_payload), SETTING(packet_type), 
                           SETTING(packet_length), 
                           &SETTING(address),SETTING(lt_addr));
    RUNNING() = response;

    DEBUG_LOG("deviceTestSerice_RfTestBredr_TxStart Txdata command returned %d", response);

    deviceTestService_RfTestBredr_SetupForTestCompletion();
}

static void deviceTestSerice_RfTestBredr_DutMode(void)
{
    ConnectionEnterDutMode();

    RUNNING() = TRUE;

    DEBUG_LOG("deviceTestSerice_RfTestBredr_DutMode");

    deviceTestService_RfTestBredr_SetupForTestCompletion();
}

static void deviceTestService_TestStoppedOnTimeout(void)
{
    DEBUG_LOG_ALWAYS("deviceTestService_TestStoppedOnTimeout. Reboot:%d",
                        SETTING(test_reboot));

    if (SETTING(test_reboot))
    {
        appPowerReboot();
    }
    else
    {
        bool response;

        if (RUNNING())
        {
            response = Test2RfStop();
            DEBUG_LOG_ALWAYS("deviceTestService_TestStoppedOnTimeout - command response:%d",
                             response);
        }

        deviceTestService_RfTestBredr_TearDownOnTestCompletion();
    }
}


static void deviceTestService_CheckTestStopOnPio(const MessagePioChanged *change)
{
    bool pio_is_set;

    /* Check for unexpected message(s) and stop for the future */
    if (!RUNNING())
    {
        if (CONFIGURED(stop_pio))
        {
            PioMonitorUnregisterTask(RFTEST_BREDR_TASK(), SETTING(test_stop_pio));
        }
        return;
    }

    if (   PioMonitorIsPioInMessage(change, SETTING(test_stop_pio), &pio_is_set)
        && pio_is_set)
    {
        DEBUG_LOG_ALWAYS("deviceTestService_CheckTestStopOnPio - rebooting");

        appPowerReboot();
    }
}

/* \brief Task handler for RF testing

    This is required for two purposes
    1) delaying the start of a test so that an OK response has a chance to arrive
    2) timeout for the end of a test step

    \param[in]  task    The task the message was sent to
    \param      id      The message identifier
    \param[in]  message The message content
 */
static void rftest_bredr_task_handler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_FN_ENTRY("rftest_bredr_task_handler MESSAGE:rftest_bredr_internal_message_t:0x%x", id);

    switch(id)
    {
        case RFTEST_BREDR_INTERNAL_CARRIER_WAVE:
            deviceTestSerice_RfTestBredr_CarrierTest();
            break;

        case RFTEST_BREDR_INTERNAL_TXSTART:
            deviceTestSerice_RfTestBredr_TxStart();
            break;

        case RFTEST_BREDR_INTERNAL_DUTMODE:
            deviceTestSerice_RfTestBredr_DutMode();
            break;

        case RFTEST_BREDR_INTERNAL_TEST_TIMEOUT:
            deviceTestService_TestStoppedOnTimeout();
            break;

        case MESSAGE_PIO_CHANGED:
            deviceTestService_CheckTestStopOnPio((const MessagePioChanged *)message);
            break;

        default:
            break;
    }
}


static bool deviceTestServiceRfTestBredrConfigured(void)
{
    return    CONFIGURED(channel)
           && CONFIGURED(packet)
           && CONFIGURED(address)
           && CONFIGURED(power);
}

/*! \brief Command handler for AT + RFTESTSTOP

    The function decides if the commands is allowed and if so stops any RF test 
    that is in progress. It will send an OK response even if there is no 
    RF testing in progress.

    \note If the command has been issued from a test interface using a radio
    then the connection will be terminated by this command. It will be 
    re-established when the test has completed.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleRfTestStop(Task task)
{
    bool response;

    if (!DeviceTestService_CommandsAllowed())
    {
        DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestStop. Disallowed");

        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* The Stop command also destroys the current connection (if any)
       so can't call the stop command if not running */
    if (RUNNING())
    {
        response = Test2RfStop();
        DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestStop - command response:%d",
                         response);

        deviceTestService_RfTestBredr_TearDownOnTestCompletion();
    }
    DeviceTestService_CommandResponseOk(task);
}


/*! \brief Command handler for AT+RFTESTCARRIER = %d:channel 

    The function decides if the command is allowed and if so, starts transmitting
    a fixed carrier signal.

    An OK response is sent so long as the carrier command is allowed.

    \note If the command has been issued from a test interface using a radio
    then the connection will be terminated by this command. It will be 
    re-established when the test has completed.

    \param[in] task The task to be used in command responses
    \param[in] carrier_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCarrier(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCarrier *carrier_params)
{
    uint16 channel_setting = carrier_params->channel;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCarrier. Channel:%d",
                      channel_setting);

    if (   !DeviceTestService_CommandsAllowed()
        || !bredr_channel_valid(channel_setting)
        || !CONFIGURED(power))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    SETTING(cw_channel) = channel_setting;

    /* Delay command action to allow OK to be sent */
    MessageSendLater(RFTEST_BREDR_TASK(),
                     RFTEST_BREDR_INTERNAL_CARRIER_WAVE, NULL,
                     deviceTestService_CommandDelayMs());

    DeviceTestService_CommandResponseOk(task);
}


/*! \brief Command handler for AT+RFTESTTXSTART

    The function decides if the command is allowed \b and \b has \b been
    \b configured and if so starts a test transmission.

    \note If the command has been issued from a test interface using a radio
    then the connection will be terminated by this command. It will be 
    re-established when the test has completed.

    \param[in] task The task to be used in command responses
    \param[in] start_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestTxStart(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestTxStart");

    if (   !DeviceTestService_CommandsAllowed()
        || !deviceTestServiceRfTestBredrConfigured())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* Delay command action to allow OK to be sent */
    MessageSendLater(RFTEST_BREDR_TASK(),
                     RFTEST_BREDR_INTERNAL_TXSTART, NULL,
                     deviceTestService_CommandDelayMs());

    DeviceTestService_CommandResponseOk(task);
}


/*! \brief Command handler for AT+RFTESTCFGCHANNEL = %d:channel 

    The function decides if the command is allowed and if so saves the 
    channel, ready for the AT+RFTESTTXSTART command.

    \param[in] task The task to be used in command responses
    \param[in] channel_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCfgChannel(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgChannel *channel_params)
{
    uint16 channel_setting = channel_params->channel;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgChannel %d", channel_setting);

    CONFIGURED(channel) = FALSE;

    if (   !DeviceTestService_CommandsAllowed()
        || !bredr_channel_valid(channel_setting))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    CONFIGURED(channel) = TRUE;
    SETTING(channel) = channel_setting;

    DeviceTestService_CommandResponseOk(task);
}

static bool deviceTestService_extractHex(struct sequence *incoming_string, unsigned max_length_bits, uint32 *result)
{
    unsigned max_length_hex_digits = HexDigitsForNumberOfBits(max_length_bits);
    unsigned length = incoming_string->length;
    uint32 value = 0;
    const uint8 *hex_digits = incoming_string->data;

    *result = (uint32)-1;   /* Prepopulate with an error value */

    if (length == 0)
    {
        return FALSE;
    }

    while (length && max_length_hex_digits)
    {
        value <<= HEXDIGIT_IN_BITS;
        value = value + deviceTestService_HexToNumber(*hex_digits);

        hex_digits++;
        length--;
        max_length_hex_digits--;
    }

    *result = value;
    return TRUE;
}

/*! \brief Helper function to read a fixed length hex field from a hex string, based on an
    'struct sequence' supplied as AT command parameters.

    The sequence is updated based on characters read.

    \param[in,out] incoming_string Details of character sequence. The sequence is updated on
                    exit to refer to any remaining characters
    \param characters The number of characters to consume from the string
    \param[out] The value retrievd from the string

    \return TRUE if sufficient characters were retrieved, FALSE otherwise
 */
static bool deviceTestService_extractHexField(struct sequence *incoming_string, unsigned characters, uint32 *result)
{
    uint16 original_length = incoming_string->length;

    if (characters > original_length)
    {
        return FALSE;
    }

    incoming_string->length = characters;
    bool converted = deviceTestService_extractHex(incoming_string, characters * HEXDIGIT_IN_BITS, result);
    if (converted)
    {
        incoming_string->data += characters;
        incoming_string->length = original_length - characters;
    }
    return converted;
}

static bool deviceTestService_extractBdaddr(struct sequence *string, bdaddr *address)
{
    uint32 decoded;

    /* Zero bdaddr here to sanitise any unused bytes */
    BdaddrSetZero(address);

    /*! Extract bluetooth address, represented as a single string consisting of
        NAP(Non-Significant Address Part), UAP (Upper Address Part), 
        LAP (Lower Address part) arranged as follows 
                NNNNUULLLLLL
     */
    if (deviceTestService_extractHexField(string, 4, &decoded))
    {
        address->nap = decoded;
        if (deviceTestService_extractHexField(string, 2, &decoded))
        {
            address->uap = decoded;
            if (deviceTestService_extractHexField(string, 6, &decoded))
            {
                address->lap = decoded;

                /* Make sure all characters consumed */
                if (string->length == 0)
                {
                    return TRUE;
                }
            }
        }
    }

    /* And zero here to ensure error value */
    BdaddrSetZero(address);
    return FALSE;
}


/*! \brief Command handler for AT+RFTESTCFGPACKET=0x%hexdigit+:payload, 0x%hexdigit+:packetType, %d:length 

    The function decides if the command is allowed and if so saves the 
    packet type supplied, ready for the AT+RFTESTTXSTART command.

                  0x00 NULL packets
                  0x01 POLL packets
                  0x02 FHS packets
                  0x03 DM1 packets
                  0x04 DH1 packets
                  0x0A DM3 packets
                  0x0B DH3 packets
                  0x0E DM5 packets
                  0x0F DH5 packets
                  0x09 AUX1 packets
                  0x24 2-DH1 packets
                  0x2A 2-DH3 packets
                  0x2E 2-DH5 packets
                  0x28 3-DH1 packets
                  0x2B 3-DH3 packets
                  0x2F 3-DH5 packets
                  0x05 HV1 packets
                  0x06 HV2 packets
                  0x07 HV3 packets
                  0x08 DV packets
                  0x17 EV3 packets
                  0x1C EV4 packets
                  0x1D EV5 packets
                  0x36 2-EV3 packets
                  0x3C 2-EV5 packets
                  0x37 3-EV3 packets
                  0x3D 3-EV5 packets  <== Largest value

    \param[in] task The task to be used in command responses
    \param[in] start_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCfgPacket(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgPacket *packet_params)
{
    uint32 payload;
    uint32 packet_type;
    uint16 length;

    bool payload_hex_ok = deviceTestService_extractHex(&packet_params->payload, 16, &payload);
    bool packet_hex_ok = deviceTestService_extractHex(&packet_params->packetType, 16, &packet_type);
    length = packet_params->length;

    CONFIGURED(packet) = FALSE;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgPacket. payload:0x%x, type:0x%x, length:%d",
                     payload,
                     packet_type,
                     length);

    if (!DeviceTestService_CommandsAllowed()
        || !payload_hex_ok
        || !packet_hex_ok
        || !(payload <= 4)
        || !(packet_type <= 0x3D)
        || !(length <= 1021))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    CONFIGURED(packet) = TRUE;
    SETTING(packet_type) = packet_type;
    SETTING(packet_payload) = payload;
    SETTING(packet_length) = length;

    DeviceTestService_CommandResponseOk(task);
}


/*! \brief Command handler for AT+RFTESTCFGADDRESS=%d:logicalAddr, %hexdigit+:bdaddr 

    Decide if the command is allowed and if so save the supplied bluetooth
    device address ready for the AT+RFTESTTXSTART command.

    An ERROR response will be sent if the address has an incorrect format.

    \param[in] task The task to be used in command responses
    \param[in] address_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCfgAddress(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgAddress *address_params)
{
    unsigned logical_addr = address_params->logicalAddr;
    bdaddr address;

    bool bdaddr_ok = deviceTestService_extractBdaddr(&address_params->bdaddr, &address);

    CONFIGURED(address) = FALSE;

    if (   !DeviceTestService_CommandsAllowed()
        || !(logical_addr <= 7)
        || !bdaddr_ok)
    {
        DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgAddress. Lt_Addr:%d Valid bdaddr:%d",
                            logical_addr, bdaddr_ok);

        DeviceTestService_CommandResponseError(task);
        return;
    }

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgAddress. Lt_Addr:%d bdaddr:%04X%02X%06lX",
                        logical_addr,
                        address.nap, address.uap, address.lap);

    CONFIGURED(address) = TRUE;
    SETTING(lt_addr) = logical_addr;
    SETTING(address) = address;

    DeviceTestService_CommandResponseOk(task);
}


/*! \brief Command handler for AT+RFTESTCFGPOWER=%d:powerSetting 

    Decide if the command is allowed and if so save the supplied power 
    setting ready for the AT+RFTESTTXSTART command.

    An ERROR response will be sent if the power setting is not supported.

    \param[in] task The task to be used in command responses
    \param[in] power_params Parameters from the command supplied by the AT command parser.
                    Unfortunately not 'parameters of power'
 */
void DeviceTestServiceCommand_HandleRfTestCfgPower(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgPower *power_params)
{
    uint16 power = power_params->powerSetting;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgPower. Power:%d", power);

    CONFIGURED(power) = FALSE;

    if (   !DeviceTestService_CommandsAllowed()
        || !(power <= 20))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    CONFIGURED(power) = TRUE;
    SETTING(power) = power;

    DeviceTestService_CommandResponseOk(task);
}


/*! \brief Command handler for AT+RFTESTCFGSTOPTIME=%d:reboot, %d:timeMs 

    Decide if the command is allowed and save the stop time ready for use
    by subsequent test commands. 

    An ERROR response is sent if the command is not allowed, or the time
    parameter is illegal, otherwise an OK response is sent.

    \param[in] task The task to be used in command responses
    \param[in] stoptime_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCfgStopTime(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgStopTime *stoptime_params)
{
    uint16 reboot = stoptime_params->reboot;
    uint16 test_timeout = stoptime_params->timeMs;
    
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgStopTime reboot:%d,time:%dms",
                        reboot, test_timeout);

    CONFIGURED(stop_time) = FALSE;

    if (   !DeviceTestService_CommandsAllowed()
        || !(reboot<=1))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    CONFIGURED(stop_time) = TRUE;
    SETTING(test_reboot) = reboot;
    SETTING(test_timeout) = test_timeout;

    DeviceTestService_CommandResponseOk(task);
}


/*! \brief Command handler for AT+RFTESTCFGSTOPPIO=%d:pio 

    Decide if the command is allowed and save the supplied pio number ready
    for use by subsequent test commands.

    An ERROR response is sent if the command is not allowed, or the time
    parameter is notsupported, otherwise an OK response is sent.

    \param[in] task The task to be used in command responses
    \param[in] stoppio_params Parameters from the command supplied by the AT command parser
 */
void DeviceTestServiceCommand_HandleRfTestCfgStopPio(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgStopPio *stoppio_params)
{
    uint16 pio = stoppio_params->pio;

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTestCfgStopPio. pio:%d", pio);

    CONFIGURED(stop_pio) = FALSE;

    if (   !DeviceTestService_CommandsAllowed()
        || !(pio <= 95))
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    CONFIGURED(stop_pio) = TRUE;
    SETTING(test_stop_pio) = pio;

    DeviceTestService_CommandResponseOk(task);
}

/*! \brief Command handler for AT+RFTESTDUTMODE

    The function decides if the command is allowed and if so 
    places the device into Device Under Test mode.

    This command will make use of the configuration for stopping on a
    PIO or timeout, but does not require them. The difference is due to
    the use cases for Device Under Test mode, Qualification testing,
    being different from production/factory test.

    \note If the command has been issued from a test interface using a radio
    then the connection will be terminated by this command. It will be 
    re-established when the test has completed.

    \param[in] task The task to be used in command responses
 */
void DeviceTestServiceCommand_HandleDeviceUnderTestMode(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleDeviceUnderTestMode");

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* Delay command action to allow OK to be sent */
    MessageSendLater(RFTEST_BREDR_TASK(),
                     RFTEST_BREDR_INTERNAL_DUTMODE, NULL,
                     deviceTestService_CommandDelayMs());

    DeviceTestService_CommandResponseOk(task);
}

/*! @} End of group documentation */

#else /* !INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2 */

/* Include stubs of all commands */

static void DeviceTestServiceCommand_HandleRfTest(Task task)
{
    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandleRfTest. RF Test Commands not supported");

    DeviceTestService_CommandResponseError(task);
}

void DeviceTestServiceCommand_HandleRfTestStop(Task task)
{
    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleRfTestCarrier(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCarrier *carrier_params)
{
    UNUSED(carrier_params);

    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleRfTestTxStart(Task task)
{
    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleRfTestCfgChannel(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgChannel *channel_params)
{
    UNUSED(channel_params);

    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleRfTestCfgPacket(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgPacket *packet_params)
{
    UNUSED(packet_params);

    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleRfTestCfgAddress(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgAddress *address_params)
{
    UNUSED(address_params);

    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleRfTestCfgPower(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgPower *power_params)
{
    UNUSED(power_params);

    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleRfTestCfgStopTime(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgStopTime *stoptime_params)
{
    UNUSED(stoptime_params);

    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleRfTestCfgStopPio(Task task,
                         const struct DeviceTestServiceCommand_HandleRfTestCfgStopPio *stoppio_params)
{
    UNUSED(stoppio_params);

    DeviceTestServiceCommand_HandleRfTest(task);
}

void DeviceTestServiceCommand_HandleDeviceUnderTestMode(Task task)
{
    /* It is possible to support DUT mode on older devices... 
       ...but requires additional support code. 
       Leaving disabled unless actively requested */
    DeviceTestServiceCommand_HandleRfTest(task);
}

#endif /* INCLUDE_DEVICE_TEST_SERVICE_RADIOTEST_V2 */
