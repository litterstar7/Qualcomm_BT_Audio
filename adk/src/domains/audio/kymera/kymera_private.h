/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_private.h
\brief      Private (internal) kymera header file.

*/

#ifndef KYMERA_PRIVATE_H
#define KYMERA_PRIVATE_H

#include <a2dp.h>
#include <hfp.h>
#include <panic.h>
#include <stream.h>
#include <sink.h>
#include <source.h>
#include <chain.h>

#include "kymera.h"
#include "kymera_output_chain_config.h"
#include "kymera_aec.h"
#include "kymera_leakthrough.h"
#include "adk_log.h"
#include "kymera_chain_roles.h"
#include "latency_config.h"
#include "microphones.h"
#include <opmsg_prim.h>

/*! \brief Macro to help getting an operator from chain.
    \param op The returned operator, or INVALID_OPERATOR if the operator was not
           found in the chain.
    \param chain_handle The chain handle.
    \param role The operator role to get from the chain.
    \return TRUE if the operator was found, else FALSE
 */
#define GET_OP_FROM_CHAIN(op, chain_handle, role) \
    (INVALID_OPERATOR != ((op) = ChainGetOperatorByRole((chain_handle), (role))))

/*@{ \name System kick periods, in microseconds */
#define KICK_PERIOD_FAST (2000)
#define KICK_PERIOD_SLOW (7500)

#define KICK_PERIOD_SLAVE_SBC (KICK_PERIOD_SLOW)
#define KICK_PERIOD_SLAVE_APTX (KICK_PERIOD_SLOW)
#define KICK_PERIOD_SLAVE_AAC (KICK_PERIOD_SLOW)
#define KICK_PERIOD_MASTER_SBC (KICK_PERIOD_SLOW)
#define KICK_PERIOD_MASTER_AAC (KICK_PERIOD_SLOW)
#define KICK_PERIOD_MASTER_APTX (KICK_PERIOD_SLOW)
#define KICK_PERIOD_TONES (KICK_PERIOD_SLOW)
#define KICK_PERIOD_LEAKTHROUGH (KICK_PERIOD_SLOW)
#define KICK_PERIOD_VOICE (KICK_PERIOD_FAST) /*!< Use low latency for voice */
#define KICK_PERIOD_WIRED_ANALOG (KICK_PERIOD_SLOW)

#define KICK_PERIOD_SLAVE_APTX_ADAPTIVE (KICK_PERIOD_FAST)
#define KICK_PERIOD_MASTER_APTX_ADAPTIVE (KICK_PERIOD_FAST)
/*@} */

/*!@{ \name Defines used in setting up kymera messages
    Kymera operator messages are 3 words long, with the ID in the 2nd word */
#define KYMERA_OP_MSG_LEN                   (3)
#define KYMERA_OP_MSG_WORD_MSG_ID           (1)
#define KYMERA_OP_MSG_WORD_EVENT_ID         (3)
#define KYMERA_OP_MSG_WORD_PAYLOAD_0        (4)

#define KYMERA_OP_MSG_WORD_PAYLOAD_NA       (0xFFFF)


#define KYMERA_OP_MSG_ID_TONE_END           (0x0001) /*!< Kymera ringtone generator TONE_END message */
#define KYMERA_OP_MSG_ID_AANC_INFO          (0x0007) /*!< AANC Capability  INFO message Kalsim testing only */
#define KYMERA_OP_MSG_ID_AANC_EVENT_TRIGGER (0x0008) /*!< AANC Capability EVENT_TRIGGER message  */
#define KYMERA_OP_MSG_ID_AANC_EVENT_CLEAR   (0x0009) /*!< AANC Capability EVENT_CLEAR aka NEGATIVE message  */
/*!@}*/

#define KYMERA_AANC_EVENT_ED_INACTIVE_GAIN_UNCHANGED (0x0000) /*Gain unchanged for 5 seconds when EDs inactive*/
#define KYMERA_AANC_EVENT_ED_ACTIVE                  (0x0001) /*Either ED active for more than 5 seconds */
#define KYMERA_AANC_EVENT_QUIET_MODE                 (0x0002) /*Quiet mode */

/*!@}*/

/*! Kymera requires gain specified in unit of 1/60th db */
#define KYMERA_DB_SCALE (60)

/*! Helper macro to get size of fixed arrays to populate structures */
#define DIMENSION_AND_ADDR_OF(ARRAY) ARRAY_DIM((ARRAY)), (ARRAY)

/*! Maximum sample rate supported by this application */
#define MAX_SAMPLE_RATE (48000)
#define DEFAULT_MIC_RATE (8000)

#define APTX_MONO_CODEC_RATE_KBPS     (192)
#define APTX_STEREO_CODEC_RATE_KBPS   (384)
#define APTXHD_STEREO_CODEC_RATE_KBPS (576)
#define APTX_AD_CODEC_RATE_KBPS       (500)

/*! Default Sidetone step up time in milliseconds */
#define ST_GAIN_RAMP_STEP_TIME_MS  (25U)

/*! Maximum codec rate expected by this application */
#define MAX_CODEC_RATE_KBPS (APTXHD_STEREO_CODEC_RATE_KBPS)

/*!:{ \name Macros to calculate buffer sizes required to hold a specific (timed) amount of audio. */
#define CODEC_BITS_PER_MEMORY_WORD (16)
#define MS_TO_BUFFER_SIZE_MONO_PCM(time_ms, sample_rate) ((((time_ms) * (sample_rate)) + (MS_PER_SEC-1)) / MS_PER_SEC)
#define US_TO_BUFFER_SIZE_MONO_PCM(time_us, sample_rate) ((((time_us) * (sample_rate)) + (US_PER_SEC-1)) / US_PER_SEC)
#define MS_TO_BUFFER_SIZE_CODEC(time_ms, codec_rate_kbps) ((((time_ms) * (codec_rate_kbps)) + (CODEC_BITS_PER_MEMORY_WORD-1)) / CODEC_BITS_PER_MEMORY_WORD)
/*!@}*/

/*!@{ \name Buffer sizes required to hold enough audio to achieve the TTP latency */
#define PRE_DECODER_BUFFER_SIZE     (MS_TO_BUFFER_SIZE_CODEC(PRE_DECODER_BUFFER_MS, MAX_CODEC_RATE_KBPS))
#define PCM_LATENCY_BUFFER_SIZE     (MS_TO_BUFFER_SIZE_MONO_PCM(PCM_LATENCY_BUFFER_MS, MAX_SAMPLE_RATE))
#define APTX_LATENCY_BUFFER_SIZE    (MS_TO_BUFFER_SIZE_CODEC(PCM_LATENCY_BUFFER_MS - (appConfigTwsDeadline() / 1000), APTX_MONO_CODEC_RATE_KBPS))
/*!@}*/

/*! Convert x into 1.31 format */
#define FRACTIONAL(x) ( (int)( (x) * ((1l<<31) - 1) ))

/*!@{ \name Macros to set and clear bits in the lock. */
#define appKymeraSetToneLock(theKymera) (theKymera)->lock |= 1U
#define appKymeraClearToneLock(theKymera) (theKymera)->lock &= ~1U
#define appKymeraSetStartingLock(theKymera) (theKymera)->lock |= 2U
#define appKymeraClearStartingLock(theKymera) (theKymera)->lock &= ~2U
/*!@}*/

/*! Get current state */
#define appKymeraGetState() (KymeraGetTaskData()->state)
#define appKymeraAdaptiveAncStarted() (appKymeraGetState() == KYMERA_STATE_ADAPTIVE_ANC_STARTED)
#define appKymeraIsBusy()    (appKymeraGetState() != KYMERA_STATE_IDLE)
#define appKymeraInConcurrency() (KymeraGetTaskData()->concurrent_state == KYMERA_CON_STATE_WITH_ADAPTIVE_ANC)
#define appKymeraIsBusyStreaming() ((appKymeraGetState() == KYMERA_STATE_A2DP_STREAMING) || (appKymeraGetState() == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING))
#define appKymeraIsScoActive() ((appKymeraGetState() == KYMERA_STATE_SCO_ACTIVE) || (appKymeraGetState() == KYMERA_STATE_SCO_SLAVE_ACTIVE))

/*! Kymera ringtone generator has a fixed sample rate of 8 kHz */
#define KYMERA_TONE_GEN_RATE (8000)

/*! Default DAC disconnection delay in milliseconds */
#define appKymeraDacDisconnectionDelayMs() (30000)

/*! Default Adaptive ANC Microphone Sample Rate */ 
#define ADAPTIVE_ANC_MIC_SAMPLE_RATE    (16000)

/*! Default mic TTP latency needed for VA */
#define VA_MIC_TTP_LATENCY     (40000)

#define TTP_LATENCY_IN_US(latency_in_ms) (latency_in_ms * US_PER_MS)
#define TTP_BUFFER_SIZE 4096
/* Workaround for AEC duplicate outputs connection sequence */
#define ENABLE_AEC_MIC_INPUT_CONN_SEQ_WORKAROUND

/*! \brief The audio data format */
typedef enum
{
    ADF_GENERIC_ENCODED = 0,
    ADF_PCM = 1,
    ADF_16_BIT_WITH_METADATA = 2,
    ADF_GENERIC_ENCODED_32BIT = 13
} audio_data_format;

/*! \brief Switch passthrough consumer states */
typedef enum
{
    PASSTHROUGH_MODE,
    CONSUMER_MODE
} switched_passthrough_states;


/*! \brief Internal message IDs */
enum app_kymera_internal_message_ids
{
    /*! Internal A2DP start message. */
    KYMERA_INTERNAL_A2DP_START,
    /*! Internal A2DP starting message. */
    KYMERA_INTERNAL_A2DP_STARTING,
    /*! Internal A2DP stop message. */
    KYMERA_INTERNAL_A2DP_STOP,
    /*! Internal A2DP stop forwarding message. */
    KYMERA_INTERNAL_A2DP_STOP_FORWARDING,
    /*! Internal A2DP set volume message. */
    KYMERA_INTERNAL_A2DP_SET_VOL,
    /*! Internal SCO start message, including start of SCO forwarding (if supported). */
    KYMERA_INTERNAL_SCO_START,
    /*! Internal message to start SCO forwarding */
    KYMERA_INTERNAL_SCO_START_FORWARDING_TX,
    /*! Internal message to stop forwarding SCO */
    KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX,
    /*! Internal message to set SCO volume */
    KYMERA_INTERNAL_SCO_SET_VOL,
    /*! Internal SCO stop message. */
    KYMERA_INTERNAL_SCO_STOP,
    /*! Internal SCO microphone mute message. */
    KYMERA_INTERNAL_SCO_MIC_MUTE,
    /*! Internal message indicating that forwarded SCO (incoming) is/will be active */
    KYMERA_INTERNAL_SCO_SLAVE_START,
    /*! Internal message to stop playing forwarded SCO */
    KYMERA_INTERNAL_SCOFWD_RX_STOP,
    /*! Internal tone play message. */
    KYMERA_INTERNAL_TONE_PROMPT_PLAY,
    /*! Select the local microphone(s) for SCO */
    KYMERA_INTERNAL_MICFWD_LOCAL_MIC,
    /*! Select a forwarded microphone for SCO */
    KYMERA_INTERNAL_MICFWD_REMOTE_MIC,
    /*! Internal ANC tuning start message */
    KYMERA_INTERNAL_ANC_TUNING_START,
    /*! Internal ANC tuning stop message */
    KYMERA_INTERNAL_ANC_TUNING_STOP,
    /*! Internal Adaptive ANC tuning start message */
    KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START,
    /*! Internal Adaptive ANC tuning stop message */
    KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP,
    /*! Internal Adaptive ANC enable message */
    KYMERA_INTERNAL_AANC_ENABLE,
    /*! Internal Adaptive ANC disable message */
    KYMERA_INTERNAL_AANC_DISABLE,
    /*! Disable the audio SS (used for the DAC disable) */
    KYMERA_INTERNAL_AUDIO_SS_DISABLE,
    /*! Internal A2DP data sync indication timeout message */
    KYMERA_INTERNAL_A2DP_DATA_SYNC_IND_TIMEOUT,
    /*! Internal message indicating timeout waiting to receive #MESSAGE_MORE_DATA */
    KYMERA_INTERNAL_A2DP_MESSAGE_MORE_DATA_TIMEOUT,
    /*! Internal A2DP audio synchronisation message */
    KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED,
    /*! Internal kick to switch off audio subsystem after prospective DSP start */
    KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF,
    /*! Internal AEC LEAKTHROUGH Standalone chain creation message */
    KYMERA_INTERNAL_AEC_LEAKTHROUGH_CREATE_STANDALONE_CHAIN,
    /*! Internal AEC LEAKTHROUGH Standalone chain destroy message */
    KYMERA_INTERNAL_AEC_LEAKTHROUGH_DESTROY_STANDALONE_CHAIN,
    /*! Internal LEAKTHROUGH Side tone enable message */
    KYMERA_INTERNAL_LEAKTHROUGH_SIDETONE_ENABLE,
    /*! Internal LEAKTHROUGH Side tone gain for ramp up algorithm */
    KYMERA_INTERNAL_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP,
    /*! Message to trigger A2DP Audio Latency adjustment */
    KYMERA_INTERNAL_LATENCY_CHECK_TIMEOUT,
    /*! Message to trigger muting of audio during fast latency adjustment */
    KYMERA_INTERNAL_LATENCY_MANAGER_MUTE,
    /*! Message to inform completion of mute duraiton */
    KYMERA_INTERNAL_LATENCY_MANAGER_MUTE_COMPLETE,
    /*! Internal tone play message. */
    KYMERA_INTERNAL_TONE_PROMPT_STOP,
    /*! Internal message to start wired analog audio */
    KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START,
    /*! Internal message to stop wired analog audio */
    KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_STOP,
    /*! Internal USB Audio start message. */
    KYMERA_INTERNAL_USB_AUDIO_START,
    /*! Internal USB Audio stop message. */
    KYMERA_INTERNAL_USB_AUDIO_STOP,
    /*! Internal USB Audio set volume message. */
    KYMERA_INTERNAL_USB_AUDIO_SET_VOL,
    /*! Internal USB Voice start message. */
    KYMERA_INTERNAL_USB_VOICE_START,
    /*! Internal USB Voice stop message. */
    KYMERA_INTERNAL_USB_VOICE_STOP,
    /*! Internal message to set USB Voice volume */
    KYMERA_INTERNAL_USB_VOICE_SET_VOL,
    /*! Internal USB Voice microphone mute message. */
    KYMERA_INTERNAL_USB_VOICE_MIC_MUTE,
    /*! Internal message indicating timeout waiting for prompt play */
    KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT,
};

/*! \brief The KYMERA_INTERNAL_A2DP_START and KYMERA_INTERNAL_A2DP_STARTING message content. */
typedef struct
{
    /*! The client's lock. Bits set in lock_mask will be cleared when A2DP is started. */
    uint16 *lock;
    /*! The bits to clear in the client lock. */
    uint16 lock_mask;
    /*! The A2DP codec settings */
    a2dp_codec_settings codec_settings;
    /*! The starting volume */
    int16 volume_in_db;
    /*! The number of times remaining the kymera module will resend this message to
        itself (having entered the locked KYMERA_STATE_A2DP_STARTING) state before
        proceeding to commence starting kymera. Starting will commence when received
        with value 0. Only applies to starting the master. */
    uint8 master_pre_start_delay;
    /*! The max bitrate for the input stream (in bps). Ignored if zero. */
    uint32 max_bitrate;
    uint8 q2q_mode; /* 1 = Q2Q mode enabled, 0 = Generic Mode */
    aptx_adaptive_ttp_latencies_t nq2q_ttp;
} KYMERA_INTERNAL_A2DP_START_T;


/*! \brief The KYMERA_INTERNAL_A2DP_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
} KYMERA_INTERNAL_A2DP_SET_VOL_T;

/*! \brief The KYMERA_INTERNAL_A2DP_STOP and KYMERA_INTERNAL_A2DP_STOP_FORWARDING message content. */
typedef struct
{
    /*! The A2DP seid */
    uint8 seid;
    /*! The media sink */
    Source source;
} KYMERA_INTERNAL_A2DP_STOP_T;

/*! \brief The KYMERA_INTERNAL_SCO_START message content. */
typedef struct
{
    /*! The SCO audio sink. */
    Sink audio_sink;
    /*! Pointer to SCO chain information. */
    const appKymeraScoChainInfo *sco_info;
    /*! The link Wesco. */
    uint8 wesco;
    /*! The starting volume. */
    int16 volume_in_db;
    /*! The number of times remaining the kymera module will resend this message to
        itself before starting kymera SCO. */
    uint8 pre_start_delay;
} KYMERA_INTERNAL_SCO_START_T;

/*! \brief The KYMERA_INTERNAL_SCOFWD_RX_START message content. */
typedef struct
{
    /*! The audio source from the air */
    Source link_source;
    /*! Pointer to SCO chain information. */
    const appKymeraScoChainInfo *sco_info;
    /*! The starting volume */
    int16 volume_in_db;
    /*! The number of times remaining the kymera module will resend this message to
        itself before starting kymera SCO. */
    uint8 pre_start_delay;
} KYMERA_INTERNAL_SCO_SLAVE_START_T;

/*! \brief The KYMERA_INTERNAL_SCO_START_FORWARDING_TX message content. */
typedef struct
{
    /*! The BT sink to the air */
    Sink forwarding_sink;
    /*! TRUE if microphone forwarding is enabled */
    bool enable_mic_fwd;
} KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T;


/*! \brief The KYMERA_INTERNAL_SCO_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
} KYMERA_INTERNAL_SCO_SET_VOL_T;


/*! \brief The KYMERA_INTERNAL_SCO_MIC_MUTE message content. */
typedef struct
{
    /*! TRUE to enable mute, FALSE to disable mute. */
    bool mute;
} KYMERA_INTERNAL_SCO_MIC_MUTE_T;

/*! \brief KYMERA_INTERNAL_TONE_PLAY message content */
typedef struct
{
    /*! Pointer to the ringtone structure to play, NULL for prompt. */
    const ringtone_note *tone;
    /*! The prompt file index to play. FILE_NONE for tone. */
    FILE_INDEX prompt;
    /*! The prompt file format */
    promptFormat prompt_format;
    /*! The tone/prompt sample rate */
    uint32 rate;
    /*! The time to play the tone/prompt, in microseconds. */
    uint32 time_to_play;
    /*! If TRUE, the tone may be interrupted by another event before it is
        completed. If FALSE, the tone may not be interrupted by another event
        and will play to completion. */
    bool interruptible;
    /*! If not NULL, set bits in client_lock_mask will be cleared in client_lock
    when the tone is stopped. */
    uint16 *client_lock;
    /*! The mask of bits to clear in client_lock. */
    uint16 client_lock_mask;
} KYMERA_INTERNAL_TONE_PROMPT_PLAY_T;

typedef struct
{
    uint16 usb_rate;
} KYMERA_INTERNAL_ANC_TUNING_START_T;

typedef struct
{
    uint16 usb_rate;
} KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T;

/*! \brief The KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
    /*! sampling rate */
    uint32 rate;
    /*! configure the required latency range */
    uint32 min_latency;
    uint32 max_latency;
    uint32 target_latency;
} KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T;

typedef struct
{
    Source input_1;
    Source input_2;
} connect_audio_output_t;

/*!Structure that defines Adaptive ANC connection parameters */
typedef struct
{
    bool in_ear;                          /*! to provide in-ear / out-ear status to adaptive anc capability */
    audio_anc_path_id control_path;       /*! to decide if FFa path becomes control or FFb */
    adaptive_anc_hw_channel_t hw_channel; /*! Hadware instance to select */
    anc_mode_t current_mode;              /*Current ANC mode*/
}KYMERA_INTERNAL_AANC_ENABLE_T;

/**
 *  \brief Connect if both Source and Sink are valid.
 *  \param source The Source data will be taken from.
 *  \param sink The Sink data will be written to.
 *  \note In the case of connection failuar, it will panics the application.
 * */
void Kymera_ConnectIfValid(Source source, Sink sink);

/**
 *  \brief Break any existing automatic connection involving the source *or* sink.
 *   Source or sink may be NULL.
 *  \param source The source which needs to be disconnected.
 *  \param sink The sink which needs to be disconnected.
 * */
void Kymera_DisconnectIfValid(Source source, Sink sink);

/*! \brief The connectivity message for USB Audio. */
typedef struct
{
    uint8 channels;
    uint8 frame_size;
    Source spkr_src;
    int16 volume_in_db;
    uint32 sample_freq;
    uint32 min_latency_ms;
    uint32 max_latency_ms;
    uint32 target_latency_ms;
} KYMERA_INTERNAL_USB_AUDIO_START_T;

/*! \brief Disconnect message for USB Audio. */
typedef struct
{
    Source source;
} KYMERA_INTERNAL_USB_AUDIO_STOP_T;

/*! \brief The KYMERA_INTERNAL_USB_AUDIO_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
} KYMERA_INTERNAL_USB_AUDIO_SET_VOL_T;

/*! \brief The connectivity message for USB Voice. */
typedef struct
{
    usb_voice_mode_t mode;
    Source spkr_src;
    Sink mic_sink;
    uint32 sampleFreq;
    int16 volume;
    uint32 min_latency_ms;
    uint32 max_latency_ms;
    uint32 target_latency_ms;
} KYMERA_INTERNAL_USB_VOICE_START_T;

/*! \brief Disconnect message for USB Voice. */
typedef struct
{
     Source spkr_src;
     Sink mic_sink;
} KYMERA_INTERNAL_USB_VOICE_STOP_T;

/*! \brief The KYMERA_INTERNAL_USB_VOICE_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    int16 volume_in_db;
} KYMERA_INTERNAL_USB_VOICE_SET_VOL_T;


/*! \brief The KYMERA_INTERNAL_USB_VOICE_MIC_MUTE message content. */
typedef struct
{
    /*! TRUE to enable mute, FALSE to disable mute. */
    bool mute;
} KYMERA_INTERNAL_USB_VOICE_MIC_MUTE_T;


void appKymeraSetState(appKymeraState state);

/*! \brief Set an operator's terminal buffer sizes.
    \param op The operator.
    \param rate The sample rate is Hz.
    \param buffer_size_ms The amount of audio the buffer should contain in ms.
    \param input_terminals The input terminals for which the buffer size should be set (bitmask).
    \param output_terminals The output terminals for which the buffer size should be set (bitmask).
*/
void appKymeraSetTerminalBufferSize(Operator op, uint32 rate, uint32 buffer_size_ms,
                                           uint16 input_terminals, uint16 output_terminals);


const appKymeraScoChainInfo *appKymeraScoFindChain(const appKymeraScoChainInfo *info, appKymeraScoMode mode, bool sco_fwd, bool mic_fwd, uint8 mic_cfg);

/*! \brief Immediately stop playing the tone or prompt */
void appKymeraTonePromptStop(void);


/*! \brief Create and configure the audio output chain.
    \param kick_period The kymera kick period.
    \param buffer_size The PCM buffer size.
    \param volume_in_db The start volume.

    \note If buffer_size is zero, the buffer size is not configured.
    \note Default parameters will be set for other parameters in the kymera_output_chain_config.
*/
void KymeraOutput_CreateChain(unsigned kick_period, unsigned buffer_size, int16 volume_in_db);

/*! \brief Create and configure the audio output chain.
    \param config The output chain configuration.
*/
void KymeraOutput_CreateChainWithConfig(const kymera_output_chain_config *config);

/*! \brief Stop and destroy the audio output chain.
*/
void KymeraOutput_DestroyChain(void);

/*! \brief Configure the audio output chain.
    \param chain The chain containing the audio output chain operators.
    \sample_rate The sample rate to configure.
    \param kick_period The kymera kick period.
    \buffer_size The PCM buffer size.
    \volume_in_db The start volume.

    \note Configures the source sync, volume and latency buffer operators in the chain.

    \note Requires the chain to use standard roles for the operators, namely
    #OPR_SOURCE_SYNC,  #OPR_VOLUME_CONTROL and #OPR_LATENCY_BUFFER.

    \note If buffer_size is zero, the buffer size is not configured.

 */
void KymeraOutput_ConfigureChainOperators(kymera_chain_handle_t chain,
                                            uint32 sample_rate, unsigned kick_period,
                                            unsigned buffer_size, int16 volume_in_db);

/*! \brief Get output chain operator by role 
    \param operator_role The output chain operator role either input or output.
    return Operator if present else INVALID_OPERATOR.
 */
Operator KymeraOutput_ChainGetOperatorByRole(unsigned operator_role);

/*! \brief Start output chain
*/
void  KymeraOutput_ChainStart(void);

/*  \brief Connect audio output chain endpoints to appropriate hardware outputs
    \param left source of Left output channel
    \param right source of Right output channel if stereo output supported
    \param output_sample_rate The output sample rate to be set
*/
void Kymera_ConnectOutputSource(Source left, Source right,uint32 output_sample_rate );

/*! \brief Get output chain handle
 */
kymera_chain_handle_t KymeraOutput_GetOutputHandle(void);

/*! \brief Set the main and auxilliary volume for output chain.
    \param volume_in_db The volume to set.
*/
void KymeraOutput_SetVolume(int16 volume_in_db);

/*! \brief Set the main volume for audio output chain
    \param volume_in_db The volume to set.
*/
void KymeraOutput_SetMainVolume(int16 volume_in_db);

/*! \brief Get output chain source 
    return source Output chain source 
    \param output_role The audio output chain role as a source
 */
Source KymeraOutput_ChainGetOutput(unsigned output_role);

/*! \brief Connect audio output chain as a source.
    return TRUE if connection successful else FALSE.
    \param.source The source used for data received from the link
    \param input_role The audio output chain role
 */
bool KymeraOutput_ConnectSource(Source source, unsigned input_role);

/*! \brief Joins the input audio chain with output chain. And if music processing is enabled it shall be
              added in between the input & output chain
    \param theKymera pointer to the kymera data structure
 */
void appKymeraJoinInputOutputChains(kymeraTaskData *theKymera);

/*! \brief Set the main kymera volume.
    \param chain The chain containing the volume control operator.
    \param volume_in_db The volume to set.
*/
void appKymeraSetMainVolume(kymera_chain_handle_t chain, int16 volume_in_db);

/*! \brief Set the main and auxilliary kymera volume.
    \param chain The chain containing the volume control operator.
    \param volume_in_db The volume to set.
*/
void appKymeraSetVolume(kymera_chain_handle_t chain, int16 volume_in_db);

/*! \brief Setup an external amplifier. */
void appKymeraExternalAmpSetup(void);

/*! \brief Configure power mode and clock frequencies of the DSP for the lowest
           power consumption possible based on the current state / codec.

   \note Calling this function with chains already started may cause audible
   glitches if using I2S output.
*/
void appKymeraConfigureDspPowerMode(void);

/*! \brief Configure the active DSP clock.
    \return TRUE on success.
    \note Changing the clock with chains already started may cause audible
    glitches if using I2S output.
*/
bool appKymeraSetActiveDspClock(audio_dsp_clock_type type);

/*! \brief Convert volume on 0..127 scale to MIN_VOLUME_DB..MAX_VOLUME_DB and
    return the KYMERA_DB_SCALE value.
    \param volume Set to 0 to mute the output.
 */
int32 volTo60thDbGain(int16 volume);

/*! \brief Check if prompt is being played.
    \return TRUE for yes, FALSE for no.
 */
bool appKymeraIsPlayingPrompt(void);

/*! \brief Check if tone prompt is active on its own.
    \return TRUE for yes, FALSE for no.
 */
bool Kymera_TonePromptActiveAlone(void);

/*! \brief Handle request to play a tone or prompt.
    \param msg The request message.
*/
void appKymeraHandleInternalTonePromptPlay(const KYMERA_INTERNAL_TONE_PROMPT_PLAY_T *msg);

/*! \brief Handle request to start A2DP.
    \param msg The request message.
    \return TRUE if A2DP start is complete. FALSE if A2DP start is incomplete.
*/
bool appKymeraHandleInternalA2dpStart(const KYMERA_INTERNAL_A2DP_START_T *msg);

/*! \brief Handle request to stop A2DP.
    \param msg The request message.
*/
void appKymeraHandleInternalA2dpStop(const KYMERA_INTERNAL_A2DP_STOP_T *msg);

/*! \brief Handle request to set A2DP volume.
    \param volume_in_db The requested volume.
*/
void appKymeraHandleInternalA2dpSetVolume(int16 volume_in_db);

/*! \brief Registers Sco callbacks in the mic interface layer
 */
void Kymera_ScoInit(void);

/*! \brief Handle request to start SCO.
    \param audio_sink The BT SCO audio sink (source of SCO audio).
    \param codec The HFP codec type.
    \param wesco WESCO parameter.
    \param volume_in_db Initial volume.
    \param use_sco_fwd Set to TRUE to use SCO forwarding capable chain.
    \return TRUE if successfully able to start SCO else FALSE.
*/
bool appKymeraHandleInternalScoStart(Sink sco_sink, const appKymeraScoChainInfo *info,
                                     uint8 wesco, int16 volume_in_db);

/*! \brief Handle request to stop SCO. */
void appKymeraHandleInternalScoStop(void);

/*! \brief Handle request to set SCO volume.
    \param volume_in_db The requested volume.
*/
void appKymeraHandleInternalScoSetVolume(int16 volume_in_db);

/*! \brief Handle request to mute the SCO microphone.
    \param mute Set to TRUE to mute the microphone.
*/
void appKymeraHandleInternalScoMicMute(bool mute);

/*! \brief Registers Sco Forwarding callbacks in the mic interface layer
 */
void Kymera_ScoForwardingInit(void);

/*! \brief Handle request to start SCO forwarding transmit.
    \param forwarding_sink The BT L2CAP sink of forwarding audio.
*/
void appKymeraHandleInternalScoForwardingStartTx(Sink forwarding_sink);

/*! \brief Handle request to stop SCO forwarding transmit. */
bool appKymeraHandleInternalScoForwardingStopTx(void);

/*! \brief Helper function to send a message for appKymeraScoFwdStartReceive()

    \param link_source      The source used for data received from the SCO forwarding link
    \param volume_in_db     volume level to use
    \param enable_mic_fwd   Should MIC forwarding be enabled or not.
    \param pre_start_delay  Maximum number of times message is resent before being actioned
*/
void appKymeraScoSlaveStartHelper(Source link_source, int16 volume_in_db, const appKymeraScoChainInfo *info, uint16 delay);

/*! \brief Handle request to start SCO forwarding receive proessing on slave.
 *
    \param link_source      The source used for data received from the SCO forwarding link.
    \param info             The SCO chain info for setting up the chain.
    \param volume_in_db     volume level to use.
    \return TRUE if salve was able to start SCO else FALSE
*/
bool appKymeraHandleInternalScoSlaveStart(Source link_source, const appKymeraScoChainInfo *info, int16 volume_in_db);

/*! \brief Handle request to stop SCO forwarding receive processing on slave. */
void appKymeraHandleInternalScoSlaveStop(void);

/*! \brief Switch the microphone source to transmit to the AG. */
void appKymeraSwitchSelectMic(micSelection mic);

/*! \brief Set the SPC mode.
    \param op The SPC operator.
    \param is_consumer Consumer mode if is_consumer is TRUE otherwise passthrough mode. */
void appKymeraConfigureSpcMode(Operator op, bool is_consumer);

/*! \brief Set the SPC data format for SCO data.
    \param op The SPC operator.
    \param format The data format to set. */
void appKymeraConfigureScoSpcDataFormat(Operator op, audio_data_format format);

/*! \brief Set operator use case ids.
    \param is_sco Set TRUE if the use case is SCO based. Set to FALSE for A2DP use cases.
*/
void KymeraOutput_SetOperatorUcids(bool is_sco);

/*! \brief Creates the Kymera Tuning Chain
    \param sample rate.
*/
void KymeraAnc_TuningCreateChain(uint16 usb_rate);

/*! \brief Destroys the Kymera Tuning Chain
    \param -.
*/
void KymeraAnc_TuningDestroyChain(void);

/*! \brief Creates the Kymera Adaptive ANC Tuning Chain
    \param sample rate.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void kymeraAdaptiveAnc_CreateAdaptiveAncTuningChain(uint16 usb_rate);
#else
#define kymeraAdaptiveAnc_CreateAdaptiveAncTuningChain(x) ((void)(0*x))
#endif

/*! \brief Destroys the Kymera Adaptive ANC Tuning Chain
    \param -.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void kymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain(void);
#else
#define kymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain() ((void)(0))
#endif

/*! \brief Create a SCO chain.

    \param info The SCO chain info to use to create the chain.
*/
kymera_chain_handle_t appKymeraScoCreateChain(const appKymeraScoChainInfo *info);


kymera_chain_handle_t appKymeraGetScoChain(void);
Operator appKymeraGetMicSwitch(void);
void appKymeraSelectSpcSwitchInput(Operator op, micSelection input);
void OperatorsAwbsSetBitpoolValue(Operator op, uint16 bitpool, bool decoder);
unsigned AudioConfigGetMicrophoneBiasVoltage(mic_bias_id id);
void AudioConfigSetRawDacGain(audio_output_t channel, uint32 raw_gain);

Source Kymera_GetMicrophoneSource(microphone_number_t microphone_number, Source source_to_synchronise_with, uint32 sample_rate,
                                        microphone_user_type_t microphone_user_type);
void Kymera_CloseMicrophone(microphone_number_t microphone_number, microphone_user_type_t microphone_user_type);


/*! \brief Derive Optimal microphone sample rate if Adaptive ANC is supported in device

    \param sample_rate  The sample rate for the specified use case e.g. SCO or Voice capture
*/
uint32 appKymeraGetOptimalMicSampleRate(uint32 sample_rate);

/*! \brief return the number of microphones used

    \param None
    \return number of microphones, default is 1
*/
uint8 Kymera_GetNumberOfMics(void);


#ifdef INCLUDE_MIRRORING
void appKymeraA2dpHandleDataSyncIndTimeout(void);
void appKymeraA2dpHandleMessageMoreDataTimeout(void);
void appKymeraA2dpHandleAudioSyncStreamInd(MessageId id, Message msg);
void appKymeraA2dpHandleAudioSynchronisedInd(void);
void appKymeraA2dpHandleMessageMoreData(const MessageMoreData *mmd);
#endif /* INCLUDE_MIRRORING */

/*! \brief returns the AEC REF UCID */
kymera_operator_ucid_t Kymera_GetAecUcid(void);

/*! \brief Get AEC Operator */
Operator Kymera_GetAecOperator(void);

/*! \brief Sets the AEC reference use-case*/
void Kymera_SetAecUseCase(aec_usecase_t usecase);

aec_usecase_t appKymeraGetAecUseCase(void);

/*! \brief returns the AEC REF UCID */
kymera_operator_ucid_t Kymera_GetAecUcid(void);

/*! \brief Get AEC Operator */
Operator Kymera_GetAecOperator(void);

/*! \brief Sets the AEC reference use-case*/
void Kymera_SetAecUseCase(aec_usecase_t usecase);

/*! \brief Initialises leakthrough kymera task*/
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughInit(void);
#else
#define Kymera_LeakthroughInit() ((void)(0))
#endif

/*! \brief Enables/Disables the AEC sidetone path */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughEnableAecSideTonePath(bool enable);
#else
#define Kymera_LeakthroughEnableAecSideTonePath(x) ((void)(0))
#endif

/*! \brief Returns the status of standalone leakthrough */
#ifdef ENABLE_AEC_LEAKTHROUGH
bool Kymera_IsStandaloneLeakthroughActive(void);
#else
#define Kymera_IsStandaloneLeakthroughActive() (FALSE)
#endif

/*! \brief Stops the leakthrough chain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughStopChainIfRunning(void);
#else
#define Kymera_LeakthroughStopChainIfRunning() ((void)(0))
#endif

/*! \brief Resumes the leakthrough chain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughResumeChainIfSuspended(void);
#else
#define Kymera_LeakthroughResumeChainIfSuspended() ((void)(0))
#endif

/*! \brief Sets the minimum leakthrough sidetone gain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_SetMinLeakthroughSidetoneGain(void);
#else
#define Kymera_SetMinLeakthroughSidetoneGain() ((void)(0))
#endif

/*! \brief Creates the leakthrough chain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_CreateLeakthroughChain(void);
#else
#define Kymera_CreateLeakthroughChain() ((void)(0))
#endif

/*! \brief Destroys the leakthrough chain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_DestroyLeakthroughChain(void);
#else
#define Kymera_DestroyLeakthroughChain() ((void)(0))
#endif

/*! \brief Connects the leakthrough mic */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_ConnectLeakthroughMic(void);
#else
#define Kymera_ConnectLeakthroughMic() ((void)(0))
#endif

/*! \brief Step up the leakthrough sidetone gain */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughStepupSTGain(void);
#else
#define Kymera_LeakthroughStepupSTGain() ((void)(0))
#endif

/*! \brief Reset sidetone gain index */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughResetGainIndex(void);
#else
#define Kymera_LeakthroughResetGainIndex() ((void)(0))
#endif


/*! \brief Disconnects the leakthrough mic */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_DisconnectLeakthroughMic(void);
#else
#define Kymera_DisconnectLeakthroughMic() ((void)(0))
#endif

/*! \brief sets the Leakthrough mic sample rate */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_setLeakthroughMicSampleRate(uint32 sample_rate);
#else
#define Kymera_setLeakthroughMicSampleRate(x) ((void)(0))
#endif

#ifdef INCLUDE_WIRED_ANALOG_AUDIO
/*! \brief Create wired analog chain and start playing the audio      
      \param msg internal message which has the configuration for wired analog chain */
void KymeraWiredAnalog_StartPlayingAudio(const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *msg);
#else
#define KymeraWiredAnalog_StartPlayingAudio(msg) ((void)(0))
#endif

#ifdef INCLUDE_WIRED_ANALOG_AUDIO
/*! \brief Destroy the wired audio chain */
void KymeraWiredAnalog_StopPlayingAudio(void);
#else
#define KymeraWiredAnalog_StopPlayingAudio() ((void)(0))
#endif

#endif /* KYMERA_PRIVATE_H */
