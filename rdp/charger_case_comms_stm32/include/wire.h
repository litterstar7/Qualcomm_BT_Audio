/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Wire protocol
*/

#ifndef WIRE_H_
#define WIRE_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "earbud.h"

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    WIRE_DEST_CASE      = 0,
    WIRE_DEST_RIGHT     = 1,
    WIRE_DEST_LEFT      = 2,
    WIRE_DEST_BROADCAST = 3
}
WIRE_DESTINATION;

typedef struct
{
    void (*rx)(uint8_t earbud, uint8_t *data, uint8_t len, bool final_piece);
    void (*ack)(uint8_t earbud);
    void (*give_up)(uint8_t earbud);
    void (*abort)(uint8_t earbud);
    void (*broadcast_finished)(void);
}
WIRE_USER_CB;

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

extern const uint8_t wire_dest[NO_OF_EARBUDS];

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void wire_init(const WIRE_USER_CB *user_cb);
void wire_periodic(void);
bool wire_tx(WIRE_DESTINATION dest, uint8_t *data, uint8_t len);
void wire_rx(uint8_t earbud, uint8_t *data, uint8_t len);
uint8_t wire_checksum(uint8_t *data, uint8_t len);

#endif /* WIRE_H_ */
