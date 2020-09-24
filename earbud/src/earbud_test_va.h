#ifndef EARBUD_TEST_VA_H
#define EARBUD_TEST_VA_H

#include <va_audio_types.h>

/*! \brief Send the VA Tap command
*/
void appTestVaTap(void);

/*! \brief Send the VA Double Tap command
*/
void appTestVaDoubleTap(void);

/*! \brief Send the VA Press and Hold command
*/
void appTestVaPressAndHold(void);

/*! \brief Send the VA Release command
*/
void appTestVaRelease(void);

/*! \brief Set the active voice assistant to GAA
*/
void EarbudTest_SetActiveVa2GAA(void);

/*! \brief Set the active voice assistant to AMA
*/
void EarbudTest_SetActiveVa2AMA(void);

/*! \brief VA audio function to be used outside a voice assistant protocol
           Simulates a real case of PTT or TTT in terms of audio
           Starts the capture.
    \param encoder Encoder to use for capturing the data
    \param num_of_mics Number of mics to attempt to use
    \return bool TRUE on success, FALSE otherwise
*/
bool EarbudTest_StartVaCapture(va_audio_codec_t encoder, unsigned num_of_mics);

/*! \brief VA audio function to be used outside a voice assistant protocol
           Simulates a real case of PTT or TTT in terms of audio
           Stops the capture.
    \return bool TRUE on success, FALSE otherwise
*/
bool EarbudTest_StopVaCapture(void);

/*! \brief VA audio function to be used outside a voice assistant protocol
           Simulates a real case of WuW detection.
           Starts the detection process.
    \param wuw_engine Engine to use for the detection
    \param num_of_mics Number of mics to attempt to use
    \param start_capture_on_detection Whether it should start encoding and dropping audio data upon detection
    \param encoder Encoder to use for capturing the data, only relevant if a capture is started upon detection
    \return bool TRUE on success, FALSE otherwise
*/
bool EarbudTest_StartVaWakeUpWordDetection(va_wuw_engine_t wuw_engine, unsigned num_of_mics, bool start_capture_on_detection, va_audio_codec_t encoder);

/*! \brief VA audio function to be used outside a voice assistant protocol
           Simulates a real case of WuW detection.
           Stops the detection process.
    \return bool TRUE on success, FALSE otherwise
*/
bool EarbudTest_StopVaWakeUpWordDetection(void);

#endif // EARBUD_TEST_VA_H
