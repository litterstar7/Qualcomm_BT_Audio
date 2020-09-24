/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for setting up and shutting down Gaia data transfer channel.
*/

#ifndef GAIA_FRAMEWORK_DATA_CHANNEL_H_
#define GAIA_FRAMEWORK_DATA_CHANNEL_H_

#include <gaia_features.h>
#include <stream.h>
#include <transform.h>

#include "gaia_framework.h"


/*! \brief Data transfer Session ID zero is invalid. */
#define INVALID_DATA_TRANSFER_SESSION_ID    (0x0000)

/*! \brief The payload size of Gaia 'Data Transfer Setup' command. */
#define GAIA_DATA_TRANSFER_SETUP_CMD_PAYLOAD_SIZE                   (3)

/*! \brief The payload size of Gaia 'Data Transfer Setup' response. */
#define GAIA_DATA_TRANSFER_SETUP_RSP_PAYLOAD_SIZE                   (2)

/*! \brief The payload size of Gaia 'Data Transfer Get' command. */
#define GAIA_DATA_TRANSFER_GET_CMD_PAYLOAD_SIZE                     (10)

/*! \brief The header size of 'Data Transfer Get' Response payload (Session ID uses 2 bytes). */
#define GAIA_DATA_TRANSFER_GET_RSP_HEADER_SIZE                      (2)

/*! \brief The command header size of Gaia 'Data Transfer Set' command. */
#define GAIA_DATA_TRANSFER_SET_CMD_HEADER_SIZE                      (6)

/*! \brief The payload size of 'Data Transfer Set' response. */
#define GAIA_DATA_TRANSFER_SET_RSP_PAYLOAD_SIZE                      (2)

/*! \brief The max data size that a single 'DataTransfer_Get' response can carry.
           This is based on the the calculation below:
                Gaia header size(SOF~LEN,VendorID,CommandID): 8 bytes,
                Tailing CS: 1 bytes,
                (DataTransfer_Get Response) Sessioin ID: 2 bytes */
#define DATA_TRANSFER_GET_RESPONSE_PAYLOAD_SIZE     (256 - 8 - 1 - GAIA_DATA_TRANSFER_GET_RSP_HEADER_SIZE)

/*! \brief The max data size that a single 'DataTransfer_Set' command can carry.
           This is based on the the calculation below:
                Gaia header size(SOF~LEN,VendorID,CommandID): 8 bytes,
                Tailing CS: 1 bytes,
                (DataTransfer_Set Command) Sessioin ID:       2 bytes
                (DataTransfer_Set Command) Starting Offset:   4 bytes */
#define DATA_TRANSFER_SET_CMD_DATA_PAYLOAD_SIZE     (256 - 8 - 1 - GAIA_DATA_TRANSFER_SET_CMD_HEADER_SIZE)


/*! \brief Types of transport
*/
typedef uint16 gaia_data_transfer_session_id_t;

/*! \brief Types of transport
*/
typedef enum
{
    /*! No transport has been selected. */
    transpote_none = 0,

    /*! Existing Gaia link using Command & Response payloads is selected. */
    transport_gaia_command_response,
    /*! Separate RFCOMM designated for data transfer is selected. */
    transport_rfcomm_data_channel,
    /*! GATT (BLE) is selected. */
    transport_gatt,
    /*! Reriable Write Command Protocol (RWCP) over GATT (BLE) is selected. */
    transport_gatt_rwcp,

    /*! Total number of transports */
    number_of_core_transport,
} core_plugin_transport_type_t;


/*! \brief Gaia Data transfer channel Error Status Codes.
*/
typedef enum
{
    /*! Request is processed successfully. */    
    data_transfer_status_success,

    /*! There are no more data available. */
    data_transfer_no_more_data,

    /*! Invalid parameter (e.g. data size, offset) is specified. */
    data_transfer_invalid_parameter,

    /*! Request is not able to complete with unspecified reason. */
    data_transfer_status_failure_with_unspecified_reason,
    /*! Insufficient resource. */
    data_transfer_status_insufficient_resource,

    /*! The sink is invalid. */
    data_transfer_status_failed_to_get_data_info,
    /*! The sink is invalid. */
    data_transfer_status_invalid_sink,
    /*! The source is invalid. */
    data_transfer_status_invalid_source,
    /*! The stream or transform is invalid. */
    data_transfer_status_invalid_stream,

    /*! Total number of Error/Status codes for Gaia Data transfer channel. */
    number_of_data_transfer_status_code
} data_transfer_status_code_t;


/*! \brief Internal Gaia Data transfer channel message IDs */
enum gaia_data_ch_internal_messages
{
    GAIA_DATA_TRANSFER_INTERNAL_BASE,
    GAIA_DATA_TRANSFER_INTERNAL_UNKNOWN_RESPONSE = 0,
    GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_IND,
    GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM,
    GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_IND,
    GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_CFM,
    GAIA_DATA_TRANSFER_INTERNAL_GET_REQ,
    GAIA_DATA_TRANSFER_INTERNAL_GET_RSP,
    GAIA_DATA_TRANSFER_INTERNAL_SET_REQ,
    GAIA_DATA_TRANSFER_INTERNAL_SET_RSP,
    GAIA_DATA_TRANSFER_INTERNAL_TOP
};


/*! Internal response to any unknown requests/indications. */
typedef struct
{
    MessageId unknown_message_id;                   /*!< A message ID, which is unknown to this plugin. */
} GAIA_DATA_TRANSFER_INTERNAL_UNKNOWN_RESPONSE_T;

/*! Internal indication that either the sink or the source of the data transfer channel is established. */
typedef struct
{
    gaia_data_transfer_session_id_t     session_id; /*!< A 16-bit ID that represents a data transfer session. */
    Sink                                sink;       /*!< The sink of the data transfer channel. */
    Source                              source;     /*!< The source of the data transfer channel. */
} GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_IND_T;

/*! Internal confirmation that the data transfer session has been started. */
typedef struct
{
    data_transfer_status_code_t         status;     /*!< Status that tells whether the session has started successfully or not. */
    gaia_data_transfer_session_id_t     session_id; /*!< A 16-bit ID that represents a data transfer session. */
    Transform                           transform;  /*!< Transform for the data transfer channel. */
} GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM_T;

/*! Internal indication that the data transfer session has been closed. */
typedef struct
{
    gaia_data_transfer_session_id_t     session_id; /*!< A 16-bit ID that represents a data transfer session. */
} GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_IND_T;

/*! Internal confirmation that the data transfer session has been closed. */
typedef struct
{
    gaia_data_transfer_session_id_t     session_id; /*!< A 16-bit ID that represents a data transfer session. */
} GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_CFM_T;

/*! Internal request to get data bytes that is the 'size' long and starting from the 'offset'. */
typedef struct
{
    gaia_data_transfer_session_id_t     session_id;         /*!< A 16-bit ID that represents a data transfer session. */
    uint32                              starting_offset;    /*!< The starting position of the data bytes requested. */
    uint32                              requested_size;     /*!< The size of the data bytes requested. */
} GAIA_DATA_TRANSFER_INTERNAL_GET_REQ_T;

/*! Internal response to 'Data Transfer Get' request. */
typedef struct
{
    data_transfer_status_code_t         status;             /*!< Status that tells whether the request has been processed successfully or not. */
    gaia_data_transfer_session_id_t     session_id;         /*!< A 16-bit ID that represents a data transfer session. */

    uint16                              data_size;          /*!< The size of the data bytes received. */
    /* !!! Do Not Add any struct variables here !!! */
    /* The code to send Gaia 'Data Transfer Get' Response assumes 'data_size'
     * are followed by 'data[1]' for avoiding 'memmove(...)' again. */
    uint8                               data[1];            /*!< The first byte of the data bytes to be sent to the peer device. */
    /* !!! Do Not Add any struct variables here !!! */
    /* This 'data[1]' is just the first byte of the data bytes.
     * The actual message size varies and depends on the size of the data bytes. */
} GAIA_DATA_TRANSFER_INTERNAL_GET_RSP_T;

/*! Internal request to set data bytes that starts from the 'offset'. */
typedef struct
{
    gaia_data_transfer_session_id_t     session_id;         /*!< A 16-bit ID that represents a data transfer session. */
    uint32                              starting_offset;    /*!< The starting position of the data bytes received. */

    uint16                              data_size;          /*!< The size of the data bytes received. */
    uint8                               data[1];            /*!< The first byte of the data bytes received from the peer device. */
    /* !!! Do Not Add any struct variables here !!! */
    /* This 'data[1]' is just the first byte of the data bytes.
     * The actual message size varies and depends on the size of the data bytes. */
} GAIA_DATA_TRANSFER_INTERNAL_SET_REQ_T;

/*! Internal response to 'Data Transfer Set' request. */
typedef struct
{
    data_transfer_status_code_t         status;             /*!< Status that tells whether the request has been processed successfully or not. */
    gaia_data_transfer_session_id_t     session_id;         /*!< A 16-bit ID that represents a data transfer session. */
} GAIA_DATA_TRANSFER_INTERNAL_SET_RSP_T;

/*! Macro for creating messages */
#define MAKE_GAIA_DATA_TRANSFER_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);
#define MAKE_GAIA_DATA_TRANSFER_MESSAGE_WITH_LEN(TYPE, LEN) TYPE##_T *message = (TYPE##_T *) PanicUnlessMalloc(sizeof(TYPE##_T) + LEN - 1);




/*! \brief Gets the function pointer of the data transfer task associated with
           a Session ID.

    \param session_id       A 16-bit ID that represents a data transfer session.
                            where Gaia data transfer channel messages are forwarded to.

    \return Task that handles data transfer channel messages.
*/
Task GaiaFramework_GetDataTransferHandler(gaia_data_transfer_session_id_t session_id);


/*! \brief Create a data transfer session, and registers the message handler
           of the Gaia Feature which receives messages for data transfer.

    \param t                GAIA transport associated with this data transfer session.

    \param feature_id       Feautue ID that represents a Gaia Feature, which will
                            be bound with a data transfer session.

    \param handler_task     Task of the Gaia Feature specified by feature_id,
                            where Gaia data transfer channel messages are forwarded to.

    \return Session ID created on success, otherwise INVALID_DATA_TRANSFER_SESSION_ID.
*/
gaia_data_transfer_session_id_t GaiaFramework_CreateDataTransferSession(GAIA_TRANSPORT *t, uint8 feature_id, Task handler_task);


/*! \brief Delete the data transfer session specified by the Session ID.
           of the Gaia Feature which receives messages for data transfer.

    \param session_id       A 16-bit ID that represents a data transfer session.
*/
void GaiaFramework_DeleteDataTransferSession(gaia_data_transfer_session_id_t session_id);


/*! \brief Set up a data transfer channel on the transport specified.

    \param t            GAIA transport associated with this data transfer session.

    \param payload_size Payload size. This must be 3 (bytes).

    \param payload      Payload data that contains 16-bit Session ID and 8-bit Transport Type.

    \return TRUE on success, otherwise FALSE.

    Command Payload formart:
         0        1        2    (Byte)
    +--------+--------+--------+
    |    Session ID   |  (*1)  |    (*1) Transport Type (core_plugin_transport_type_t)
    +--------+--------+--------+
        - session_id        This 16-bit ID indicates the Gaia Feature that uses
                            the data transfer channel to be opened.

        - transport_type    Transport type specifes which transport (BR/EDR or BLE)
                            to be used for the datta transfer channel.

    This function checks if the command payload contains valid parameters, 'Session ID'
    and 'Transport Type' shown above.

    Transport Type: Gaia Command & Response (transport_gaia_command_response)
        As this transport uses existing Gaia Command & Response link,
        this function just checks if the given session_id is valid or not.
        To send or receive data bytes, the mobile app must use either of the
        commands below:
            - Data Transfer Get Command
                The requested (as an offset) part of the data bytes will be
                sent as the response payload to the mobile app.
            - Data Transfer Set Command
                The mobile app sends data bytes to the device as the part of
                the command payload.

    Transport Type: RFCOMM Data Channel (transport_rfcomm_data_channel)
        This function initiates the followings:
            (1) To allocate an RFCOMM channel.
            (2) To register an SDP record that includes the RFCOMM channel.

        At the host (e.g. Android mobile app), application level APIs use
        a UUID rather than an RFCOMM channel number to establish an RFCOMM
        connection. The UUID represents a service, and in our case, which has
        an RFCOMM channel number provided by the SDP record.

        This means that there are some message excanges between the stack
        (RFCOMM, SDP) and this Gaia code before we can send a response back to
        the mobile application.

    Transport Type: GATT (transport_gatt)
        (Not supported)

    Transport Type: GATT RWCP (transport_gatt_rwcp)
        (Not supported)
*/
bool GaiaFramework_DataTransferSetup(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);



/*! \brief Handle 'Data Transfer Get' Command from the mobile app.

    \param t            GAIA transport associated with this data transfer session.

    \param payload_size Payload size. This must be 10 (bytes).

    \param payload      Payload data that contains the command parameters:
                            Session ID, Starting offset, and requested size.

    This function checks if the Session ID is valid or not, and then forward the
    request to the Feature associated to this Session ID.

    Command Payload formart:
         0        1        2        3        4        5        6        7        8        9    (Byte)
    +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
    |    Session ID   |  (MSB)   Starting Offset   (LSB)  |  (MSB)   Requested Size    (LSB)  |
    +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+

    Notes:
        This function is applicable only if the transprot type is 'transport_gaia_command_response'.
*/
bool GaiaFramework_DataTransferGet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);


/*! \brief Handles 'Data Transfer Set' Command from the mobile app.

    \param t            GAIA transport associated with this data transfer session.

    \param payload_size Payload size, which is variable and depends on the payload.

    \param payload      Payload data that contains Session ID and Data Bytes.

    This function checks if the Session ID is valid or not, and then forward the
    request to the Feature associated to this Session ID.

    Command Payload formart:
         0        1        2        3        4        5        6        7       ...    (6 + N) (Byte)
    +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
    |    Session ID   |  (MSB)   Starting Offset   (LSB)  | Data 0 | Data 1 |   ...  | Data N |
    +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+

    Notes:
        This function is applicable only if the transprot type is 'transport_gaia_command_response'.
*/
bool GaiaFramework_DataTransferSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);


/*! \brief Shut down all the data transfer channels. Free up the resources
           allocated for the data transfer channels.

    \return TRUE on success, otherwise FALSE.

    This function ensures the following things:
     - Unregister the SDP record.
     - Deallocate the RFCOMM channel.
     - Delete data transfer session instances (i.e. free memory).
*/
void GaiaFramework_ShutDownDataChannels(void);


/*! \brief Gaia Data Transfer internal message handler.

    \param task     The task that has sent the internal data transfer message.

    \param id       The message ID to handle.

    \param message  The message content (if any).

    This function manages data transfer internal messages that come from Gaia
    Feature plugins.
*/
void GaiaFramework_DataTransferInternalMessageHandler(Task task, MessageId id, Message message);

#endif /* GAIA_FRAMEWORK_DATA_CHANNEL_H_ */
