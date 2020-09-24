/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   device_test_service Device Test Service
\ingroup    services
\brief      The Device Test Service provides a service for device testing and
            configuration.

@{
*/

#ifndef DEVICE_TEST_SERVICE_H
#define DEVICE_TEST_SERVICE_H

#include "domain_message.h"


/*! Size of the key used in authentication for the device test service */
/*! @{ */
#define DTS_KEY_SIZE_BITS           128
#define DTS_KEY_SIZE_OCTETS         (DTS_KEY_SIZE_BITS/8)
#define DTS_KEY_SIZE_WORDS          (DTS_KEY_SIZE_OCTETS / 2)
#define DTS_KEY_SIZE_HEX_NIBBLES    (DTS_KEY_SIZE_OCTETS * 2)
/*! @} */

/*! Maximum content length for a single response. 

    Longer responses can be sent using DeviceTestService_CommandResponsePartial()
 */
#define DEVICE_TEST_SERVICE_MAX_RESPONSE_LEN    128

typedef enum
{
    DEVICE_TEST_SERVICE_ENDED = DEVICE_TEST_MESSAGE_BASE,
} device_test_service_message_t;


#ifdef INCLUDE_DEVICE_TEST_SERVICE

/*! \brief Initialises the device test service.

    \param init_task Not used

    \return TRUE
 */
bool DeviceTestService_Init(Task init_task);


/*! Query whether test mode is selected

    \return TRUE if test mode is enabled at present 
 */
bool DeviceTestService_TestMode(void);


/*! Save the test mode, so will apply when next read 

    Used to preserve an updated test mode so that when the
    device reboots, the new mode can be used.

    \param mode The value to save
 */
void DeviceTestService_SaveTestMode(uint16 mode);


/*! Send a string response to the last command

    The task passed should match that sent in the function call.

    Responses will be terminated with \\r\\n automatically.

    The response supplied should be a normal C string, with a zero
    termination. The supplied length is used so it would be 
    legal to send a string with no termination if the length is 
    correct.

        'O''K', length 2
        "OKAY", length 2 will just use "OK"

    \note The maximum response length is limited. Consider the limit to be about
        100 characters (\see #DEVICE_TEST_SERVICE_MAX_RESPONSE_LEN).
        If a response is longer than this then 
        DeviceTestService_CommandResponsePartial should be used for the 
        response.

    \note The string sent in the response will be the smallest of
        * Its actual length as a null-terminated string
        * The length supplied as a parameter
        * #DEVICE_TEST_SERVICE_MAX_RESPONSE_LEN

    \note Interim responses such as this should eventually be followed
            by an OK (or ERROR)

    \see DeviceTestService_CommandResponseOk

    \param task The task information sent for the incoming command
    \param response The response to be sent.
    \param length The length of the response. 0 is allowed, in which case 
            the length of the string is used.
 */
void DeviceTestService_CommandResponse(Task task, const char *response, unsigned length);

/*! Send part of a single response to the last command

    The task passed should match that sent in the function call.

    Most responses should be less than 100 characters, \see 
    #DEVICE_TEST_SERVICE_MAX_RESPONSE_LEN for the exact limit). Responses
    within that limit should use DeviceTestService_CommandResponse() 
    which adds necessary characters at the beginning and end of the message.

    If a response is longer than the limit then this function
    can be used to separate the response into multiple parts. The first
    part of the response should have first_part set TRUE, and the final 
    part should have last_part set TRUE.

    \note The string sent in the response will be the smallest of
        * Its actual length as a null-terminated string
        * The length supplied as a parameter
        * #DEVICE_TEST_SERVICE_MAX_RESPONSE_LEN

    \note This function can be called with both first_part and last_part 
    set TRUE. This is equivalent to DeviceTestService_CommandResponse().

    \see DeviceTestService_CommandResponse() for other details of
    parameters.

    \param task         The task information sent for the incoming command
    \param text         The partial response to be sent.
    \param length       The length of the response. 0 is allowed, in which case 
                        the length of the string is used.
    \param first_part   This is the first part of a single response to a command
    \param last_part    This is the final part of a single respone to a command
 */
void DeviceTestService_CommandResponsePartial(Task task, 
                                              const char *response, unsigned length,
                                              bool first_part,
                                              bool last_part);


/*! Send an OK response for the current command

    The task passed should match that sent in the function call.
    Only one OK or Error should be sent in response to a command.

    \see DeviceTestService_CommandResponseError
    \see DeviceTestService_CommandResponseOkOrError

    \param task The task information sent for the incoming command
 */
void DeviceTestService_CommandResponseOk(Task task);


/*! Send an ERROR response for the current command

    The task passed should match that sent in the function call.
    Only one Ok or Error should be sent in response to a command.

    \see DeviceTestService_CommandResponseOk
    \see DeviceTestService_CommandResponseOkOrError

    \param task The task information sent for the incoming command
 */
void DeviceTestService_CommandResponseError(Task task);


/*! Send an OK or ERROR response for the current command based on parameter

    The task passed should match that sent in the function call.
    Only one Ok or Error should be sent in response to a command.

   \see DeviceTestService_CommandResponseOk
   \see DeviceTestService_CommandResponseError

   \param task The task information sent for the incoming command
   \param success Whether we are meant to be representing a success or not.
            TRUE ==> "OK", FALSE ==> "ERROR"
*/
void DeviceTestService_CommandResponseOkOrError(Task task, bool success);


/*! Start the device test service. It will autonomously try to 
    create a connection until stopped.

    A message \ref DEVICE_TEST_SERVICE_ENDED will be sent to the 
    app_task when the device test service is terminated. 

    The service can be terminated by
    \li a call to DeviceTestService_Stop
    \li the Device Test Service in response to a test command 

    \param app_task The task to receive messages from the device test
                    service
 */
void DeviceTestService_Start(Task app_task);


/*! Stop the device test service. Any existing session will terminate.

    When stopped, a DEVICE_TEST_SERVICE_ENDED message will be sent to
    the application task.

    \param app_task The task to receive messages from the device test
                    service. This must be the same as the task supplied
                    in the DeviceTestService_Start command. It is 
                    provided as a parameter here in case the service has
                    already stopped.
 */
void DeviceTestService_Stop(Task app_task);


/*! Report whether the device test service is active

    \return TRUE if the device test service is enabled and active (trying to
            connect or connected), FALSE otherwise
 */
bool DeviceTestService_IsActive(void);


/* Include documentation for example file */
/*!
    \example{lineno} device_test_service_auth_example.py

    This is an example using Python 2.7. 

    The Pycryptoplus library is used
    because the more common pycrypto library does not support AES-CMAC. Python 3 has
    native support for AES-CMAC, but requires the keys in a different format.
*/


#else
#define DeviceTestService_TestMode() FALSE
#define DeviceTestService_SaveTestMode(_mode) (UNUSED(_mode))

#define DeviceTestService_Start(_task) (UNUSED(_task))
#define DeviceTestService_Stop(_task)  (UNUSED(_task))

#define DeviceTestService_CommandResponse(_task, _response, _length) \
                            (UNUSED(_task), UNUSED(_response), UNUSED(_length))
#define DeviceTestService_CommandResponsePartial(_task, _response, _length, _first, _last) \
                            (UNUSED(_task), UNUSED(_response), UNUSED(_length), UNUSED(_first), UNUSED(_last))

#define DeviceTestService_CommandResponseOk(_task) (UNUSED(_task))
#define DeviceTestService_CommandResponseError(_task) (UNUSED(_task))
#define DeviceTestService_CommandResponseOkOrError(_task, _success) (UNUSED(_task), UNUSED(_success))

#define DeviceTestService_IsActive() FALSE

#endif /* INCLUDE_DEVICE_TEST_SERVICE */

#endif /* DEVICE_TEST_SERVICE_H */

/*! @} End of group documentation */

