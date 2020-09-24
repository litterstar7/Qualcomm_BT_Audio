/****************************************************************************
Copyright (c) 2010 - 2018 Qualcomm Technologies International, Ltd.


*/

/*!
    @file gaia.h
    @brief Header file for the Generic Application Interface Architecture library

    This library implements the GAIA protocol.

    The library exposes a functional downstream API and a message-based upstream API.
*/

#ifndef _GAIA_H_
#define _GAIA_H_

#include <library.h>
#include <bdaddr.h>
#include <message.h>
#include <gatt.h>
#include <upgrade.h>
#include <handover_if.h>

#include "transport_manager.h"

/* GAIA Lib Handover interface */
extern const handover_interface gaia_handover_if;

/*  GAIA API Version 2.0  */
#define GAIA_API_VERSION_MAJOR (2)
#define GAIA_API_VERSION_MINOR (0)

#define GAIA_PROTOCOL_FLAG_NONE (0x00)      /* No checksum */
#define GAIA_PROTOCOL_FLAG_CHECK (0x01)     /* Checksum at end of packet */

#define GAIA_PROTOCOL_FLAG_8_BIT_LENGTH (0x00)      /* Payload length field is 8 bits */
#define GAIA_PROTOCOL_FLAG_16_BIT_LENGTH (0x02)     /* Payload length field is 16 bits */

#define GAIA_VENDOR_QTIL (0x000A)
#define GAIA_VENDOR_NONE (0x7FFE)

#define GAIA_ACK_MASK (0x8000)
#define GAIA_ACK_MASK_H (0x80)

#define GAIA_COMMAND_TYPE_MASK (0x7F00)
#define GAIA_COMMAND_DEVICE_RESET (0x0202)
#define GAIA_COMMAND_GET_API_VERSION (0x0300)

#define GAIA_STATUS_SUCCESS (0x00)
#define GAIA_STATUS_NOT_SUPPORTED (0x01)
#define GAIA_STATUS_NOT_AUTHENTICATED (0x02)
#define GAIA_STATUS_INSUFFICIENT_RESOURCES (0x03)
#define GAIA_STATUS_AUTHENTICATING (0x04)
#define GAIA_STATUS_INVALID_PARAMETER (0x05)
#define GAIA_STATUS_INCORRECT_STATE (0x06)
#define GAIA_STATUS_IN_PROGRESS (0x07)

#define GAIA_STATUS_NONE (0xFE)

/*! @brief Enumeration of types of transport which Gaia supports.
 */
typedef enum
{
    gaia_transport_test,                                /*!< Test transport */
    gaia_transport_rfcomm = transport_mgr_type_rfcomm,  /*!< RFCOMM using CSR GAIA UUID */
    gaia_transport_spp,                                 /*!< Serial Port Profile (SPP) transport */
    gaia_transport_gatt,                                /*!< GATT (LE) transport */
    gaia_transport_iap2,                                /*!< iAP2 / Bluetooth transport */
    gaia_transport_max,
    FORCE_ENUM_TO_MIN_16BIT(gaia_transport_type)
} gaia_transport_type;

/*! @brief Enumeration of messages a client task may receive from the Gaia library.
 */
typedef enum
{
    GAIA_INIT_CFM = GAIA_MESSAGE_BASE,  /*!< Confirmation that the library has initialised */
    GAIA_CONNECT_CFM,                   /*!< Confirmation of an outbound connection request */
    GAIA_CONNECT_IND,                   /*!< Indication of an inbound connection */
    GAIA_DISCONNECT_IND,                /*!< Indication that the connection has closed */
    GAIA_DISCONNECT_CFM,                /*!< Confirmation that a requested disconnection has completed */
    GAIA_START_SERVICE_CFM,             /*!< Confirmation that a Gaia server has started */
    GAIA_STOP_SERVICE_CFM,              /*!< Confirmation that a Gaia server has stopped */
    GAIA_COMMAND_IND,                   /*!< Indication that a command has been received */
    GAIA_COMMAND_RES,                   /*!< Indication that a command has been received */

    GAIA_UPGRADE_CONNECT_IND,           /*!< Indication of VM Upgrade successful connection */
    GAIA_UPGRADE_DISCONNECT_IND,        /*!< Indication of VM Upgrade is disconnected*/

    /* Library message limit */
    GAIA_MESSAGE_TOP
} GaiaMessageId;


/*! @brief Gaia app version enumerations
 */
typedef enum
{
    gaia_app_version_2 = 0, /*!< Gaia v2 application */
    gaia_app_version_3,     /*!< Gaia v3 application */
} gaia_app_version_t;


/*! @brief Data endpoint modes.
 */
typedef enum
{
    GAIA_DATA_ENDPOINT_MODE_NONE,
    GAIA_DATA_ENDPOINT_MODE_RWCP
} gaia_data_endpoint_mode_t;



/*! @brief Definition of a GAIA_TRANSPORT handle.
 *
 * Used to identity a specific Gaia connection with a remote device over a specific transport instance.
 */
typedef struct _gaia_transport GAIA_TRANSPORT;

/*! @brief Common message structure conveying transport only.
 */
typedef struct
{
    GAIA_TRANSPORT *transport;          /*!< Indicates the GAIA instance */
} GAIA_TRANSPORT_MESSAGE_T;

/*! @brief Common message structure conveying transport and success of an operation.
 */
typedef struct
{
    GAIA_TRANSPORT *transport;          /*!< Indicates the GAIA instance */
    bool success;                       /*!< The success of the operation */
} GAIA_TRANSPORT_SUCCESS_MESSAGE_T;

/*! @brief Message confirming status of Gaia library initialisation.
 */
typedef struct
{
    bool success;                       /*!< The success of the initialisation request */
} GAIA_INIT_CFM_T;


/*! @brief Message indicating an incoming connection attempt.
 */
typedef GAIA_TRANSPORT_SUCCESS_MESSAGE_T GAIA_CONNECT_IND_T;


/*! @brief Message indicating a Gaia connection has been disconnected.
 */
typedef GAIA_TRANSPORT_MESSAGE_T GAIA_DISCONNECT_IND_T;


/*! @brief Message confirming the status of a request by a client task to disconnect a Gaia connection.
 */
typedef GAIA_TRANSPORT_MESSAGE_T GAIA_DISCONNECT_CFM_T;


/*! @brief Message confirming the status of a connection request initiated by a client task.
 */
typedef GAIA_TRANSPORT_SUCCESS_MESSAGE_T GAIA_CONNECT_CFM_T;


/*! @brief Message confirming the status of a Start Service request initiated by a client task.
 */
typedef struct
{
    gaia_transport_type transport_type; /*!< Indicates the transport type */
    GAIA_TRANSPORT *transport;          /*!< Indicates the transport instance */
    bool success;                       /*!< The success of the request */
} GAIA_START_SERVICE_CFM_T;


/*! @brief Message indicating an incoming Gaia command that cannot be handled by the Gaia library.
 */
typedef struct
{
    GAIA_TRANSPORT *transport;          /*!< Indicates the originating instance */
    uint8 protocol_version;             /*!< The protocol version */
    uint16 vendor_id;                   /*!< The 16-bit Vendor ID */
    uint16 command_id;                  /*!< The 16-bit command ID */
    uint16 size_payload;                /*!< The length of the payload */
    const uint8 *payload;               /*!< The payload octets unpacked into uint8s*/
} GAIA_COMMAND_IND_T;


/*! @brief V3 app command handler function definition.
 */
typedef void (*gaia_v3_command_handler_fn_t)(Task task, GAIA_TRANSPORT *transport,
                                             uint16 command_id, uint16 vendor_id,
                                             uint16 size_payload, const uint8 *payload);

/*! @brief Gets the app version the gaia library is currently using
*/
gaia_app_version_t GaiaGetAppVersion(void);

/*!
    @brief Disconnect from host
*/
void GaiaDisconnectRequest(GAIA_TRANSPORT *transport);


/*!
    @brief Respond to a GAIA_DISCONNECT_IND message
*/
void GaiaDisconnectResponse(GAIA_TRANSPORT *transport);


/*!
    @brief Initialise the Gaia protocol handler library
    @param task The task that is responsible for handling messages sent from the library.
    @param max_connections The maximum number of concurrent connections to be accommodated.
*/
void GaiaInit(Task task, uint16 max_connections);

/*!
    @brief Changes the API Minor Version reported by GAIA_COMMAND_GET_API_VERSION
    @param version the API Minor Version to be reported, 0 to 15
    Changes the API Minor Version reported by GAIA_COMMAND_GET_API_VERSION.
    Returns TRUE on success, FALSE if the value is out of range (0..15) or
    the GAIA library is not initialised.
*/
bool GaiaSetApiMinorVersion(uint8 version);


/*!
    @brief Request that the given command be passed to application code
           rather than being handled by the library.  Returns TRUE on success.
    @param command_id The Command ID
    @param value TRUE if the command is to be handled by application code,
           FALSE if the library is to be responsible for performing this command

    Certain commands for which a library implementation is provided may instead
    be passed to the application code.  This is useful where an application
    implementation already exists or where the application wishes to perform
    extra processing above and beyond that provided by the library implementation.

    Returns TRUE on success.  Some commands must always be handled by the library
    and some are not implemented in the library.  Attempting to change the locus
    of these commands results in a FALSE return value.
*/
bool GaiaSetAppWillHandleCommand(uint16 command_id, bool value);


/*!
    @brief Build a Gaia packet in the transport sink and flush it.
           The function completes synchronously and no confirmation message is sent
           to the calling task
    @param transport Pointer to a GAIA_TRANSPORT over which to transmit.
    @param vendor_id The Vendor ID to include in the assembled packet
    @param command_id The Command ID to include in the assembled packet
    @param status The Status Code to include in the assembled packet
    @param size_payload The length of the payload (not including the Status
           Code) to be copied into the packet
    @param payload Pointer to the uint8[] payload to be built into the assembled packet.
           If the payload length is zero this argument is not accessed and may be NULL
*/
void GaiaSendPacket(GAIA_TRANSPORT *transport, uint16 vendor_id, uint16 command_id,
                    uint8 status, uint16 size_payload, const uint8 *payload);

void Gaia_SendDataPacket(GAIA_TRANSPORT *transport, uint16 vendor_id, uint16 command_id,
                         uint8 status, uint16 size_payload, const uint8 *payload);

/*!
    @brief Returns TRUE if GAIA session is enabled for the given transport instance
    @param transport Pointer to a GAIA_TRANSPORT instance.
*/
bool GaiaGetSessionEnable(const GAIA_TRANSPORT *transport);


/*!
    @brief Enables or disables GAIA session for the given transport instance
    @param transport Pointer to a GAIA_TRANSPORT instance.
    @param enable TRUE to enable the session, FALSE to disable it
*/
void GaiaSetSessionEnable(GAIA_TRANSPORT *transport, bool enable);


/*!
    @brief Returns the transport type for the given transport instance
    @param transport Pointer to a GAIA_TRANSPORT instance.
*/
gaia_transport_type GaiaTransportGetType(const GAIA_TRANSPORT *transport);



/*! @brief Process the GAIA data received on GAIA data endpoint.
 */
void GaiaRwcpProcessCommand(const uint8 *command, uint16 size_command);

/*! @brief Process the notification sent from RWCP server */
void GaiaRwcpSendNotification(const uint8 *payload, uint16 payload_length);

/*! @brief Sets the app version the gaia library should use

    @param version version to use
*/
void GaiaSetAppVersion(gaia_app_version_t version);


typedef struct
{
    union
    {
        bdaddr rfcomm_bdaddr;
    } u;
} gaia_transport_connect_params_t;


typedef struct
{
    uint16 max_tx_payload_size; /*!< Maximum transmit payload size */
    uint16 optimum_tx_payload_size; /*!< Optimum transmit payload size */
    uint16 max_rx_payload_size; /*!< Maximum receive payload size */
    uint16 optimum_rx_payload_size; /*!< Optimum receive payload size */

    unsigned has_tx_flow_control:1; /*!< Transport has flow control in transmit direction */
    unsigned has_rx_flow_control:1; /*!< Transport has flow control in receive direction */
} gaia_transport_info_t;

typedef enum
{
    GAIA_TRANSPORT_MAX_TX_PAYLOAD = 1,
    GAIA_TRANSPORT_OPTIMUM_TX_PAYLOAD,
    GAIA_TRANSPORT_MAX_RX_PAYLOAD,
    GAIA_TRANSPORT_OPTIMUM_RX_PAYLOAD,
    GAIA_TRANSPORT_TX_FLOW_CONTROL,
    GAIA_TRANSPORT_RX_FLOW_CONTROL,
    GAIA_TRANSPORT_PROTOCOL_VERSION,
} gaia_transport_info_key_t;


struct _gaia_transport;


typedef struct
{
    uint8 service_data_size;

    /*! Start GAIA service for transport */
    void (*start_service)(struct _gaia_transport *t);

    /*! Stop GAIA service for transport */
    void (*stop_service)(struct _gaia_transport *t);

    /*! Connect transport */
    void (*connect_req)(struct _gaia_transport *t, const gaia_transport_connect_params_t *);

    /*! Disconnect transport */
    void (*disconnect_req)(struct _gaia_transport *t);

    /*! Disconnect due to error */
    void (*error)(struct _gaia_transport *t);

    /*! Indication that packet has been handled and transport can free any resources associated with packet */
    void (*packet_handled)(struct _gaia_transport *t, uint16 size_payload, const void *payload);

    /*! Send packet over command channel */
    bool (*send_command_packet)(struct _gaia_transport *t, uint16 vendor_id, uint16 command_id,
                                uint8 status, uint16 size_payload, const void *payload);

    /*! Set data endpoint */
    bool (*set_data_endpoint)(struct _gaia_transport *t, gaia_data_endpoint_mode_t endpoint);

    /*! Get data endpoint */
    gaia_data_endpoint_mode_t (*get_data_endpoint)(struct _gaia_transport *t);

    /*! Get data endpoint payload was received on */
    gaia_data_endpoint_mode_t (*get_payload_data_endpoint)(struct _gaia_transport *t, uint16 size_payload, const uint8 *payload);

    /*! Send packet over data channel (maybe same as command channel on transports that don't support separate data channel) */
    bool (*send_data_packet)(struct _gaia_transport *t, uint16 vendor_id, uint16 command_id,
                             uint8 status, uint16 size_payload, const void *payload);

    /*! Get features supported transport (voice assistant, etc...) */
    uint8 (*features)(struct _gaia_transport *t);

    /*! Get transport information (max packet size, etc...) */
    bool (*get_info)(struct _gaia_transport *t, gaia_transport_info_key_t key, uint32 *value);

    /*! Set transport parameter (max packet size, etc...) */
    bool (*set_parameter)(struct _gaia_transport *t, gaia_transport_info_key_t key, uint32 *value);
} gaia_transport_functions_t;



typedef enum
{
    GAIA_TRANSPORT_STOPPING,        /*!< Transport is stopping */
    GAIA_TRANSPORT_STOPPED,         /*!< Transport is stopped, all connection are disconected */
    GAIA_TRANSPORT_STARTING,        /*!< Transport is starting */
    GAIA_TRANSPORT_STARTED,         /*!< Transport has started and ready to connect */
    GAIA_TRANSPORT_CONNECTING,      /*!< Transport is connecting */
    GAIA_TRANSPORT_CONNECTED,       /*!< Transport is connected */
    GAIA_TRANSPORT_DISCONNECTING,   /*!< Transport is disconnecting */
    GAIA_TRANSPORT_ERROR,           /*!< Transport is disconnecting due to error */
} gaia_transport_state;


typedef enum
{
    GAIA_TRANSPORT_VERSION_ERROR,
    GAIA_TRANSPORT_FRAMING_ERROR,
    GAIA_TRANSPORT_CHECKSUM_ERROR,
    GAIA_TRANSPORT_INSUFFICENT_BUFFER_SPACE,
} gaia_transport_error;

#define GAIA_TRANSPORT_FEATURE_DATA     (0x02)  /*!< Support separate data session/channel */
#define GAIA_TRANSPORT_FEATURE_BREDR    (0x80)

/*! @brief Generic Gaia transport data structure.
 */
typedef struct _gaia_transport
{
    TaskData task;
    const gaia_transport_functions_t *functions;
    gaia_transport_type type;
    gaia_transport_state state;
    uint8 flags;
    uint32 client_data;
} gaia_transport;

void Gaia_TransportRegister(gaia_transport_type, const gaia_transport_functions_t *);

/*! @brief Attempt to connect Gaia to a device over a given transport.
 */
void Gaia_TransportConnectReq(gaia_transport_type, const gaia_transport_connect_params_t *);
void Gaia_TransportConnectInd(gaia_transport *t, bool success);
bool Gaia_TransportIsConnected(gaia_transport *t);

/*! @brief Attempt to disconnect Gaia over a given transport.
 */
void Gaia_TransportDisconnectReq(gaia_transport *t);
void Gaia_TransportDisconnectInd(gaia_transport *t);

/*! @brief Start Gaia as a server on a given transport.
 */
void Gaia_TransportStartService(gaia_transport_type transport_type);
void Gaia_TransportStartServiceCfm(gaia_transport *t, bool success);

/*! @brief Stop Gaia as a server on a given transport.
 */
void Gaia_TransportStopService(gaia_transport_type transport_type);
void Gaia_TransportStopServiceCfm(gaia_transport *t, bool success);

/*! @brief Indiation from transport of an error.
 */
void Gaia_TransportErrorInd(gaia_transport *t, gaia_transport_error error);

/*! @brief Find transport that supports specified feature(s)
 */
gaia_transport *Gaia_TransportFindFeature(gaia_transport_type start, uint8 feature);

/*! @brief Find transport of a specific type
 */
gaia_transport *Gaia_TransportFindService(gaia_transport_type type, gaia_transport *start);

/*! @brief Get next transport starting at index
 */
gaia_transport *Gaia_TransportIterate(uint8_t *index);

/*! @brief Check if transport supports specific feature(s)
 */
bool Gaia_TransportHasFeature(gaia_transport *t, uint8 feature);


/*! @brief Transmit a Gaia packet over a given transport.
 */
bool Gaia_TransportSendPacket(gaia_transport *t,
                              uint16 vendor_id, uint16 command_id, uint16 status,
                              uint16 payload_length, const uint8 *payload);

bool Gaia_TransportSendDataPacket(gaia_transport *t,
                                  uint16 vendor_id, uint16 command_id, uint16 status,
                                  uint16 payload_length, const uint8 *payload);

/*! @brief Set data endpoint mode for a given transport. (Only supported for GATT curently)
 */
bool Gaia_TransportSetDataEndpointMode(gaia_transport *t, gaia_data_endpoint_mode_t mode);
gaia_data_endpoint_mode_t Gaia_TransportGetDataEndpointMode(gaia_transport *t);
gaia_data_endpoint_mode_t Gaia_TransportGetPayloadDataEndpointMode(gaia_transport *t, uint16 size_payload, const uint8 *payload);

/*! @brief Get client/user data for transport */
uint32 Gaia_TransportGetClientData(gaia_transport *t);

/*! @brief Set client/user data for transport */
void Gaia_TransportSetClientData(gaia_transport *t, uint32 client_data);

/*! @brief Calculate total packet length for a packet */
uint16 Gaia_TransportCommonCalcRxPacketLength(uint16 size_payload, uint8 flags);
uint16 Gaia_TransportCommonCalcTxPacketLength(uint16 size_payload, uint8 status);

/*! @brief Build a packet into specified buffer */
void Gaia_TransportCommonBuildPacket(const uint8 protocol_version, uint8 *pkt_buf, const uint16 pkt_length,
                                     const uint16 vendor_id, const uint16 command_id, const uint8 status,
                                     const uint16 size_payload, const uint8 *payload);

/*! @brief Handle received packet */
uint16 Gaia_TransportCommonReceivePacket(gaia_transport *transport, const uint8 protocol_version, uint16 data_length, const uint8 *data_buf,
                                         bool (*pkt_callback)(gaia_transport *transport, const uint16 pkt_size, uint16 vendor_id, uint16 command_id, uint16 size_payload, const uint8 *payload));

/*! @brief Get transport information */
bool Gaia_TransportGetInfo(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value);

/*! @brief Set transport parameter */
bool Gaia_TransportSetParameter(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value);

/*! Inform transport that packet has been handled so that any resource can be freed */
void Gaia_TransportPacketHandled(gaia_transport *t, uint16 payload_size, const void *payload);

/*! Process a GAIA command or pass it up as unhandled */
void Gaia_ProcessCommand(gaia_transport *transport, uint16 vendor_id, uint16 command_id, uint16 size_payload, const uint8 *payload);


/*! Command has been handled, must be called in response to GAIA_COMMAND_IND*/
void Gaia_CommandResponse(gaia_transport *t, uint16 size_payload, const void *payload);


bool Gaia_IsGattCommandEndpoint(uint16 handle);
bool Gaia_IsGattDataEndpoint(uint16 handle);
bool Gaia_IsGattResponseClientConfig(uint16 handle);
bool Gaia_IsGattDataClientConfig(uint16 handle);
bool Gaia_IsGattResponseEndpoint(uint16 handle);
uint16 Gaia_GetGattResponseEndpoint(void);
uint16 Gaia_GetGattDataEndpoint(void);
uint16 Gaia_GetGattGaiaService(void);
uint16 Gaia_GetGattGaiaServiceEnd(void);


/*! @brief Gets the data endpoint mode
    @retval Data endpoint mode
*/
gaia_data_endpoint_mode_t Gaia_GetDataEndpointMode(GAIA_TRANSPORT *transport);


/*! @brief Tunnel a VM Upgrade Protocol command to the upgrade library
    @param mode Endpoint mode
    @retval True if the mode is valid otherwise send error with GAIA_STATUS_INVALID_PARAMETER
*/
bool Gaia_SetDataEndpointMode(GAIA_TRANSPORT *transport, gaia_data_endpoint_mode_t mode);


/*! @brief Gets the data endpoint mode for specified payload
    @retval Data endpoint mode
*/
gaia_data_endpoint_mode_t Gaia_GetPayloadDataEndpointMode(GAIA_TRANSPORT *transport, uint16 size_payload, const uint8 *payload);


#endif /* ifndef _GAIA_H_ */
