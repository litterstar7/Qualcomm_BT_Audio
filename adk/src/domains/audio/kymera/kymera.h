/*!
\copyright  Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     The Kymera Manager API

*/

#ifndef KYMERA_H
#define KYMERA_H

#include <chain.h>
#include <transform.h>
#include <hfp.h>
#include <a2dp.h>
#include <anc.h>
#include <audio_plugin_common.h>
#include <kymera_config.h>
#include <rtime.h>
#include "va_audio_types.h"
#include "aec_leakthrough.h"
#include "ringtone/ringtone_if.h"
#include "file/file_if.h"
#include "usb_audio.h"


/*! \brief List of all supported audio chains. */
typedef struct
{
    const chain_config_t *chain_aptx_ad_tws_plus_decoder_config;
    const chain_config_t *chain_aptx_mono_no_autosync_decoder_config;
    const chain_config_t *chain_sbc_mono_no_autosync_decoder_config;
    const chain_config_t *chain_aac_stereo_decoder_left_config;
    const chain_config_t *chain_aac_stereo_decoder_right_config;
    const chain_config_t *chain_forwarding_input_sbc_left_config;
    const chain_config_t *chain_forwarding_input_sbc_right_config;
    const chain_config_t *chain_forwarding_input_sbc_left_no_pcm_buffer_config;
    const chain_config_t *chain_forwarding_input_sbc_right_no_pcm_buffer_config;
    const chain_config_t *chain_forwarding_input_aptx_left_config;
    const chain_config_t *chain_forwarding_input_aptx_right_config;
    const chain_config_t *chain_forwarding_input_aptx_left_no_pcm_buffer_config;
    const chain_config_t *chain_forwarding_input_aptx_right_no_pcm_buffer_config;
    const chain_config_t *chain_forwarding_input_aac_left_config;
    const chain_config_t *chain_forwarding_input_aac_right_config;
    const chain_config_t *chain_forwarding_input_aac_stereo_left_config;
    const chain_config_t *chain_forwarding_input_aac_stereo_right_config;
    const chain_config_t *chain_input_aac_stereo_mix_config;
    const chain_config_t *chain_input_sbc_stereo_mix_config;
    const chain_config_t *chain_input_aptx_stereo_mix_config;
    const chain_config_t *chain_aec_config;
    const chain_config_t *chain_output_volume_config;
    const chain_config_t *chain_tone_gen_config;
    const chain_config_t *chain_tone_gen_no_iir_config;
    const chain_config_t *chain_prompt_decoder_config;
    const chain_config_t *chain_prompt_decoder_no_iir_config;
    const chain_config_t *chain_prompt_pcm_config;
    const chain_config_t *chain_sco_nb_config;
    const chain_config_t *chain_sco_wb_config;
    const chain_config_t *chain_sco_swb_config;
    const chain_config_t *chain_micfwd_wb_config;
    const chain_config_t *chain_micfwd_nb_config;
    const chain_config_t *chain_scofwd_wb_config;
    const chain_config_t *chain_scofwd_nb_config;
    const chain_config_t *chain_sco_nb_2mic_config;
    const chain_config_t *chain_sco_wb_2mic_config;
    const chain_config_t *chain_sco_swb_2mic_config;
    const chain_config_t *chain_sco_nb_3mic_config;
    const chain_config_t *chain_sco_wb_3mic_config;
    const chain_config_t *chain_sco_swb_3mic_config;
    const chain_config_t *chain_sco_nb_2mic_binaural_config;
    const chain_config_t *chain_sco_wb_2mic_binaural_config;
    const chain_config_t *chain_sco_swb_2mic_binaural_config;
    const chain_config_t *chain_micfwd_wb_2mic_config;
    const chain_config_t *chain_micfwd_nb_2mic_config;
    const chain_config_t *chain_scofwd_wb_2mic_config;
    const chain_config_t *chain_scofwd_nb_2mic_config;
    const chain_config_t *chain_micfwd_send_config;
    const chain_config_t *chain_scofwd_recv_config;
    const chain_config_t *chain_micfwd_send_2mic_config;
    const chain_config_t *chain_scofwd_recv_2mic_config;
    const chain_config_t *chain_aanc_config;
    const chain_config_t *chain_aanc_resampler_sco_tx_path_config;
    const chain_config_t *chain_aanc_splitter_sco_rx_path_config;
    const chain_config_t *chain_aanc_splitter_sco_tx_path_config;
    const chain_config_t *chain_aanc_resampler_sco_rx_path_config;
    const chain_config_t *chain_aanc_consumer;
    const chain_config_t *chain_input_aptx_adaptive_stereo_mix_config;
    const chain_config_t *chain_input_aptx_adaptive_stereo_mix_q2q_config;
    const chain_config_t *chain_input_sbc_stereo_config;
    const chain_config_t *chain_input_aptx_stereo_config;
    const chain_config_t *chain_input_aptxhd_stereo_config;
    const chain_config_t *chain_input_aptx_adaptive_stereo_config;
    const chain_config_t *chain_input_aptx_adaptive_stereo_q2q_config;
    const chain_config_t *chain_input_aac_stereo_config;
    const chain_config_t *chain_music_processing_config;
    const chain_config_t *chain_input_wired_analog_stereo_config;
    const chain_config_t *chain_input_usb_stereo_config;
    const chain_config_t *chain_usb_voice_wb_config;
    const chain_config_t *chain_usb_voice_wb_2mic_config;
    const chain_config_t *chain_usb_voice_wb_2mic_binaural_config;
    const chain_config_t *chain_usb_voice_nb_config;
    const chain_config_t *chain_usb_voice_nb_2mic_config;
    const chain_config_t *chain_usb_voice_nb_2mic_binaural_config;
    const chain_config_t *chain_mic_resampler_config;
} kymera_chain_configs_t;

typedef struct
{
    /*! Pointer to the definition of tone being played. */
    const ringtone_note *tone;
} KYMERA_NOTIFICATION_TONE_STARTED_T;

typedef struct
{
    /*! File index of the voice prompt being played. */
    FILE_INDEX id;
} KYMERA_NOTIFICATION_PROMPT_STARTED_T;

enum kymera_messages
{
    /*! A tone have started */
    KYMERA_NOTIFICATION_TONE_STARTED = KYMERA_MESSAGE_BASE,
    /*! A voice prompt have started */
    KYMERA_NOTIFICATION_PROMPT_STARTED,
    /*! Latency reconfiguration has completed */
    KYMERA_LATENCY_MANAGER_RECONFIG_COMPLETE_IND
};

/*! \brief The kymera module states. */
typedef enum app_kymera_states
{
    /*! Kymera is idle. */
    KYMERA_STATE_IDLE,
    /*! Starting master A2DP kymera in three steps. */
    KYMERA_STATE_A2DP_STARTING_A,
    KYMERA_STATE_A2DP_STARTING_B,
    KYMERA_STATE_A2DP_STARTING_C,
    /*! Starting slave A2DP kymera. */
    KYMERA_STATE_A2DP_STARTING_SLAVE,
    /*! Kymera is streaming A2DP locally. */
    KYMERA_STATE_A2DP_STREAMING,
    /*! Kymera is streaming A2DP locally and forwarding to the slave. */
    KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING,
    /*! Kymera is streaming SCO locally. */
    KYMERA_STATE_SCO_ACTIVE,
    /*! Kymera is streaming SCO locally, and may be forwarding */
    KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING,
    /*! Kymera is receiving forwarded SCO over a link */
    KYMERA_STATE_SCO_SLAVE_ACTIVE,
    /*! Kymera is playing a tone or a prompt. */
    KYMERA_STATE_TONE_PLAYING,
    /*! Kymera is performing ANC tuning. */
    KYMERA_STATE_ANC_TUNING,
    /*! Kymera is performing Adaptive ANC */
    KYMERA_STATE_ADAPTIVE_ANC_STARTED,
    /*! Kymera is running standlone leakthrough Chain.*/
    KYMERA_STATE_STANDALONE_LEAKTHROUGH,
    /*! Kymera is performing a loopback. */
    KYMERA_STATE_MIC_LOOPBACK,
    /*! Kymera is playing wired audio, could be analog/USB */
    KYMERA_STATE_WIRED_AUDIO_PLAYING,
    /*! Kymera is performing USB Audio. */
    KYMERA_STATE_USB_AUDIO_ACTIVE,
    KYMERA_STATE_USB_VOICE_ACTIVE
} appKymeraState;


/*! \brief The kymera module concurrent states. */
typedef enum app_kymera_concurrent_states
{
    /*! No Concurrency */
    KYMERA_CON_STATE_NONE,
    /*! Kymera Concurrent with Adaptive ANC */
    KYMERA_CON_STATE_WITH_ADAPTIVE_ANC
} appKymeraConcurrentState;


typedef struct
{
    /* The info variable is  interpreted according to message that it is delievered to clients.
            event_id  for  AANC_EVENT_CLEAR 
            gain value for KYMERA_AANC_EVENT_ED_INACTIVE_GAIN_UNCHANGED
            flags recevied for KYMERA_AANC_EVENT_ED_ACTIVE
            NA    when KYMERA_AANC_EVENT_QUIET_MODE
        */
    uint16 info;
} kymera_aanc_event_msg_t;

typedef kymera_aanc_event_msg_t KYMERA_AANC_CLEAR_IND_T;
typedef kymera_aanc_event_msg_t KYMERA_AANC_ED_ACTIVE_TRIGGER_IND_T;
typedef kymera_aanc_event_msg_t KYMERA_AANC_ED_INACTIVE_TRIGGER_IND_T;
typedef kymera_aanc_event_msg_t KYMERA_AANC_QUIET_MODE_TRIGGER_IND_T;
typedef kymera_aanc_event_msg_t KYMERA_AANC_EVENT_IND_T;


/*! \brief Events sent by Anc_Statemanager to other modules. */
typedef enum
{
    KYMERA_AANC_ED_ACTIVE_TRIGGER_IND = KYMERA_MESSAGE_BASE,
    KYMERA_AANC_ED_INACTIVE_TRIGGER_IND,
    KYMERA_AANC_QUIET_MODE_TRIGGER_IND,
    KYMERA_AANC_ED_ACTIVE_CLEAR_IND,
    KYMERA_AANC_ED_INACTIVE_CLEAR_IND,
    KYMERA_AANC_QUIET_MODE_CLEAR_IND    
} kymera_msg_t;


/*! \brief Enumeration of microphone selection options.

    The enumeration numbering is important, they match
    the input numbering in the Switched Passthrough
    Consumer operator.
 */
typedef enum
{
    /*! Local earbud microphone selected. */
    MIC_SELECTION_LOCAL = 1,

    /*! Remote earbud microphone selected. */
    MIC_SELECTION_REMOTE = 2,
} micSelection;

typedef enum
{
    NO_SCO,
    SCO_NB,
    SCO_WB,
    SCO_SWB,
    SCO_UWB
} appKymeraScoMode;

typedef struct
{
    appKymeraScoMode mode;
    bool sco_fwd;
    bool mic_fwd;
    uint8 mic_cfg;
    const chain_config_t *chain;
    uint32_t rate;   
} appKymeraScoChainInfo;

typedef struct
{
    va_audio_codec_t encoder;
    const chain_config_t *chain_config;
} appKymeraVaEncodeChainInfo;

typedef struct
{
    const appKymeraVaEncodeChainInfo *chain_table;
    unsigned table_length;
} appKymeraVaEncodeChainTable;

typedef struct
{
    va_wuw_engine_t       wuw_engine;
    const chain_config_t *chain_config;
} appKymeraVaWuwChainInfo;

typedef struct
{
    const appKymeraVaWuwChainInfo *chain_table;
    unsigned table_length;
} appKymeraVaWuwChainTable;

typedef struct
{
    bool  wake_up_word_detection;
    bool  clear_voice_capture;
    uint8 number_of_mics; 
    const chain_config_t *chain_config;
} appKymeraVaMicChainInfo;

typedef struct
{
    const appKymeraVaMicChainInfo *chain_table;
    unsigned table_length;
} appKymeraVaMicChainTable;

typedef void (* KymeraVoiceCaptureStarted)(Source capture_source);

/*! \brief Response to a Wake-Up-Word detected indication */
typedef struct
{
    bool start_capture;
    KymeraVoiceCaptureStarted capture_callback;
    va_audio_wuw_capture_params_t capture_params;
} kymera_wuw_detected_response_t;

typedef kymera_wuw_detected_response_t (* KymeraWakeUpWordDetected)(const va_audio_wuw_detection_info_t *wuw_info);


/*! Structure defining a single hardware output */
typedef struct
{
    /*! The type of output hardware */
    audio_hardware hardware;
    /*! The specific hardware instance used for this output */
    audio_instance instance;
    /*! The specific channel used for the output */
    audio_channel  channel;
} appKymeraHardwareOutput;

#define MAX_NUMBER_SUPPORTED_MICS 4

#ifdef INCLUDE_MIRRORING
/*! \brief Enumeration of kymera audio sync states */
typedef enum
{
    /*!  default state */
    KYMERA_AUDIO_SYNC_STATE_INIT,
    /*! audio sync is in progress */
    KYMERA_AUDIO_SYNC_STATE_IN_PROGRESS,
     /*! audio sync has completed*/
    KYMERA_AUDIO_SYNC_STATE_COMPLETE
}appKymeraAudioSyncState;

/*! \brief Enumeration of kymera audio sync states */
typedef enum
{
    KYMERA_AUDIO_SYNC_START_PRIMARY_UNSYNCHRONISED,
    KYMERA_AUDIO_SYNC_START_PRIMARY_SYNCHRONISED,
    KYMERA_AUDIO_SYNC_START_SECONDARY_SYNCHRONISED,
    KYMERA_AUDIO_SYNC_START_SECONDARY_JOINS_SYNCHRONISED,
    KYMERA_AUDIO_SYNC_START_Q2Q
}appKymeraAudioSyncStartMode;

/*! \brief Kymera audio synchronisation information structure */
typedef struct
{
    appKymeraAudioSyncStartMode mode;
    appKymeraAudioSyncState state;
    Source source;
} appKymeraAudioSyncInfo;
#endif /* INCLUDE_MIRRORING */

/*! \brief Kymera instance structure.

    This structure contains all the information for Kymera audio chains.
*/
typedef struct
{
    /*! The kymera module's task. */
    TaskData          task;
    /*! The current state. */
    appKymeraState    state;
    /*! List of tasks registered for notifications */
    task_list_t *client_tasks;

    /*! Task registered to receive notifications. */
    Task listener;
    /* Concurrent State*/
    appKymeraConcurrentState concurrent_state;
    
    /*! The input chain is used in TWS master and slave roles for A2DP streaming
        and is typified by containing a decoder. */
    kymera_chain_handle_t chain_input_handle;
    /*! The tone chain is used when a tone is played. */
    kymera_chain_handle_t chain_tone_handle;

	/*! The music processing chain. It implements things like EQ.
		It is inserted between input and output chains.	*/
    kymera_chain_handle_t chain_music_processing_handle;

    /*! The output_vol_handle/sco_handle chain are used mutually exclusively.
        The output_vol_handle/sco_handle both contain OPR_SOURCE_SYNC/OPR_VOLUME_CONTROL.
        These chains are unioned to simplify volume control code: output_vol_handle
        can always be used with OPR_VOLUME_CONTROL regardless of whether SCO/A2DP is active. */
    union
    {
        /*! Used in TWS master and slave roles for A2DP streaming */
        kymera_chain_handle_t output_vol_handle;
        /*! Used for SCO audio. */
        kymera_chain_handle_t sco_handle;
        /*! Used for USB voice. */
        kymera_chain_handle_t usb_voice_handle;
    } chainu;

#ifdef INCLUDE_MIRRORING
    union{
        /*! The TWM hash transform generates a unique identifier for packets received
         * over the air from A2DP source device. It can be configured to construct and
         * append a RTP header to packet before writing them to the audio subsystem.
         */
        Transform hash;
        /* P0 Packetiser for use with Q2Q Mode */
        Transform packetiser;
    }hashu;

    /*! TWM convert clock transform used to convert TTP info (in local system
     * time) available in source stream into bluetooth wallclock time before
     * writing to sink stream.
     */
    Transform convert_ttp_to_wc;

    /*! TWM convert clock transform used to convert TTP info (in bluetooth wallclock
     * time) available in source stream into local system time before writing to sink
     * stream.
     */
    Transform convert_wc_to_ttp;

    /* Audio sync information */
    appKymeraAudioSyncInfo sync_info;

    /* A2DP media source */
    Source media_source;

#else
    /*! The TWS master packetiser transform packs compressed audio frames
        (SBC, AAC, aptX) from the audio subsystem into TWS packets for transmission
        over the air to the TWS slave.
        The TWS slave packetiser transform receives TWS packets over the air from
        the TWS master. It unpacks compressed audio frames and writes them to the
        audio subsystem. */
    Transform packetiser;
#endif /* INCLUDE_MIRRORING */
    /*! The current output sample rate. */
    uint32 output_rate;

    /*! A lock bitfield. Internal messages are typically sent conditionally on
        this lock meaning events are queued until the lock is cleared. */
    uint16 lock;
    uint16 busy_lock;

    /*! The current A2DP stream endpoint identifier. */
    uint8  a2dp_seid;

    /*! The current playing tone client's lock. */
    uint16 *tone_client_lock;

    /*! The current playing tone client lock mask - bits to clear in the lock
         when the tone is stopped. */
    uint16 tone_client_lock_mask;

    /*! Number of tones/prompts playing and queued up to be played */
    uint8 tone_count;

    /*! Which microphone to use during mic forwarding */
    micSelection mic;
    const appKymeraScoChainInfo *sco_info;
    
    /*! The prompt file source whilst prompt is playing */
    Source prompt_source;

    anc_mic_params_t anc_mic_params;
    uint8 dac_amp_usage;

    /*! ANC tuning state */
    uint16 usb_rate;
    BundleID anc_tuning_bundle_id;

#ifdef DOWNLOAD_USB_AUDIO
    BundleID usb_audio_bundle_id;
#endif
    Operator usb_rx, anc_tuning, usb_tx;

#ifdef ENABLE_ADAPTIVE_ANC
    BundleID aanc_tuning_bundle_id;
    Operator aanc_tuning;
#endif

    /* If TRUE, a mono mix of the left/right audio channels will be rendered.
       If FALSE, either the left or right audio channel will be rendered. */
    unsigned enable_left_right_mix : 1;

    unsigned cp_header_enabled : 1;

    unsigned enable_cvc_passthrough : 1;

    unsigned q2q_mode;
#ifdef INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN
    /*! In Standalone ANC (none audio chains active) the passthrough operator will be connected to a DAC to suppress spurious tones  */
    Operator anc_passthough_operator;
#endif
} kymeraTaskData;

typedef struct
{
    usb_voice_mode_t mode;
    uint8 mic_cfg;
    const chain_config_t *chain;
    uint32_t rate;   
} appKymeraUsbVoiceChainInfo;

#define VOLUME_MUTE_IN_DB  (-90)

/*!< State data for the DSP configuration */
extern kymeraTaskData  app_kymera;

/*! Get pointer to Kymera structure */
#define KymeraGetTaskData()  (&app_kymera)

/*! Get task from the Kymera structure */
#define KymeraGetTask()    (&app_kymera.task)

/*! \brief Start streaming A2DP audio.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when A2DP is started, or if an A2DP stop is requested,
           before A2DP has started.
    \param client_lock_mask A mask of bits to clear in the client_lock.
    \param codec_settings The A2DP codec settings.
    \param max_bitrate The max bitrate for the input stream (in bps). Ignored if zero.
    \param volume_in_db The start volume.
    \param master_pre_start_delay This function always sends an internal message
    to request the module start kymera. The internal message is sent conditionally
    on the completion of other activities, e.g. a tone. The caller may request
    that the internal message is sent master_pre_start_delay additional times before the
    start of kymera commences. The intention of this is to allow the caller to
    delay the starting of kymera (with its long, blocking functions) to match
    the message pipeline of some concurrent message sequence the caller doesn't
    want to be blocked by the starting of kymera. This delay is only applied
    when starting the 'master' (a non-TWS sink SEID).
    \param q2q_mode The source device is a Qualcomm device that supports Q2Q mode
*/
void appKymeraA2dpStart(uint16 *client_lock, uint16 client_lock_mask,
                        const a2dp_codec_settings *codec_settings,
                        uint32 max_bitrate,
                        int16 volume_in_db, uint8 master_pre_start_delay,
                        uint8 q2q_mode, aptx_adaptive_ttp_latencies_t nq2q_ttp);

/*! \brief Stop streaming A2DP audio.
    \param seid The stream endpoint ID to stop.
    \param source The source associatied with the seid.
*/
void appKymeraA2dpStop(uint8 seid, Source source);

/*! \brief Set the A2DP streaming volume.
    \param volume_in_db.
*/
void appKymeraA2dpSetVolume(int16 volume_in_db);

/*! \brief Start SCO audio.
    \param audio_sink The SCO audio sink.
    \param codec WB-Speech codec bit masks.
    \param wesco The link Wesco.
    \param volume_in_db The starting volume.
    \param pre_start_delay This function always sends an internal message
    to request the module start SCO. The internal message is sent conditionally
    on the completion of other activities, e.g. a tone. The caller may request
    that the internal message is sent pre_start_delay additional times before
    starting kymera. The intention of this is to allow the caller to
    delay the start of kymera (with its long, blocking functions) to match
    the message pipeline of some concurrent message sequence the caller doesn't
    want to be blocked by the starting of kymera.
    \param allow_scofwd Allow the use of SCO forwarding if it is supported by
    this device.
*/
bool appKymeraScoStart(Sink audio_sink, appKymeraScoMode mode, bool *allow_scofwd, bool *allow_micfwd,
                       uint8 wesco, int16 volume_in_db, uint8 pre_start_delay);


/*! \brief Stop SCO audio.
*/
void appKymeraScoStop(void);

/*! \brief Start a chain for receiving forwarded SCO audio.

    \param link_source      The source used for data received from the SCO forwarding link
    \param volume_in_db     volume level to use
    \param enable_mic_fwd   Should MIC forwarding be enabled or not.
    \param pre_start_delay  Maximum number of times message is resent before being actioned
*/
void appKymeraScoSlaveStart(Source link_source, int16 volume_in_db, bool enable_mic_fwd,
                                 uint16 pre_start_delay);

/*! \brief Stop chain receiving forwarded SCO audio.
*/
void appKymeraScoSlaveStop(void);

/*! \brief Start sending forwarded audio.

    \param forwarding_sink The BT sink to the air.
    \param enable_mic_fwd Should MIC forwarding be enabled or not.

    \note If the SCO is to be forwarded then the full chain,
    including local playback, is started by this call.
*/
void appKymeraScoStartForwarding(Sink forwarding_sink, bool enable_mic_fwd);

/*! \brief Stop sending received SCO audio to the peer

    Local playback of SCO will continue, although in most cases
    will be stopped by a separate command almost immediately.
 */
void appKymeraScoStopForwarding(void);

/*! \brief Enable/Disable generation of forwarded SCO audio.

    \param[in] pause If TRUE will stop generating SCO audio data, if
                      FALSE will resume.

    This function leaves the SCO/SFWD chains up and running and flips
    the switched passthrough consumer between 'consume' and 'passthrough'
    modes to control if forwarded SCO data generated on the stream
    source for the SCOFWD packetiser to send.
*/
void appKymeraScoForwardingPause(bool pause);

/*! \brief Set SCO volume.

    \param[in] volume_in_db.
 */
void appKymeraScoSetVolume(int16 volume_in_db);

/*! \brief Enable or disable MIC muting.

    \param[in] mute TRUE to mute MIC, FALSE to unmute MIC.
 */
void appKymeraScoMicMute(bool mute);

/*! \brief Get the SCO CVC voice quality.
    \return The voice quality.
 */
uint8 appKymeraScoVoiceQuality(void);

/*! \brief Select the local Microphone for use

    Switches audio from remote (forwarded) Microphone to local Mic src.
 */
void appKymeraScoUseLocalMic(void);

/*! \brief Select the remote Microphone for use

    Switches audio from local Microphone to the remote (forwarded) Mic src.
 */
void appKymeraScoUseRemoteMic(void);


/*! \brief Play a tone.
    \param tone The address of the tone to play.
    \param ttp Time to play the audio tone in microseconds.
    \param interruptible If TRUE, the tone may be interrupted by another event
           before it is completed. If FALSE, the tone may not be interrupted by
           another event and will play to completion.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when the tone finishes - either on completion, or when
           interrupted.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void appKymeraTonePlay(const ringtone_note *tone, rtime_t ttp, bool interruptible,
                       uint16 *client_lock, uint16 client_lock_mask);

/*! \brief The prompt file format */
typedef enum prompt_format
{
    PROMPT_FORMAT_PCM,
    PROMPT_FORMAT_SBC,
} promptFormat;

/*! \brief Play a prompt.
    \param prompt The file index of the prompt to play.
    \param format The prompt file format.
    \param rate The prompt sample rate.
    \param ttp The time to play the audio prompt in microseconds.
    \param interruptible If TRUE, the prompt may be interrupted by another event
           before it is completed. If FALSE, the prompt may not be interrupted by
           another event and will play to completion.
    \param client_lock If not NULL, bits set in client_lock_mask will be cleared
           in client_lock when the prompt finishes - either on completion, or when
           interrupted.
    \param client_lock_mask A mask of bits to clear in the client_lock.
*/
void appKymeraPromptPlay(FILE_INDEX prompt, promptFormat format,
                         uint32 rate, rtime_t ttp, bool interruptible,
                         uint16 *client_lock, uint16 client_lock_mask);

/*! \brief Stop playing an active tone or prompt 

    Cancel/stop the currently playing tone or prompt.

    \note This command will only cancel tones and prompts that are allowed
        to be interrupted. This is specified in the interruptible parameter
        used when playing a tone/prompt.

    \note This API should not normally be used. Tones and prompts have a 
        limited duration and will end within a reasonable timescale. 
        Starting a new tone/prompt will also cancel any currently active tone.
*/
void appKymeraTonePromptCancel(void);


/*! \brief Initialise the kymera module. */
bool appKymeraInit(Task init_task);

/*! \brief Helper function that checks if the Kymera sub-system is idle

    Checking this does not guarantee that a subsequent function call that starts
    kymera activity will succeed.

    \return TRUE if the kymera sub-system was in the idle state at the time
                the function was called, FALSE otherwise.
 */
bool Kymera_IsIdle(void);

/*! \brief Register a Task to receive notifications from Kymera.

    Once registered, #client_task will receive #shadow_profile_msg_t messages.

    \param client_task Task to register to receive shadow_profile notifications.
*/
void Kymera_ClientRegister(Task client_task);

/*! \brief Un-register a Task that is receiving notifications from Kymera.

    If the task is not currently registered then nothing will be changed.

    \param client_task Task to un-register from shadow_profile notifications.
*/
void Kymera_ClientUnregister(Task client_task);



/*! \brief Configure downloadable capabilities bundles.

    This function must be called before appKymeraInit(),
    otherwise no downloadable capabilities will be loaded.

    \param config Pointer to bundle configuration.
*/
void Kymera_SetBundleConfig(const capability_bundle_config_t *config);

/*! \brief Populate all audio chains configuration.

    Number of audio chains used and its specific configuration may depend
    on an application. Only pointers to audio chains used in particular application
    have to be populated.

    This function must be called before audio is used.

    \param configs Audio chains configuration.
*/
void Kymera_SetChainConfigs(const kymera_chain_configs_t *configs);

/*! \brief Get audio chains configuration.

    \return Audio chains configuration.
*/
const kymera_chain_configs_t *Kymera_GetChainConfigs(void);

/*! \brief Set table used to determine audio chain based on SCO parameters.

    Table set by this function applies to primary TWS device as well as standalone device.

    This function must be called before audio is used.

    \param info SCO audio chains mapping.
*/
void Kymera_SetScoChainTable(const appKymeraScoChainInfo *info);

/*! \brief Set table used to determine audio chain based on SCO parameters.

    Table set by this function applies to secondary TWS device.

    This function must be called before audio is used.

    \param info SCO audio chains mapping.
*/
void Kymera_SetScoSlaveChainTable(const appKymeraScoChainInfo *info);

/*! \brief Set table used to determine audio chain based on VA encode parameters.

    Table set by this function applies to primary TWS device as well as standalone device.

    This function must be called before audio is used.

    \param chain_table VA encode chains mapping.
    */
void Kymera_SetVaEncodeChainTable(const appKymeraVaEncodeChainTable *chain_table);

/*! \brief Set table used to determine audio chain based on VA Mic parameters.

    Table set by this function applies to primary TWS device as well as standalone device.

    This function must be called before audio is used.

    \param chain_table VA mic chains mapping.
    */
void Kymera_SetVaMicChainTable(const appKymeraVaMicChainTable *chain_table);

/*! \brief Set table used to determine audio chain based on VA Wake Up Word parameters.

    Table set by this function applies to primary TWS device as well as standalone device.

    This function must be called before audio is used.

    \param chain_table Wake Up Word chains mapping.
    */
void Kymera_SetVaWuwChainTable(const appKymeraVaWuwChainTable *chain_table);

/*! \brief Set the left/right mixer mode
    \param stereo_lr_mix If TRUE, a 50/50 mix of the left and right stereo channels
    will be output by the mixer to the local device, otherwise, the left/right-ness
    of the earbud will be used to output 100% l/r.
*/
void appKymeraSetStereoLeftRightMix(bool stereo_lr_mix);

/*! \brief Enable or disable an external amplifier.
    \param enable TRUE to enable, FALSE to disable.
*/
void appKymeraExternalAmpControl(bool enable);

/*! \brief Start Anc tuning procedure.
           Note that Device has to be enumerated as USB audio device before calling this API.
    \param usb_rate the input sample rate of audio data
    \return void
*/
void KymeraAnc_EnterTuning(uint16 usb_rate);

/*! \brief Stop ANC tuning procedure.
    \param void
    \return void
*/
void KymeraAnc_ExitTuning(void);

/*! \brief Cancel any pending KYMERA_INTERNAL_A2DP_START message.
    \param void
    \return void
*/
void appKymeraCancelA2dpStart(void);
#define appKymeraIsTonePlaying() (KymeraGetTaskData()->tone_count > 0)

/*! \brief Prospectively start the DSP (if not already started).
    After a period, the DSP will be automatically stopped again if no activity
    is started */
void appKymeraProspectiveDspPowerOn(void);

#ifdef INCLUDE_MIRRORING
/*! \brief Set the primary/secondary synchronised start time
 *  \param clock Local clock synchronisation time instant
 */
void appKymeraA2dpSetSyncStartTime(uint32 clock);

/*!
 * \brief kymera_a2dp_mirror_handover_if
 *
 *        Struct containing interface function pointers for marshalling and handover
 *        operations.  See handover_if library documentation for more information.
 */
extern const handover_interface kymera_a2dp_mirror_handover_if;
#endif /* INCLUDE_MIRRORING */

/*! \brief Connects passthrough operator to dac in order to mitigate tonal noise
    observed in QCC512x devices*/
void KymeraAnc_ConnectPassthroughSupportChainToDac(void);

/*! \brief Disconnects passthrough operator from dac used in conjunction with
     KymeraAncConnectPassthroughSupportChainToDac(void) */
void KymeraAnc_DisconnectPassthroughSupportChainFromDac(void);

/*! \brief Creates passthrough operator support chain used for mitigation of tonal
    noise observed with QCC512x devices*/
void KymeraAnc_CreatePassthroughSupportChain(void);

/*! \brief Destroys passthrough operator support chain which could have been used with
    Qcc512x devices for mitigating tonal noise*/
void KymeraAnc_DestroyPassthroughSupportChain(void);

/*! \brief Start voice capture.
    \param callback Called when capture has started.
    \param params Parameters based on which the voice capture will be configured.
*/
void Kymera_StartVoiceCapture(KymeraVoiceCaptureStarted callback, const va_audio_voice_capture_params_t *params);

/*! \brief Stop voice capture.
*/
void Kymera_StopVoiceCapture(void);

/*! \brief Start Wake-Up-Word detection.
    \param callback Called when Wake-Up-Word is detected, a voice capture can be started based on the return.
    \param params Parameters based on which the Wake-Up-Word detection will be configured.
*/
void Kymera_StartWakeUpWordDetection(KymeraWakeUpWordDetected callback, const va_audio_wuw_detection_params_t *params);

/*! \brief Stop Wake-Up-Word detection.
*/
void Kymera_StopWakeUpWordDetection(void);

/*! \brief Get the version number of the WuW engine operator.
    \param version The version number to be populated.
*/
void Kymera_GetWakeUpWordEngineVersion(va_audio_wuw_engine_version_t *version);

/*! \brief Updates the DSP clock to the fastest possible speed in case of
 * A2DP and SCO before enabling the ANC and changing the mode and revert back to the previous
 * clock speed later to reduce the higher latency in peer sync
 */
void KymeraAnc_UpdateDspClock(void);

/*! \brief Get current Seid */
#define Kymera_GetCurrentSeid() (KymeraGetTaskData()->a2dp_seid)

/*! \brief Register for notification

    Registered task will receive KYMERA_NOTIFICATION_* messages.

    As it is intended to be used by test system it supports only one client.
    That is to minimise memory use.
    It can be extended to support arbitrary number of clients when necessary.

    \param task Listener task
*/
void Kymera_RegisterNotificationListener(Task task);

/*! Kymera API references for software leak-through */
/*! \brief Enables the AEC sidetone after a given timeout */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughEnableAecSideToneAfterTimeout(void);
#else
#define Kymera_LeakthroughEnableAecSideToneAfterTimeout() ((void)(0))
#endif

/*! \brief Updates the AEC operator UCID */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughUpdateAecOperatorUcid(void);
#else
#define Kymera_LeakthroughUpdateAecOperatorUcid() ((void)(0))
#endif

/*! \brief Enables the leakthrough */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_EnableLeakthrough(void);
#else
#define Kymera_EnableLeakthrough() ((void)(0))
#endif

/*! \brief Tries to enable the Adaptive ANC chain */
void Kymera_EnableAdaptiveAnc(bool in_ear, audio_anc_path_id path, adaptive_anc_hw_channel_t hw_channel, anc_mode_t mode);
/*! \brief Tries to disable the Adaptive ANC chain */
void Kymera_DisableAdaptiveAnc(void);
/*! \breif Update Adaptive ANC InEar status */
void Kymera_UpdateAdaptiveAncInEarStatus(void);
/*! \breif Update Adaptive ANC InEar status */
void Kymera_UpdateAdaptiveAncOutOfEarStatus(void);
/*! \breif Enable Adaptive ANC Adaptivity */
void Kymera_EnableAdaptiveAncAdaptivity(void);
/*! \breif Disable Adaptive ANC Adaptivity */
void Kymera_DisableAdaptiveAncAdaptivity(void);
/*! \brief Returns if Adaptive ANC is enabled based on Kymera state */
bool Kymera_IsAdaptiveAncEnabled(void);
/*! \breif Get the Feed Forward gain */
void Kymera_GetApdativeAncFFGain(uint8 *gain);
/*! \breif Set the Adaptive ANC Mu Values */
void Kymera_SetApdativeAncGainValues(uint32 mantissa, uint32 exponent);

void Kymera_EnableAdaptiveAncGentleMute(void);
void Kymera_AdaptiveAncSetUcid(anc_mode_t new_mode);
void Kymera_AdaptiveAncApplyModeChange(anc_mode_t new_mode, audio_anc_path_id feedforward_anc_path, adaptive_anc_hw_channel_t hw_channel);

/*! \brief Mute the side tone path and disconnect the leakthrough microphone */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughMuteDisconnect(bool disconnect_mic);
#else
#define Kymera_LeakthroughMuteDisconnect(x) ((void)(0))
#endif

/*! \brief Connect the leakthrough microphone and setup the side tone path */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughConnectWithTone(void);
#else
#define Kymera_LeakthroughConnectWithTone() ((void)(0))
#endif

/*! \brief Update the AEC use-case and restart the side tone path */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_LeakthroughUpdateAecOperatorAndSidetone(void);
#else
#define Kymera_LeakthroughUpdateAecOperatorAndSidetone() ((void)(0))
#endif

/*! \brief Disable the leakthrough */
#ifdef ENABLE_AEC_LEAKTHROUGH
void Kymera_DisableLeakthrough(void);
#else
#define Kymera_DisableLeakthrough() ((void)(0))
#endif

/*! \brief Set up loop back from Mic input to DAC when kymera is ON

    \param mic_number select the mic to loop back
    \param sample_rate set the sampling rate for both input and output chain
 */
void appKymeraCreateLoopBackAudioChain(microphone_number_t mic_number, uint32 sample_rate);

/*! \brief Destroy loop back from Mic input to DAC chain

    \param mic_number select the mic to loop back

 */
void appKymeraDestroyLoopbackAudioChain(microphone_number_t mic_number);

/*! \brief Starts the wired analog audio chain
      \param volume_in_db Volume for wired analog
      \param rate sample rate at which wired analog audio needs to be played
      \param min_latency minimum latency identified for wired audio
      \param max_latency maximum latency identified for wired audio
      \param target_latency fixed latency identified for wired audio 
 */
void Kymera_StartWiredAnalogAudio(int16 volume_in_db, uint32 rate, uint32 min_latency, uint32 max_latency, uint32 target_latency);
/*! \brief Stop the wired analog audio chain
 */
void Kymera_StopWiredAnalogAudio(void);

/*! \brief Create and start USB Audio.
    \param channels USB Audio channels
    \param frame_size 16 bit/24bits.
    \param Sink USB OUT endpoint sink.
    \param volume_in_db Intal volume
    \param rate Sample Frequency.
    \param min_latency TTP minimum value in micro-seconds
    \param max_latency TTP max value in micro-seconds
    \param target_latency TTP default value in micro-seconds
*/
void appKymeraUsbAudioStart(uint8 channels, uint8 frame_size,
                            Source src, int16 volume_in_db,
                            uint32 rate, uint32 min_latency, uint32 max_latency,
                            uint32 target_latency);

/*! \brief Stop and destroy USB Audio chain.
    \param Source USB OUT endpoint source.
*/
void appKymeraUsbAudioStop(Source usb_src);

/*! \brief Set volume for USB Audio chain.
    \param volume_in_db Volume to be set.
*/
void appKymeraUsbAudioSetVolume(int16 volume_in_db);

/*! \brief Create and start USB Voice.
    \param cvc_2_mic 1-mic or 2-mic cvc config
    \param mic_sink Sink for mic USB TX endpoint.
    \param mode Type of mode (NB/WB etc)
    \param rate Sample Frequency.
    \param spkr_src Speaker Source of USB RX endpoint
    \param volume_in_db Initial volume
    \param min_latency TTP minimum value in micro-seconds
    \param max_latency TTP max value in micro-seconds
    \param target_latency TTP default value in micro-seconds
*/
void appKymeraUsbVoiceStart(Sink mic_sink,
            usb_voice_mode_t mode, uint32 sample_rate, Source spkr_src,
            int16 volume_in_db, uint32 min_latency, uint32 max_latency,
            uint32 target_latency);

/*! \brief Stop and destroy USB Audio chain.
    \param spkr_src USB OUT (from host) endpoint source.
    \param mic_sink USB IN (to host) endpoint sink.
*/
void appKymeraUsbVoiceStop(Source spkr_src, Sink mic_sink);

/*! \brief Set USB Voice volume.

    \param[in] volume_in_db.
 */
void appKymeraUsbVoiceSetVolume(int16 volume_in_db);

/*! \brief Enable or disable MIC muting.

    \param[in] mute TRUE to mute MIC, FALSE to unmute MIC.
*/
void appKymeraUsbVoiceMicMute(bool mute);

/*! \brief Prepare chains required to play prompt.

    \param[in] format The format of the prompt to prepare for
    \param[in] sample_rate The sample rate of the prompt to prepare for

    \return TRUE if chains prepared, otherwise FALSE
 */
bool Kymera_PrepareForPrompt(promptFormat format, uint16 sample_rate);

/*! \brief Check if chains have been prepared for prompt.

    \param[in] format The format of the prompt to play
    \param[in] sample_rate The sample rate of the prompt to play

    \return TRUE if ready for prompt, otherwise FALSE
 */
bool Kymera_IsReadyForPrompt(promptFormat format, uint16 sample_rate);

/*! \brief Get the stream transform connecting the A2DP media source to kymera
    \return The transform, or 0 if the A2DP audio chains are not active.
    \note This function will always return 0 if INCLUDE_MIRRORING is undefined.
*/
Transform Kymera_GetA2dpMediaStreamTransform(void);

#endif /* KYMERA_H */
