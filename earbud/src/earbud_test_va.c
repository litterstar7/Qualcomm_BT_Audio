#include <logging.h>
#include <ui.h>
#include <logical_input_switch.h>
#include <voice_ui_va_client_if.h>
#include <voice_audio_manager.h>

#include "earbud_test_va.h"

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

static const va_audio_encode_config_t va_encode_config_table[] =
{
    {.encoder = va_audio_codec_sbc, .encoder_params.sbc =
        {
            .bitpool_size = 24,
            .block_length = 16,
            .number_of_subbands = 8,
            .allocation_method = sbc_encoder_allocation_method_loudness,
        }
    },
    {.encoder = va_audio_codec_msbc, .encoder_params.msbc = {.bitpool_size = 24}},
    {.encoder = va_audio_codec_opus, .encoder_params.opus = {.frame_size = 40}},
};

static bool earbudTest_PopulateVaEncodeConfig(va_audio_codec_t encoder, va_audio_encode_config_t *config)
{
    bool status = FALSE;
    unsigned i;

    for(i = 0; i < ARRAY_DIM(va_encode_config_table); i++)
    {
        if (va_encode_config_table[i].encoder == encoder)
        {
            status = TRUE;
            if (config)
            {
                *config = va_encode_config_table[i];
            }
            break;
        }
    }

    return status;
}

static bool earbudTest_PopulateVaMicConfig(unsigned num_of_mics, va_audio_mic_config_t *config)
{
    config->sample_rate = 16000;
    config->min_number_of_mics = 1;
    /* Use it as max in order to attempt to use this many mics */
    config->max_number_of_mics = num_of_mics;

    return (config->max_number_of_mics >= 1);
}

static bool earbudTest_PopulateVoiceCaptureParams(va_audio_codec_t encoder, unsigned num_of_mics, va_audio_voice_capture_params_t *params)
{
    return earbudTest_PopulateVaMicConfig(num_of_mics, &params->mic_config) && earbudTest_PopulateVaEncodeConfig(encoder, &params->encode_config);
}


void appTestVaTap(void)
{
    DEBUG_LOG_ALWAYS("appTestVaTap");
    /* Simulates a "Button Down -> Button Up -> Single Press Detected" sequence
    for the default configuration of a dedicated VA button */
#ifndef INCLUDE_AMA
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
#endif
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_3);
}

void appTestVaDoubleTap(void)
{
    DEBUG_LOG_ALWAYS("appTestVaDoubleTap");
    /* Simulates a "Button Down -> Button Up -> Button Down -> Button Up -> Double Press Detected"
    sequence for the default configuration of a dedicated VA button */
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_4);
}

void appTestVaPressAndHold(void)
{
    DEBUG_LOG_ALWAYS("appTestVaPressAndHold");
    /* Simulates a "Button Down -> Hold" sequence for the default configuration
    of a dedicated VA button */
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_1);
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_5);
}

void appTestVaRelease(void)
{
    DEBUG_LOG_ALWAYS("appTestVaRelease");
    /* Simulates a "Button Up" event for the default configuration
    of a dedicated VA button */
    LogicalInputSwitch_SendPassthroughLogicalInput(ui_input_va_6);
}

void EarbudTest_SetActiveVa2GAA(void)
{
    VoiceUi_SelectVoiceAssistant(voice_ui_provider_gaa, voice_ui_reboot_allowed);
}

void EarbudTest_SetActiveVa2AMA(void)
{
    VoiceUi_SelectVoiceAssistant(voice_ui_provider_ama, voice_ui_reboot_allowed);
}

static unsigned earbudTest_DropDataInSource(Source source)
{
    SourceDrop(source, SourceSize(source));
    return 0;
}

bool EarbudTest_StartVaCapture(va_audio_codec_t encoder, unsigned num_of_mics)
{
    bool status = FALSE;
    va_audio_voice_capture_params_t params = {0};

    if (earbudTest_PopulateVoiceCaptureParams(encoder, num_of_mics, &params))
    {
        status = VoiceAudioManager_StartCapture(earbudTest_DropDataInSource, &params);
    }

    return status;
}

bool EarbudTest_StopVaCapture(void)
{
    return VoiceAudioManager_StopCapture();
}

#ifdef INCLUDE_WUW
static const struct
{
    va_wuw_engine_t engine;
    unsigned        capture_ts_based_on_wuw_start_ts:1;
    uint16          max_pre_roll_in_ms;
    uint16          pre_roll_on_capture_in_ms;
    const char     *model;
} wuw_detection_start_table[] =
{
    {va_wuw_engine_qva, TRUE, 2000, 500, "tfd_0.bin"},
    {va_wuw_engine_gva, TRUE, 2000, 500, "gaa_model.bin"},
    {va_wuw_engine_apva, FALSE, 2000, 500, "apva_model.bin"}
};


static struct
{
    unsigned         start_capture_on_detection:1;
    va_audio_codec_t encoder_for_capture_on_detection;
    va_wuw_engine_t  wuw_engine;
} va_config = {0};

static bool earbudTest_PopulateWuwDetectionParams(va_wuw_engine_t engine, unsigned num_of_mics, va_audio_wuw_detection_params_t *params)
{
    bool status = FALSE;
    unsigned i;

    if (earbudTest_PopulateVaMicConfig(num_of_mics, &params->mic_config) == FALSE)
    {
        return FALSE;
    }

    for(i = 0; i < ARRAY_DIM(wuw_detection_start_table); i++)
    {
        if (wuw_detection_start_table[i].engine == engine)
        {
            FILE_INDEX model = FileFind(FILE_ROOT, wuw_detection_start_table[i].model, strlen(wuw_detection_start_table[i].model));
            if (model != FILE_NONE)
            {
                status = TRUE;
                if (params)
                {
                    params->wuw_config.engine = wuw_detection_start_table[i].engine;
                    params->wuw_config.model = model;
                    params->max_pre_roll_in_ms = wuw_detection_start_table[i].max_pre_roll_in_ms;
                }
                break;
            }
        }
    }

    return status;
}

static bool earbudTest_PopulateStartCaptureTimeStamp(const va_audio_wuw_detection_info_t *wuw_info, uint32 *timestamp)
{
    bool status = FALSE;
    unsigned i;

    for(i = 0; i < ARRAY_DIM(wuw_detection_start_table); i++)
    {
        if (wuw_detection_start_table[i].engine == va_config.wuw_engine)
        {
            uint32 pre_roll = wuw_detection_start_table[i].pre_roll_on_capture_in_ms * 1000;
            status = TRUE;
            if (timestamp)
            {
                if (wuw_detection_start_table[i].capture_ts_based_on_wuw_start_ts)
                {
                    *timestamp = wuw_info->start_timestamp - pre_roll;
                }
                else
                {
                    *timestamp = wuw_info->end_timestamp - pre_roll;
                }
            }
            break;
        }
    }

    return status;
}

static va_audio_wuw_detected_response_t earbudTest_WuwDetected(const va_audio_wuw_detection_info_t *wuw_info)
{
    va_audio_wuw_detected_response_t response = {0};

    if (va_config.start_capture_on_detection &&
        earbudTest_PopulateVaEncodeConfig(va_config.encoder_for_capture_on_detection, &response.capture_params.encode_config) &&
        earbudTest_PopulateStartCaptureTimeStamp(wuw_info, &response.capture_params.start_timestamp))
    {
        response.start_capture = TRUE;
        response.capture_callback = earbudTest_DropDataInSource;
    }

    return response;
}

bool EarbudTest_StartVaWakeUpWordDetection(va_wuw_engine_t wuw_engine, unsigned num_of_mics, bool start_capture_on_detection, va_audio_codec_t encoder)
{
    va_audio_wuw_detection_params_t params = {0};

    if (earbudTest_PopulateWuwDetectionParams(wuw_engine, num_of_mics, &params) == FALSE)
    {
        return FALSE;
    }

    if (start_capture_on_detection && (earbudTest_PopulateVaEncodeConfig(encoder, NULL) == FALSE))
    {
        return FALSE;
    }

    va_config.start_capture_on_detection = start_capture_on_detection;
    va_config.encoder_for_capture_on_detection = encoder;
    va_config.wuw_engine = wuw_engine;

    return VoiceAudioManager_StartDetection(earbudTest_WuwDetected, &params);
}

bool EarbudTest_StopVaWakeUpWordDetection(void)
{
    return VoiceAudioManager_StopDetection();
}
#endif /* INCLUDE_WUW */
