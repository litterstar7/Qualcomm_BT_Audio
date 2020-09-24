/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger Case Protocol
*/

#ifndef CCP_H_
#define CCP_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "wire.h"

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CCP_MSG_STATUS        = 0,
    CCP_MSG_EARBUD_STATUS = 1,
    CCP_MSG_RESET         = 2,
    CCP_MSG_STATUS_REQ    = 3
}
CCP_MESSAGE;

typedef enum
{
    CCP_CH_CASE_INFO = 0,
    CCP_CH_DTS       = 1
}
CCP_CHANNEL;

typedef struct
{
    void (*rx_earbud_status)(uint8_t earbud, uint8_t pp, uint8_t battery, uint8_t charging);
    void (*ack)(uint8_t earbud);
    void (*give_up)(uint8_t earbud);
    void (*abort)(uint8_t earbud);
    void (*broadcast_finished)(void);
}
CCP_USER_CB;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void ccp_init(const CCP_USER_CB *user_cb);
void ccp_periodic(void);
bool ccp_tx_short_status(bool lid, bool charger);

bool ccp_tx_status(
    bool lid,
    bool charger_connected,
    bool charging,
    uint8_t battery_case,
    uint8_t battery_left,
    uint8_t battery_right,
    uint8_t charging_left,
    uint8_t charging_right);

bool ccp_tx_status_request(uint8_t earbud);
bool ccp_tx_reset(uint8_t earbud, bool factory);
bool ccp_at_command(uint8_t cli_source, WIRE_DESTINATION dest, char *at_cmd);

#endif /* CCP_H_ */
