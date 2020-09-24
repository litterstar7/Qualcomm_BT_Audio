/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private header for ANC functionality
*/

#ifndef KYMERA_ANC_H_
#define KYMERA_ANC_H_

#include <kymera.h>

#ifdef INCLUDE_STEREO
#define getAncFeedForwardRightMic()   (appConfigAncFeedForwardRightMic())
#define getAncFeedBackRightMic()      (appConfigAncFeedBackRightMic())
#define getAncFeedForwardLeftMic()    (appConfigAncFeedForwardLeftMic())
#define getAncFeedBackLeftMic()       (appConfigAncFeedBackLeftMic())

#define getAncTuningMonitorLeftMic()  (appConfigAncTuningMonitorLeftMic())
#define getAncTuningMonitorRightMic() (appConfigAncTuningMonitorRightMic())
#else
#define getAncFeedForwardRightMic()   (microphone_none)
#define getAncFeedBackRightMic()      (microphone_none)
#define getAncFeedForwardLeftMic()    (appConfigAncFeedForwardMic())
#define getAncFeedBackLeftMic()       (appConfigAncFeedBackMic())

#define getAncTuningMonitorLeftMic()  (appConfigAncTuningMonitorMic())
#define getAncTuningMonitorRightMic() (microphone_none)
#endif

/*!
 * \brief Makes the support chain ready for ANC hardware. applicable only for QCC512x devices
 * \param appKymeraState state current kymera state.
 *
 */
#if defined INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN && defined ENABLE_ANC
void KymeraAnc_PreStateTransition(appKymeraState state);
#else
#define KymeraAnc_PreStateTransition(x) ((void)(x))
#endif

#endif /* KYMERA_ANC_H_ */
