/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger Case Protocol / Case Comms Protocol
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdio.h>
#include "main.h"
#include "wire.h"
#include "case.h"
#include "ccp.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* CCP_POLL_TIMEOUT: Time in ticks between polls.
*/
#define CCP_POLL_TIMEOUT 50

/*
* CCP_MAX_POLLS: Maximum number of polls before giving up.
*/
#define CCP_MAX_POLLS 5

#define CCP_MAX_MSG_SIZE 20

/*
* Case Comms Header field definitions.
*/
#define CCP_HDR_MASK_M       0x80
#define CCP_HDR_BIT_M        7
#define CCP_HDR_MASK_CHAN_ID 0x70
#define CCP_HDR_BIT_CHAN_ID  4
#define CCP_HDR_MASK_MSG_ID  0x0F
#define CCP_HDR_BIT_MSG_ID   0

/*
* First status byte field definitions.
*/
#define CCP_STATUS_1_MASK_CC 0x02
#define CCP_STATUS_1_BIT_CC  1
#define CCP_STATUS_1_MASK_L  0x01
#define CCP_STATUS_1_BIT_L   0

/*
* Battery status field definitions.
*/
#define CCP_BATTERY_MASK_C     0x80
#define CCP_BATTERY_BIT_C      7
#define CCP_BATTERY_MASK_LEVEL 0x7F
#define CCP_BATTERY_BIT_LEVEL  0

/*
* Reset message field definitions.
*/
#define CCP_RESET_MASK_R 0x01
#define CCP_RESET_BIT_R  0

/*
* Earbud status field definitions.
*/
#define CCP_EARBUD_STATUS_MASK_PP 0x01
#define CCP_EARBUD_STATUS_BIT_PP  0

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    bool busy;
    uint8_t poll_timeout;
    uint8_t poll_count;
}
CCP_TRANSACTION;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static void ccp_rx(uint8_t earbud, uint8_t *data, uint8_t len, bool final_piece);
static void ccp_ack(uint8_t earbud);
static void ccp_give_up(uint8_t earbud);
static void ccp_abort(uint8_t earbud);
static void ccp_broadcast_finished(void);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CCP_USER_CB *ccp_user;

static const WIRE_USER_CB ccp_wire_cb =
{
    ccp_rx,
    ccp_ack,
    ccp_give_up,
    ccp_abort,
    ccp_broadcast_finished
};

static CCP_TRANSACTION ccp_transaction[NO_OF_EARBUDS] = {0};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* Send a message to an earbud.
*/
static bool ccp_tx(
    CCP_MESSAGE msg,
    CCP_CHANNEL chan,
    WIRE_DESTINATION dest,
    uint8_t *data,
    uint8_t len,
    bool need_answer)
{
    bool ret = false;

    if ((len + 2) <= CCP_MAX_MSG_SIZE)
    {
        uint8_t buf[CCP_MAX_MSG_SIZE];

        /*
        * Case Comms Header.
        */
        buf[0] = BITMAP_SET(CCP_HDR, CHAN_ID, chan) |
                 BITMAP_SET(CCP_HDR, MSG_ID, msg);

        if (data && len)
        {
            memcpy(&buf[1], data, len);
        }

        if (dest==WIRE_DEST_BROADCAST)
        {
            ret = wire_tx(dest, buf, len+1);
        }
        else
        {
            CCP_TRANSACTION *ct =
                &ccp_transaction[(dest==WIRE_DEST_LEFT) ? EARBUD_LEFT:EARBUD_RIGHT];

            if (!ct->busy && wire_tx(dest, buf, len+1))
            {
                if (need_answer)
                {
                    ct->busy = true;
                    ct->poll_count = 0;
                    ct->poll_timeout = 0;
                }
                ret = true;
            }
        }
    }

    return ret;
}

/*
* Broadcast a short status message.
*/
bool ccp_tx_short_status(bool lid, bool charger)
{
    uint8_t buf;

    buf = BITMAP_SET(CCP_STATUS_1, L, lid) |
          BITMAP_SET(CCP_STATUS_1, CC, charger);

    return ccp_tx(
        CCP_MSG_STATUS, CCP_CH_CASE_INFO, WIRE_DEST_BROADCAST, &buf, 1, false);
}

/*
* Broadcast a complete status message.
*/
bool ccp_tx_status(
    bool lid,
    bool charger_connected,
    bool charging,
    uint8_t battery_case,
    uint8_t battery_left,
    uint8_t battery_right,
    uint8_t charging_left,
    uint8_t charging_right)
{
    uint8_t buf[4];

    buf[0] = BITMAP_SET(CCP_STATUS_1, L, lid) |
             BITMAP_SET(CCP_STATUS_1, CC, charger_connected);

    buf[1] = BITMAP_SET(CCP_BATTERY, LEVEL, battery_case) |
             BITMAP_SET(CCP_BATTERY, C, charging);

    buf[2] = BITMAP_SET(CCP_BATTERY, LEVEL, battery_left) |
             BITMAP_SET(CCP_BATTERY, C, charging_left);
    buf[3] = BITMAP_SET(CCP_BATTERY, LEVEL, battery_right) |
             BITMAP_SET(CCP_BATTERY, C, charging_right);

    return ccp_tx(
        CCP_MSG_STATUS, CCP_CH_CASE_INFO, WIRE_DEST_BROADCAST, buf, 4, false);
}

/*
* Send a status request message to the specified earbud.
*/
bool ccp_tx_status_request(uint8_t earbud)
{
    return ccp_tx(
        CCP_MSG_STATUS_REQ, CCP_CH_CASE_INFO,
        wire_dest[earbud], NULL, 0, true);
}

/*
* Send a reset message to the specified earbud.
*/
bool ccp_tx_reset(uint8_t earbud, bool factory)
{
    uint8_t buf;

    buf = BITMAP_SET(CCP_RESET, R, factory);

    return ccp_tx(
        CCP_MSG_RESET, CCP_CH_CASE_INFO, wire_dest[earbud], &buf, 1, false);
}

/*
* Receive a message from an earbud.
*/
void ccp_rx(
    uint8_t earbud,
    uint8_t *data,
    uint8_t len,
    bool final_piece)
{
    cli_tx_hex(CLI_BROADCAST, "WIRE->CCP", data, len);

    if (final_piece)
    {
        switch (BITMAP_GET(CCP_HDR, CHAN_ID, data[0]))
        {
            case CCP_CH_CASE_INFO:
                switch (BITMAP_GET(CCP_HDR, MSG_ID, data[0]))
                {
                    case CCP_MSG_EARBUD_STATUS:
                        ccp_user->rx_earbud_status(
                            earbud,
                            BITMAP_GET(CCP_EARBUD_STATUS, PP, data[1]),
                            BITMAP_GET(CCP_BATTERY, LEVEL, data[2]),
                            BITMAP_GET(CCP_BATTERY, C, data[2]));
                        break;

                    default:
                        break;
                }
                break;

            default:
                break;
        }

        ccp_transaction[earbud].busy = false;
    }
}

/*
* Receive an acknowledgement from an earbud.
*/
static void ccp_ack(uint8_t earbud)
{
    if (ccp_transaction[earbud].busy)
    {
        /*
        * We want an actual response, not just an ack. Set the timeout for the
        * next poll.
        */
        ccp_transaction[earbud].poll_timeout = CCP_POLL_TIMEOUT;
    }
    else
    {
        ccp_user->ack(earbud);
    }
}

/*
* Notification that a message could not be sent.
*/
static void ccp_give_up(uint8_t earbud)
{
    ccp_user->give_up(earbud);
    ccp_transaction[earbud].busy = false;
}

/*
* Notification that a message to be sent was aborted.
*/
static void ccp_abort(uint8_t earbud)
{
    ccp_user->abort(earbud);
    ccp_transaction[earbud].busy = false;
}

/*
* Notification that broadcasting has finished.
*/
static void ccp_broadcast_finished(void)
{
    ccp_user->broadcast_finished();
}

/*
* Send an AT command to an earbud. Note that the 'source' argument refers to
* the CLI.
*/
bool ccp_at_command(
    uint8_t cmd_source __attribute__((unused)),
    WIRE_DESTINATION dest __attribute__((unused)),
    char *at_cmd __attribute__((unused)))
{
    return false;
}

/*
* Charger Case Protocol initialisation.
*/
void ccp_init(const CCP_USER_CB *user_cb)
{
    ccp_user = user_cb;
    wire_init(&ccp_wire_cb);
}

static void ccp_manage_transaction(uint8_t earbud)
{
    CCP_TRANSACTION *ct = &ccp_transaction[earbud];

    if ((ct->busy) && (ct->poll_timeout))
    {
        /*
        * Previously we received an empty ack, but we are waiting for an
        * actual response.
        */
        if (!(--ct->poll_timeout))
        {
            if (ct->poll_count < CCP_MAX_POLLS)
            {
                /*
                * Send a poll.
                */
                if (wire_tx(wire_dest[earbud], NULL, 0))
                {
                    ct->poll_timeout = CCP_POLL_TIMEOUT;
                    ct->poll_count++;
                }
                else
                {
                    /*
                    * Poll failed, this would be because wire or
                    * charger_comms are (temporarily) busy. Increment the
                    * timeout counter so that we will try again next time
                    * round. Don't count this as one of our retries.
                    */
                    ct->poll_timeout++;
                }
            }
            else
            {
                ct->busy = false;
                ccp_give_up(earbud);
            }
        }
    }
}

/*
* Charger Case Protocol periodic function.
*/
void ccp_periodic(void)
{
    ccp_manage_transaction(EARBUD_LEFT);
    ccp_manage_transaction(EARBUD_RIGHT);
}


