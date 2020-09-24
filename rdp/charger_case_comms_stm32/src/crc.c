/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     CRC-8-WCDMA encoder and verifier. The input is fed in reverse order (MSB first).
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "crc.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/* The polynomial for 8-bits CRC is  X^8 + x^7 + x^4 + x^3 + x+ 1
 * The reversed representation is used */
#define GNPOLY   0xd9

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint16_t update_crc8_loop(uint16_t crc, uint8_t loop_no, uint8_t gnp)
{
    uint16_t cnt;

    for(cnt = 0; cnt < loop_no; cnt++)
    {
        crc = (crc & 0x1)? ((crc >> 1) ^ gnp):(crc >> 1);
    }

    return crc;
}

/**
 * @brief This function performs CRC encoding or validation depending on \encode.
 * If \encode is True, it generating a CRC.
 * If \encode is False, it is validating a CRC.
 *
 * @param *data  Pointer to the data which should be encoded or validated.
 *               If encode is True, This is the data to encode a CRC for.
 *               If encode is False, The data decode and 8 bit CR
 * @param data_len The lengh of data in octets. It does not include the 8 bit CRC byte when verifying
 * @param encode If True, generate a CRC. If false, validate the data with the CRC as the final octet.
 * @param *parity Pointer to the encoded/decoded CRC value.
 */
static void crc_encoder(uint8_t *data, uint8_t data_len, bool encode, uint8_t *parity)
{
    /*
    * The polynomial for CRC is  X^8 + x^7 + x^4 + x^3 + x+ 1
    */
    uint16_t gnp = GNPOLY;
    uint16_t crc;
    uint16_t cnt;

    /* Initial value is all ones to ensure CRC fails with data full of zero's. */
    crc = 0xff;

    /* first encoded/decoded data in U8 word count */
    for (cnt = 0; cnt < data_len; cnt++)
    {
        crc = crc ^ (data[cnt] & 0xff);
        crc = update_crc8_loop(crc, 8, gnp);
    }

    /* If decoding, read 8-bit parity which is the next word of the input data */
    if(!encode)
    {
        /* It is decoding, then process the 8-bit parity*/
        crc = crc ^ (data[data_len] & 0xff);
        crc = update_crc8_loop(crc, 8, gnp);
    }
    *parity  = crc & 0xff;
}

uint8_t crc_calculate_crc8(uint8_t *data, uint8_t data_len)
{
    uint8_t crc_result;
    crc_encoder(data, data_len, true, &crc_result);
    return crc_result;
}

bool crc_verify_crc8(uint8_t *data, uint8_t data_len)
{
    uint8_t crc_parity;
    crc_encoder(data, data_len, false, &crc_parity);
    return crc_parity == 0;
}
