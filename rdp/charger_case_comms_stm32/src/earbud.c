/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Earbud
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "power.h"
#include "ccp.h"
#include "wire.h"
#include "earbud.h"

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    uint8_t sn;
    uint8_t nesn;
    uint8_t pattern_ctr;
    uint16_t nack_pattern;
    uint16_t corrupt_pattern;
}
EARBUD_INFO;

/*-----------------------------------------------------------------------------
------------------ PROTOYPES --------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT earbud_cmd_nack(uint8_t cmd_source);
static CLI_RESULT earbud_cmd_corrupt(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static EARBUD_INFO earbud_info[NO_OF_EARBUDS] = {0};

static const CLI_COMMAND earbud_command[] =
{
    { "nack",    earbud_cmd_nack,    2 },
    { "corrupt", earbud_cmd_corrupt, 2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void earbud_rx(uint8_t *buf, uint8_t buf_len)
{
    WIRE_DESTINATION dest = (WIRE_DESTINATION)((buf[0] & 0x30) >> 4);

    if (dest == WIRE_DEST_BROADCAST)
    {
        earbud_info[EARBUD_LEFT].sn = 0;
        earbud_info[EARBUD_LEFT].nesn = 0;
        earbud_info[EARBUD_RIGHT].sn = 0;
        earbud_info[EARBUD_RIGHT].nesn = 0;
    }
    else
    {
        uint8_t earbud = (dest==WIRE_DEST_LEFT) ? EARBUD_LEFT:EARBUD_RIGHT;
        EARBUD_INFO *info = &earbud_info[earbud];
        uint8_t rbuf[10];
        uint8_t len = 1;
        uint8_t pkt_nesn = (buf[0] & 0x40) ? 1:0;
        uint8_t pkt_sn = (buf[0] & 0x80) ? 1:0;
        static uint8_t rsp_later = 0;

        if ((1 << info->pattern_ctr) & info->nack_pattern)
        {
            /*
            * Signal a NACK by not toggling NESN.
            */
            PRINT_B("NACK response");
        }
        else
        {
            if (pkt_sn == info->nesn)
            {
                info->nesn = (info->nesn) ? 0:1;
            }

            if (pkt_nesn != info->sn)
            {
                info->sn = (info->sn) ?  0:1;
            }

            if (buf_len < 3)
            {
                if (rsp_later)
                {
                    rsp_later--;
                    if (!rsp_later)
                    {
                        rbuf[1] = 0x01;
                        rbuf[2] = 0x00;
                        rbuf[3] = (uint8_t)(rand() % 100);
                        len = 4;
                    }
                }
                else
                {
                    len = 0;
                }
            }
            else
            {
                CCP_MESSAGE msg = (CCP_MESSAGE)(buf[1] & 0xF);

                if (msg==CCP_MSG_STATUS_REQ)
                {
                    rsp_later = 3;
                }
            }
        }

        if (len)
        {
            rbuf[0] = len;

            if (info->sn)
            {
                rbuf[0] |= 0x80;
            }

            if (info->nesn)
            {
                rbuf[0] |= 0x40;
            }
            rbuf[len] = wire_checksum(rbuf, len);

            if ((1 << info->pattern_ctr) & info->corrupt_pattern)
            {
                /*
                * Corrupt the data so that it will be rejected due to a
                * checksum mismatch.
                */
                PRINT_B("Corrupt response");
                rbuf[0] ^= 0xFF;
            }
            wire_rx(earbud, rbuf, len+1);
        }

        info->pattern_ctr = (info->pattern_ctr + 1) & 0xF;
    }
    power_clear_run_reason(POWER_RR_CHARGER_COMMS);
}

static bool earbud_cli_choice(uint8_t *earbud)
{
    bool ret = false;
    char *tok = cli_get_next_token();

    if (tok)
    {
        if (!strcasecmp(tok, "L"))
        {
            *earbud = EARBUD_LEFT;
            ret = true;
        }
        else if (!strcasecmp(tok, "R"))
        {
            *earbud = EARBUD_RIGHT;
            ret = true;
        }
    }

    return ret;
}

static CLI_RESULT earbud_cmd_nack(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint8_t earbud;

    if (earbud_cli_choice(&earbud))
    {
        long int x;

        if (cli_get_next_parameter(&x, 16))
        {
            earbud_info[earbud].nack_pattern = (uint16_t)x;
            ret = CLI_OK;
        }
    }

    return ret;
}

static CLI_RESULT earbud_cmd_corrupt(uint8_t cmd_source __attribute__((unused)))
{
    CLI_RESULT ret = CLI_ERROR;
    uint8_t earbud;

    if (earbud_cli_choice(&earbud))
    {
        long int x;

        if (cli_get_next_parameter(&x, 16))
        {
            earbud_info[earbud].corrupt_pattern = (uint16_t)x;
            ret = CLI_OK;
        }
    }

    return ret;
}

CLI_RESULT earbud_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(earbud_command, cmd_source);
}
