/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_audio.c
\brief  Implementation of audio functionality for Amazon Voice Service
*/

#ifdef INCLUDE_AMA
#include "ama.h"
#include "ama_config.h"
#include "ama_audio.h"
#include "ama_data.h"
#include "ama_speech.h"
#include "ama_rfcomm.h"
#include "ama_voice_ui_handle.h"
#include <voice_ui_va_client_if.h>
#include "ama_ble.h"
#include "ama_speech.h"
#include <source.h>
#include <stdlib.h>

#define PRE_ROLL_US 500000UL

#define AmaAudio_GetOpusFrameSize(config)  ((config->u.opus_req_kbps == AMA_OPUS_16KBPS) ? 40 : 80);
#define AmaAudio_SetAudioFormat(config) ((config->u.opus_req_kbps == AMA_OPUS_16KBPS) ?\
                                                    AmaSpeech_SetAudioFormat(ama_audio_format_opus_16khz_16kbps_cbr_0_20ms) :\
                                                    AmaSpeech_SetAudioFormat(ama_audio_format_opus_16khz_32kbps_cbr_0_20ms))
typedef struct _ama_current_locale
{
    const char *name;
    FILE_INDEX file_index;
}ama_current_locale_t;

/* Function pointer to send the encoded voice data */
static bool (*amaAudio_SendVoiceData)(Source src);

static ama_current_locale_t current_locale = {.file_index = FILE_NONE, .name = AMA_DEFAULT_LOCALE};
#ifdef INCLUDE_WUW
static const char *locale_ids[] = {AMA_AVAILABLE_LOCALES};
#endif

inline static void amaAudio_SetCurrentLocaleFileIndex(void)
{
    PanicZero(current_locale.file_index = FileFind(FILE_ROOT, current_locale.name, strlen(current_locale.name)));
}

static bool amaAudio_SendMsbcVoiceData(Source source)
{
    #define AMA_HEADER_LEN 4
    #define MSBC_ENC_PKT_LEN 60
    #define MSBC_FRAME_LEN 57
    #define MSBC_FRAME_COUNT 5
    #define MSBC_BLE_FRAME_COUNT 1

    uint8 frames_to_send;
    uint16 payload_posn;
    uint16 lengthSourceThreshold;
    uint8 *buffer = NULL;
    uint8 no_of_transport_pkt = 0;
    uint8 initial_position = 0;

    bool sent_if_necessary = FALSE;

    if(AmaData_GetActiveTransport() == ama_transport_ble)
    {
        frames_to_send = MSBC_BLE_FRAME_COUNT;
        initial_position = AMA_HEADER_LEN - 1;
    }
    else
    {
        frames_to_send = MSBC_FRAME_COUNT;
        initial_position = AMA_HEADER_LEN;
    }

    lengthSourceThreshold = MSBC_ENC_PKT_LEN * frames_to_send;

    while ((SourceSize(source) >= (lengthSourceThreshold + 2)) && no_of_transport_pkt < 3)
    {
        const uint8 *source_ptr = SourceMap(source);
        uint32 copied = 0;
        uint32 frame;
        uint16 length;

        if(!buffer)
            buffer = PanicUnlessMalloc((MSBC_FRAME_LEN * frames_to_send) + AMA_HEADER_LEN);

        payload_posn = initial_position;
        
        for (frame = 0; frame < frames_to_send; frame++)
        {
            memmove(&buffer[payload_posn], &source_ptr[(frame * MSBC_ENC_PKT_LEN) + 2], MSBC_FRAME_LEN);
            payload_posn += MSBC_FRAME_LEN;
            copied += MSBC_FRAME_LEN;
        }

        length = AmaProtocol_PrepareVoicePacket(buffer, copied);

        sent_if_necessary = Ama_SendData(buffer, length);

        if(sent_if_necessary)
        {
            SourceDrop(source, lengthSourceThreshold);
        }
        else
        {
            break;
        }

        no_of_transport_pkt++;
    }

    free(buffer);

    DEBUG_LOG_V_VERBOSE("amaAudio_SendMsbcVoiceData: %d bytes remaining", SourceSize(source));

    return sent_if_necessary;
}

static bool amaAudio_SendOpusVoiceData(Source source)
{
    /* Parameters used by Opus codec*/
    #define AMA_OPUS_HEADER_LEN         3
    #define OPUS_16KBPS_ENC_PKT_LEN     40
    #define OPUS_32KBPS_ENC_PKT_LEN     80
    #define OPUS_16KBPS_LE_FRAME_COUNT      4
    #define OPUS_16KBPS_RFCOMM_FRAME_COUNT  5
    #define OPUS_32KBPS_RFCOMM_FRAME_COUNT  3
    #define OPUS_32KBPS_LE_FRAME_COUNT      2

    uint16 payload_posn;
    uint16 lengthSourceThreshold;
    uint8 *buffer = NULL;
    bool sent_if_necessary = FALSE;
    uint8 no_of_transport_pkt = 0;
    ama_transport_t transport;
    uint16 opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN; /* Make complier happy. */
    uint16 opus_frame_count = OPUS_16KBPS_RFCOMM_FRAME_COUNT;

    transport = AmaData_GetActiveTransport();

    switch(AmaSpeech_GetAudioFormat())
    {
        case AUDIO_FORMAT__OPUS_16KHZ_16KBPS_CBR_0_20MS :

            if(transport == ama_transport_rfcomm)
            {
                opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_16KBPS_RFCOMM_FRAME_COUNT;
            }
            else
            {
                opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_16KBPS_LE_FRAME_COUNT;
            }
            break;

        case AUDIO_FORMAT__OPUS_16KHZ_32KBPS_CBR_0_20MS :

            if(transport == ama_transport_rfcomm)
            {
                opus_enc_pkt_len = OPUS_32KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_32KBPS_RFCOMM_FRAME_COUNT;
            }
            else
            {
                opus_enc_pkt_len = OPUS_32KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_32KBPS_LE_FRAME_COUNT;
            }
            break;

        case AUDIO_FORMAT__PCM_L16_16KHZ_MONO :
        case AUDIO_FORMAT__MSBC:
        default:
            DEBUG_LOG_ERROR("amaAudio_SendOpusVoiceData: Unexpected audio format");
            Panic();
            break;
    }

    lengthSourceThreshold = (opus_frame_count * opus_enc_pkt_len);

    while (SourceSize(source) && (SourceSize(source) >= lengthSourceThreshold) && (no_of_transport_pkt < 3))
    {
        const uint8 *opus_ptr = SourceMap(source);
        uint16 frame;
        uint16 copied = 0;
        uint16 length;

        if(!buffer)
            buffer = PanicUnlessMalloc((lengthSourceThreshold) + 3);

        payload_posn = AMA_OPUS_HEADER_LEN;

        for (frame = 0; frame < opus_frame_count; frame++)
        {
            memmove(&buffer[payload_posn], &opus_ptr[(frame*opus_enc_pkt_len)], opus_enc_pkt_len);
            payload_posn += opus_enc_pkt_len;
            copied += opus_enc_pkt_len;
        }

        length = AmaProtocol_PrepareVoicePacket(buffer, copied);

        sent_if_necessary = Ama_SendData(buffer, length);

        if(sent_if_necessary)
        {
            SourceDrop(source, lengthSourceThreshold);
        }
        else
        {
            break;
        }

        no_of_transport_pkt++;
    }

    free(buffer);

    DEBUG_LOG_V_VERBOSE("amaAudio_SendOpusVoiceData: %d bytes remaining", SourceSize(source));

    return sent_if_necessary;
}

static va_audio_codec_t amaAudio_ConvertCodecType(ama_codec_t codec_type)
{
    switch(codec_type)
    {
        case ama_codec_sbc:
            return va_audio_codec_sbc;
        case ama_codec_msbc:
            return va_audio_codec_msbc;
        case ama_codec_opus:
            return va_audio_codec_opus;
        default:
            DEBUG_LOG_ERROR("amaAudio_ConvertCodecType: Unknown codec");
            Panic();
            return ama_codec_last;
    }
}

unsigned AmaAudio_HandleVoiceData(Source src)
{
    unsigned timeout_in_ms = 0;

    if(AmaData_IsSendingVoiceData())
    {
        if (amaAudio_SendVoiceData(src) == FALSE)
        {
            /* Making sure we attempt to retransmit even if the source is full */
            timeout_in_ms = 50;
        }
    }
    else
    {
        SourceDrop(src, SourceSize(src));
    }

    return timeout_in_ms;
}

static va_audio_encode_config_t amaAudio_GetEncodeConfiguration(void)
{
    va_audio_encode_config_t config = {0};

    ama_audio_data_t *ama_audio_cfg = AmaData_GetAudioData();
    config.encoder = amaAudio_ConvertCodecType(ama_audio_cfg->codec);

    switch(config.encoder)
    {
        case va_audio_codec_msbc:
            amaAudio_SendVoiceData = amaAudio_SendMsbcVoiceData;
            config.encoder_params.msbc.bitpool_size = ama_audio_cfg->u.msbc_bitpool_size;
            break;

        case va_audio_codec_opus:
            amaAudio_SendVoiceData = amaAudio_SendOpusVoiceData;
            config.encoder_params.opus.frame_size = AmaAudio_GetOpusFrameSize(ama_audio_cfg);
            break;

        default:
            DEBUG_LOG_ERROR("amaAudio_GetEncodeConfiguration: Unsupported codec");
            Panic();
            break;
    }

    return config;
}

static void amaAudio_SetAudioFormat(void)
{
    ama_audio_data_t *ama_audio_cfg = AmaData_GetAudioData();

    switch(ama_audio_cfg->codec)
    {
        case ama_codec_msbc:
            AmaSpeech_SetAudioFormat(ama_audio_format_msbc);
            break;
        case ama_codec_opus:
            AmaAudio_SetAudioFormat(ama_audio_cfg);
            break;
        default:
            DEBUG_LOG_ERROR("amaAudio_SetAudioFormat: Unsupported codec");
            Panic();
            break;
    }
}

static uint32 amaAudio_GetStartCaptureTimestamp(const va_audio_wuw_detection_info_t *wuw_info)
{
    return (wuw_info->start_timestamp - (uint32) PRE_ROLL_US);
}

bool AmaAudio_WakeWordDetected(va_audio_wuw_capture_params_t *capture_params, const va_audio_wuw_detection_info_t *wuw_info)
{
    bool start_capture = FALSE;

    capture_params->start_timestamp = amaAudio_GetStartCaptureTimestamp(wuw_info);

    DEBUG_LOG("amaAudio_WakeUpWordDetected");

    amaAudio_SetAudioFormat();

    if (AmaData_IsReadyToSendStartSpeech() && AmaSpeech_StartWakeWord(PRE_ROLL_US, wuw_info->start_timestamp, wuw_info->end_timestamp))
    {
        start_capture = TRUE;
        capture_params->encode_config = amaAudio_GetEncodeConfiguration();
        AmaData_SetState(ama_state_sending);
    }

    return start_capture;
}

static bool amaAudio_StartVoiceCapture(void)
{
    va_audio_voice_capture_params_t audio_cfg = {0};

    audio_cfg.mic_config.sample_rate = 16000;
    audio_cfg.mic_config.max_number_of_mics = AMA_MAX_NUMBER_OF_MICS;
    audio_cfg.mic_config.min_number_of_mics = AMA_MIN_NUMBER_OF_MICS;
    audio_cfg.encode_config = amaAudio_GetEncodeConfiguration();

    voice_ui_audio_status_t status = VoiceUi_StartAudioCapture(Ama_GetVoiceUiHandle(), &audio_cfg);
    if (voice_ui_audio_failed == status)
    {
        DEBUG_LOG_ERROR("amaAudio_StartVoiceCapture: Failed to start capture");
        Panic();
    }

    return (voice_ui_audio_success == status) ;
}

static void amaAudio_StopVoiceCapture(void)
{
    VoiceUi_StopAudioCapture(Ama_GetVoiceUiHandle());
}

static void amaAudio_StartWuwDetection(void)
{
    va_audio_wuw_detection_params_t detection =
    {
        .max_pre_roll_in_ms = 2000,
        .wuw_config =
        {
            .engine = va_wuw_engine_apva,
            .model = current_locale.file_index
        },
        .mic_config =
        {
            .sample_rate = 16000,
            .max_number_of_mics = AMA_MAX_NUMBER_OF_MICS,
            .min_number_of_mics = AMA_MIN_NUMBER_OF_MICS
        }
    };

    if (detection.wuw_config.model == FILE_NONE)
    {
        DEBUG_LOG_ERROR("amaAudio_StartWuWDetection: Failed to find model");
        Panic();
    }
    voice_ui_audio_status_t status = VoiceUi_StartWakeUpWordDetection(Ama_GetVoiceUiHandle(), &detection);
    if (voice_ui_audio_failed == status)
    {
        DEBUG_LOG_ERROR("amaAudio_StartWuWDetection: Failed to start detection");
        Panic();
    }
}

static void amaAudio_StopWuwDetection(void)
{
    VoiceUi_StopWakeUpWordDetection(Ama_GetVoiceUiHandle());
}

bool AmaAudio_Start(ama_audio_trigger_t type)
{
    bool return_val = FALSE;

    amaAudio_SetAudioFormat();
    
    if (AmaData_IsReadyToSendStartSpeech())
    {
        switch(type)
        {
            case ama_audio_trigger_tap:
                return_val = AmaSpeech_StartTapToTalk();
                break;
            case ama_audio_trigger_press:
                return_val = AmaSpeech_StartPushToTalk();
                break;
            default:
                DEBUG_LOG_ERROR("AmaAudio_Start: Unsupported trigger");
                Panic();
                break;
        }
    }

    if(return_val)
    {
        if(!amaAudio_StartVoiceCapture())
        {
            AmaSpeech_Stop();
            return_val = FALSE;
        }
    }

    return return_val;
}

DataFileID Ama_LoadHotwordModelFile(FILE_INDEX file_index)
{
    PanicFalse(file_index == current_locale.file_index);
    DEBUG_LOG("Ama_LoadHotwordModelFile %d", file_index);
    return OperatorDataLoadEx(file_index, DATAFILE_BIN, STORAGE_INTERNAL, FALSE);
}

void AmaAudio_Stop(void)
{
    DEBUG_LOG("AmaAudio_Stop");
    amaAudio_StopVoiceCapture();
}

bool AmaAudio_Provide(const AMA_SPEECH_PROVIDE_IND_T* ind)
{
    bool return_val = FALSE;
    if (AmaData_IsReadyToSendStartSpeech())
    {
        return_val = amaAudio_StartVoiceCapture();
    }
    AmaProtocol_ProvideSpeechRsp(return_val, ind);
    return return_val;
}

void AmaAudio_End(void)
{
    DEBUG_LOG("AmaAudio_End");
    AmaSpeech_End();
    amaAudio_StopVoiceCapture();
}

void AmaAudio_Suspended(void)
{
    DEBUG_LOG("AmaAudio_Suspended");
    AmaSpeech_Stop();
}

void AmaAudio_StartWakeWordDetection(void)
{
    DEBUG_LOG("AmaAudio_StartWakeWordDetection");
    amaAudio_StartWuwDetection();
}

void AmaAudio_StopWakeWordDetection(void)
{
    DEBUG_LOG("AmaAudio_StopWakeWordDetection");
    amaAudio_StopWuwDetection();
}

void AmaAudio_GetSupportedLocales(ama_supported_locales_t* supported_locales)
{
    memset(supported_locales, 0, sizeof(ama_supported_locales_t));
#ifdef INCLUDE_WUW
    for(uint8 i = 0; i < MAX_AMA_LOCALES; i++)
    {
        FILE_INDEX locale_index = FileFind(FILE_ROOT, locale_ids[i], strlen(locale_ids[i]));
        if(locale_index != FILE_NONE)
        {
            supported_locales->name[supported_locales->num_locales] = locale_ids[i];
            supported_locales->num_locales++;
        }
    }
#endif
}

const char* AmaAudio_GetCurrentLocale(void)
{
#ifdef INCLUDE_WUW
    PanicFalse(current_locale.file_index != FILE_NONE);
    return current_locale.name;
#else
    return "";
#endif
}

void AmaAudio_SetLocale(const char* locale)
{
#ifdef INCLUDE_WUW
    current_locale.name = locale;
    amaAudio_SetCurrentLocaleFileIndex();
    AmaAudio_StartWakeWordDetection();
#else
    UNUSED(locale);
#endif
}

void AmaAudio_Init(void)
{
#ifdef INCLUDE_WUW
    amaAudio_SetCurrentLocaleFileIndex();
    PanicFalse(MAX_AMA_LOCALES == (sizeof(locale_ids)/sizeof(locale_ids[0])));
#endif
}

#endif /* INCLUDE_AMA */
