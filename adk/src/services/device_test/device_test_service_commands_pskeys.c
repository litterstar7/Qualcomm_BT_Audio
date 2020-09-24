/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Implementation of device test service commands for handling persistent
            storage keys.
*/
/*! \addtogroup device_test_service
@{
*/

#include "device_test_service.h"
#include "device_test_service_auth.h"
#include "device_test_service_commands_helper.h"
#include "device_test_parse.h"

#include <ps.h>
#include <logging.h>
#include <stdio.h>
#include <ctype.h>

/*! The maximum length of PSKEY that can be supported 

    This length will be honoured for reading PSKEYS. Pskeys of this length
    can be written if the command reaches here. The length may be restricted
    elsewhere in the system */
#define PSKEY_MAX_LENGTH_SUPPORTED  160

/*! The base part of the AT command response */
#define PSKEYGET_RESPONSE "+PSKEYGET:"

#define PSKEYGET_RESPONSE_VARIABLE_FMT "%d,%d,"

/*! Worst case variable length portion */
#define PSKEYGET_RESPONSE_VARIABLE_EXAMPLE "519,65535,"

/*! The length of the full response, including the maximum length of 
    any variable portion. As we use sizeof() this will include the 
    terminating NULL character*/
#define BASE_PSKEYGET_RESPONSE_LEN (sizeof(PSKEYGET_RESPONSE) + sizeof(PSKEYGET_RESPONSE_VARIABLE_EXAMPLE))

/*! Length of local buffer for response to a PSKEYGET */
#define PSKEYGET_BUFFER_BYTES 120

/*! Length of each word in the response string

    4 hex digits and a space */
#define PSKEYGET_LENGTH_OF_WORD_IN_RESPONSE 5

/*! Should be able to fit at least one word in response buffer */
COMPILE_TIME_ASSERT(PSKEYGET_BUFFER_BYTES >= BASE_PSKEYGET_RESPONSE_LEN + PSKEYGET_LENGTH_OF_WORD_IN_RESPONSE,
                    no_space_for_any_words_in_pskey_response);

/*! Helper macro to find out how many words of a response can fit in a specified length */
#define PSKEYGET_NUM_WORDS_IN(_len) ((_len) / PSKEYGET_LENGTH_OF_WORD_IN_RESPONSE)

/*! Number of words to fit in the first part of response */
#define PSKEYGET_NUM_WORDS_IN_FIRST_RESPONSE  PSKEYGET_NUM_WORDS_IN(PSKEYGET_BUFFER_BYTES - BASE_PSKEYGET_RESPONSE_LEN)

/*! Number of words in a continuation response */
#define PSKEYGET_NUM_WORDS_IN_RESPONSE PSKEYGET_NUM_WORDS_IN(PSKEYGET_BUFFER_BYTES)


/*! Macro to map between local and global PSKEY identifiers */
#define PSKEY_MAP(_START, _END, _INTERNAL) { (_START), (_END), (_INTERNAL), (_INTERNAL + ((_END)-(_START))) }

/*! Structure containing the complete mapping between PSKEY identifiers */
static const struct 
{
    uint16 global_key_id_start;
    uint16 global_key_id_end;
    uint16 local_key_id_start;
    uint16 local_key_id_end;
} pskey_range_map[] = {
        PSKEY_MAP(PSKEY_USR0,       PSKEY_USR49, 0),
        PSKEY_MAP(PSKEY_DSP0,       PSKEY_DSP49, 50),
        PSKEY_MAP(PSKEY_CONNLIB0,   PSKEY_CONNLIB49, 100),
        PSKEY_MAP(PSKEY_USR50,      PSKEY_USR99, 150),
        PSKEY_MAP(PSKEY_CUSTOMER0,  PSKEY_CUSTOMER89, 200),
        PSKEY_MAP(PSKEY_READONLY0,  PSKEY_READONLY9, 290),
        PSKEY_MAP(PSKEY_CUSTOMER90, PSKEY_CUSTOMER299, 300),
        PSKEY_MAP(PSKEY_UPGRADE0,   PSKEY_UPGRADE9, 510),
};

/*! \brief convert a supplied pskey ID to an internal number 

    \param id User supplied pskey ID. Could be internal, or PSKEY ID

    \return The local/internal key id used by PsRetrieve(), PsStore().
            If the ID is not mapped, then the original ID is returned.
*/
static uint16  deviceTestServiceCommand_GetInternalId(uint16 id)
{
    unsigned map_entry = ARRAY_DIM(pskey_range_map);

    if (id <= 519)
    {
        /* Already an internal ID */
        return id;
    }

    while (map_entry--)
    {
        if (   pskey_range_map[map_entry].global_key_id_start <= id
            && id <= pskey_range_map[map_entry].global_key_id_end)
        {
            return    pskey_range_map[map_entry].local_key_id_start 
                   + (id - pskey_range_map[map_entry].global_key_id_start);
        }
    }

    DEBUG_LOG_WARN("DeviceTestServiceCommand_GetInternalId accessing unmapped PSKEY %d (0x%x)", id, id);
    return id;
}

/*! \brief convert a supplied internal pskey ID to the external PSKEY ID

    \param local_id The local identifier

    \return The matching global PSKEY ID
*/
static uint16 deviceTestServiceCommand_GetGlobalId(uint16 local_id)
{
    unsigned map_entry = ARRAY_DIM(pskey_range_map);

    while (map_entry--)
    {
        if (   pskey_range_map[map_entry].local_key_id_start <= local_id 
            && local_id <= pskey_range_map[map_entry].local_key_id_end)
        {
            return    pskey_range_map[map_entry].global_key_id_start 
                   + (local_id - pskey_range_map[map_entry].local_key_id_start);
        }
    }

    /* Garbage in, garbage out. This should never happen, but Panic doesnt seem like a good idea. */
    return local_id;
}


/*! Command handler for AT + PSKEYSET = pskey, value

    This function sets the specified key to the requested value.

    Errors are reported if the requested key is not supported, or if
    the value cannot be validated.

    \param[in] task The task to be used in command responses
    \param[in] set  The parameters from the received command
 */
void DeviceTestServiceCommand_HandlePskeySet(Task task,
                            const struct DeviceTestServiceCommand_HandlePskeySet *set)
{
    unsigned key = set->pskey;
    uint16 local_key = deviceTestServiceCommand_GetInternalId(key);
    /* Allocate an extra word to simplify length checking */
    uint16 key_to_store[PSKEY_MAX_LENGTH_SUPPORTED+1];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePskeySet. pskey:%d (local:%d)", key, local_key);

    if (   !DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    unsigned nibbles = 0;
    unsigned string_len = set->value.length;
    const uint8 *input = set->value.data;
    uint16 value = 0;
    uint16 *next_word = key_to_store;
    bool error = FALSE;
    unsigned key_len = 0;

    while (string_len-- && (key_len <= PSKEY_MAX_LENGTH_SUPPORTED))
    {
        char ch = *input++;

        if (!isxdigit(ch))
        {
            if (ch == ' ')
            {
                if (nibbles == 0)
                {
                    continue;
                }
            }
            error = TRUE;
            break;
        }
        value = (value << 4) + deviceTestService_HexToNumber(ch);
        if (++nibbles == 4)
        {
            *next_word++ = value;
            value = 0;
            nibbles = 0;
            key_len++;
        }
    }

    if (!error && nibbles == 0 && key_len && key_len <= PSKEY_MAX_LENGTH_SUPPORTED)
    {
        DEBUG_LOG_VERBOSE("DeviceTestServiceCommand_HandlePskeySet. Storing key:%d length:%d",
                            local_key, key_len);

        if (PsStore(local_key, key_to_store, key_len))
        {
            DeviceTestService_CommandResponseOk(task);
            return;
        }
        else
        {
            DEBUG_LOG_WARN("DeviceTestServiceCommand_HandlePskeySet. Failed to write. Defragging.");

            PsDefragBlocking();
            if (PsStore(local_key, key_to_store, key_len))
            {
                DeviceTestService_CommandResponseOk(task);
                return;
            }
        }
    }

    DEBUG_LOG_DEBUG("DeviceTestServiceCommand_HandlePskeySet. Failed. Key:%d Error:%d Attempted length:%d",
                    local_key, error, key_len);
    DeviceTestService_CommandResponseError(task);
}


/*! Command handler for AT + PSKEYGET = pskey, value

    This function read the specified key and sends its value as a response 
    followed by OK.

    An error is reported if the requested key is not supported.

    \param[in] task The task to be used in command responses
    \param[in] get  The parameters from the received command
 */
void DeviceTestServiceCommand_HandlePskeyGet(Task task,
                            const struct DeviceTestServiceCommand_HandlePskeyGet *get)
{
    uint16 key = get->pskey;
    uint16 local_key = deviceTestServiceCommand_GetInternalId(key);
    uint16 pskey_length_words;
    uint16 retrieved_key[PSKEY_MAX_LENGTH_SUPPORTED];
    char   response[PSKEYGET_BUFFER_BYTES];

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePskeyGet. pskey:%d (local:%d)", key, local_key);

    if (   !DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    pskey_length_words = PsRetrieve(local_key, NULL, 0);
    if (pskey_length_words && pskey_length_words <= PSKEY_MAX_LENGTH_SUPPORTED)
    {
        if (PsRetrieve(local_key, retrieved_key, pskey_length_words) == pskey_length_words)
        {
            uint16 global_key = deviceTestServiceCommand_GetGlobalId(local_key);
            char *next_char = response;
            unsigned index_into_pskey = 0;
            unsigned portion_end = PSKEYGET_NUM_WORDS_IN_FIRST_RESPONSE;

            /* The first part of the response contains the text for PSKEYGET */

            next_char += sprintf(next_char, PSKEYGET_RESPONSE PSKEYGET_RESPONSE_VARIABLE_FMT,
                                 global_key, local_key);

            /* Now populate each response, adjusting variables for the next response
               (if any is needed) */
            while (index_into_pskey < pskey_length_words)
            {
                while (index_into_pskey < portion_end && index_into_pskey < pskey_length_words)
                {
                    next_char += sprintf(next_char,"%04X ",retrieved_key[index_into_pskey++]);
                }
                DeviceTestService_CommandResponsePartial(task, 
                                                         response, next_char - response,
                                                         index_into_pskey <= PSKEYGET_NUM_WORDS_IN_FIRST_RESPONSE, 
                                                         index_into_pskey >= pskey_length_words);

                next_char = response;
                portion_end += PSKEYGET_NUM_WORDS_IN_RESPONSE;
            }
            DeviceTestService_CommandResponseOk(task);
            return;
        }
    }

    DeviceTestService_CommandResponseError(task);
}


/*! Command handler for AT + PSKEYCLEAR = pskey

    This function clears the specified key to the requested value.

    Errors are reported if the requested key is not supported.

    \param[in] task The task to be used in command responses
    \param[in] clear The parameters from the received command
 */
void DeviceTestServiceCommand_HandlePskeyClear(Task task,
                            const struct DeviceTestServiceCommand_HandlePskeyClear *clear)
{
    unsigned key = clear->pskey;
    uint16 local_key = deviceTestServiceCommand_GetInternalId(key);

    DEBUG_LOG_ALWAYS("DeviceTestServiceCommand_HandlePskeyClear. pskey:%d (local:%d)", key, local_key);

    if (!DeviceTestService_CommandsAllowed())
    {
        DeviceTestService_CommandResponseError(task);
        return;
    }

    /* No error to detect if clearing a key */
    PsStore(local_key, NULL, 0);
    DeviceTestService_CommandResponseOk(task);
}

