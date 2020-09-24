/****************************************************************************
Copyright (c) 2016 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    tws_packetiser_common.c
*/

#include <sink.h>
#include <logging.h>

#include <tws_packetiser.h>
#include <tws_packetiser_private.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(tws_packetiser_message_id_t)


uint8 *tpSinkMapAndClaim(Sink sink, uint32 len)
{
    uint8 *dest = SinkMap(sink);
    if (dest)
    {
        int32 extra = len - SinkClaim(sink, 0);
        if (extra > 0)
        {
            if (SinkClaim(sink, extra) == 0xFFFF)
            {
                return NULL;
            }
        }
    }
    return dest;
}
