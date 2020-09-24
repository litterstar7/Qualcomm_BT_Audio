/*******************************************************************************
Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_configure_coefficients.c

DESCRIPTION
    Functions required to configure the IIR/LP/DC filters and gains.
*/

#include "anc_configure_coefficients.h"
#include "anc_config_data.h"
#include "anc_data.h"
#include "anc_debug.h"
#include <audio_anc.h>
#include <source.h>

#define DAWTH 32
/* Convert x into 1.(DAWTH - 1) format */
#define FRACTIONAL(x) ( (int)( (x) * ((1l<<(DAWTH-1)) - 1) ))

/*
 * \brief Inline assembly helper function for performing a fractional multiplication
 *
 * \param a The first value for the multiplication in fractional encoding
 * \param b The value to multiply a by in fractional encoding
 * \return Multiplication result in fractional encoding.
 */
#if defined(__KALIMBA__) && !defined(__GNUC__)
asm int frac_mult(int a, int b)
{
    @{} = @{a} * @{b} (frac);
}
#else
#define frac_mult(a, b) ((int)((((long long)a)*(b)) >> (DAWTH-1))) 
#endif

static uint16 convertCoefficientFromStoredFormat(uint32 coefficient)
{
    return frac_mult(coefficient,FRACTIONAL(1.0/16.0));
}

static void readCoefficients(uint16 * req_coefficient, iir_config_t * iir_config)
{
    unsigned index;
    for(index = 0; index < NUMBER_OF_IIR_COEFFICIENTS; index++)
    {
        req_coefficient[index] = convertCoefficientFromStoredFormat(iir_config->coefficients[index]);
    }
}


static void setIirCoefficients(audio_anc_instance instance, anc_instance_config_t * config)
{
    uint16 coefficients[NUMBER_OF_IIR_COEFFICIENTS];

    readCoefficients(coefficients, &config->feed_forward_a.iir_config);
    ANC_ASSERT(AudioAncFilterIirSet(instance, AUDIO_ANC_PATH_ID_FFA, NUMBER_OF_IIR_COEFFICIENTS, coefficients));

    memset(coefficients, 0, sizeof(uint16)*NUMBER_OF_IIR_COEFFICIENTS);
    readCoefficients(coefficients, &config->feed_forward_b.iir_config);
    ANC_ASSERT(AudioAncFilterIirSet(instance, AUDIO_ANC_PATH_ID_FFB, NUMBER_OF_IIR_COEFFICIENTS, coefficients));

    memset(coefficients, 0, sizeof(uint16)*NUMBER_OF_IIR_COEFFICIENTS);
    readCoefficients(coefficients, &config->feed_back.iir_config);
    ANC_ASSERT(AudioAncFilterIirSet(instance, AUDIO_ANC_PATH_ID_FB, NUMBER_OF_IIR_COEFFICIENTS,coefficients));
}

static void setLpfCoefficients(audio_anc_instance instance, anc_instance_config_t * config)
{
    ANC_ASSERT(AudioAncFilterLpfSet(instance, AUDIO_ANC_PATH_ID_FFA,
                    config->feed_forward_a.lpf_config.lpf_shift1,
                    config->feed_forward_a.lpf_config.lpf_shift2));

    ANC_ASSERT(AudioAncFilterLpfSet(instance, AUDIO_ANC_PATH_ID_FFB,
                        config->feed_forward_b.lpf_config.lpf_shift1,
                        config->feed_forward_b.lpf_config.lpf_shift2));

    ANC_ASSERT(AudioAncFilterLpfSet(instance, AUDIO_ANC_PATH_ID_FB,
                        config->feed_back.lpf_config.lpf_shift1,
                        config->feed_back.lpf_config.lpf_shift2));
}

static Source getAnyAncMicSourceForInstance(audio_anc_instance instance)
{
    anc_path_enable mic_path = feed_forward_left;
    audio_mic_params * mic_params = NULL;

    if(instance == AUDIO_ANC_INSTANCE_0)
    {
        switch(ancDataGetMicParams()->enabled_mics)
        {
            case feed_forward_mode:
            case feed_forward_mode_left_only:
            case hybrid_mode:
            case hybrid_mode_left_only:
                mic_path = feed_forward_left;
                break;
            case feed_back_mode:
            case feed_back_mode_left_only:
                mic_path = feed_back_left;
                break;
            default:
                Panic();
                break;
        }
    }
    else
    {
        switch(ancDataGetMicParams()->enabled_mics)
        {
            case feed_forward_mode:
            case feed_forward_mode_right_only:
            case hybrid_mode:
            case hybrid_mode_right_only:
                mic_path = feed_forward_right;
                break;
            case feed_back_mode:
            case feed_back_mode_right_only:
                mic_path = feed_back_right;
                break;
            default:
                Panic();
                break;
        }
    }
    mic_params = ancConfigDataGetMicForMicPath(mic_path);

    return AudioPluginGetMicSource(*mic_params, mic_params->channel);
}

static void setDcFilters(Source mic_source, anc_instance_config_t * config)
{
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFA_DC_FILTER_SHIFT, config->feed_forward_a.dc_filter_config.filter_shift));
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFA_DC_FILTER_ENABLE, config->feed_forward_a.dc_filter_config.filter_enable));

    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFB_DC_FILTER_SHIFT, config->feed_forward_b.dc_filter_config.filter_shift));
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFB_DC_FILTER_ENABLE, config->feed_forward_b.dc_filter_config.filter_enable));
}

static void setSmallLpf(Source mic_source, anc_instance_config_t * config)
{
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_SM_LPF_FILTER_SHIFT, config->small_lpf.small_lpf_config.filter_shift));
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_SM_LPF_FILTER_ENABLE, config->small_lpf.small_lpf_config.filter_enable));
}

static void setPathGains(Source mic_source, anc_instance_config_t * config)
{
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFA_GAIN, config->feed_forward_a.gain_config.gain));
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFA_GAIN_SHIFT, config->feed_forward_a.gain_config.gain_shift));

    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFB_GAIN, config->feed_forward_b.gain_config.gain));
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFB_GAIN_SHIFT, config->feed_forward_b.gain_config.gain_shift));

    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FB_GAIN, config->feed_back.gain_config.gain));
    ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FB_GAIN_SHIFT, config->feed_back.gain_config.gain_shift));
}

anc_instance_config_t * getInstanceConfig(audio_anc_instance instance)
{
    return (instance == AUDIO_ANC_INSTANCE_1 ? &ancDataGetCurrentModeConfig()->instance[ANC_INSTANCE_1_INDEX]
                                                : &ancDataGetCurrentModeConfig()->instance[ANC_INSTANCE_0_INDEX]);
}

static void configureCoefficientForInstance(audio_anc_instance instance)
{
    anc_instance_config_t * config = getInstanceConfig(instance);

    Source mic_source = getAnyAncMicSourceForInstance(instance);

    setIirCoefficients(instance, config);
    setLpfCoefficients(instance, config);

    setDcFilters(mic_source, config);
    setSmallLpf(mic_source, config);
}

static void configureGainsForInstance(audio_anc_instance instance)
{
    anc_instance_config_t * config = getInstanceConfig(instance);

    Source mic_source = getAnyAncMicSourceForInstance(instance);

    setPathGains(mic_source, config);
}

/******************************************************************************/
void ancConfigureMutePathGains(void)
{
    if(ancDataIsLeftChannelConfigurable())
    {
       Source mic_source_left = getAnyAncMicSourceForInstance(AUDIO_ANC_INSTANCE_0);
       
       if(mic_source_left)
       {
          ANC_ASSERT(SourceConfigure(mic_source_left, STREAM_ANC_FFA_GAIN, 0));
          ANC_ASSERT(SourceConfigure(mic_source_left, STREAM_ANC_FFB_GAIN, 0));
          ANC_ASSERT(SourceConfigure(mic_source_left, STREAM_ANC_FB_GAIN, 0));
       }
    }

    if(ancDataIsRightChannelConfigurable())
    {
       Source mic_source_right = getAnyAncMicSourceForInstance(AUDIO_ANC_INSTANCE_1);

       if(mic_source_right)
       {
          ANC_ASSERT(SourceConfigure(mic_source_right, STREAM_ANC_FFA_GAIN, 0));
          ANC_ASSERT(SourceConfigure(mic_source_right, STREAM_ANC_FFB_GAIN, 0));
          ANC_ASSERT(SourceConfigure(mic_source_right, STREAM_ANC_FB_GAIN, 0));
       }
    }
}

/******************************************************************************/
void ancConfigureFilterCoefficients(void)
{
    if(ancDataIsLeftChannelConfigurable())
    {
        configureCoefficientForInstance(AUDIO_ANC_INSTANCE_0);
    }

    if(ancDataIsRightChannelConfigurable())
    {
        configureCoefficientForInstance(AUDIO_ANC_INSTANCE_1);
    }
}

/******************************************************************************/
void ancConfigureFilterPathGains(void)
{
    if(ancDataIsLeftChannelConfigurable())
    {
        configureGainsForInstance(AUDIO_ANC_INSTANCE_0);
    }

    if(ancDataIsRightChannelConfigurable())
    {
        configureGainsForInstance(AUDIO_ANC_INSTANCE_1);
    }
}

/******************************************************************************/
bool ancConfigureGainForFFApath(audio_anc_instance instance, uint8 gain)
{
    Source mic_source = getAnyAncMicSourceForInstance(instance);

    if(mic_source)
    {
        return(SourceConfigure(mic_source, STREAM_ANC_FFA_GAIN, gain));
    }
    return FALSE;
}

/******************************************************************************/
bool ancConfigureGainForFFBpath(audio_anc_instance instance, uint8 gain)
{
    Source mic_source = getAnyAncMicSourceForInstance(instance);

    if(mic_source)
    {
        return(SourceConfigure(mic_source, STREAM_ANC_FFB_GAIN,gain));
    }
    return FALSE;
}

/******************************************************************************/
bool ancConfigureGainForFBpath(audio_anc_instance instance, uint8 gain)
{
    Source mic_source = getAnyAncMicSourceForInstance(instance);
    if(mic_source)
    {
        return(SourceConfigure(mic_source, STREAM_ANC_FB_GAIN,gain));
    }
    return FALSE;
}

/******************************************************************************/
static void setUserPathGains(Source mic_source, anc_user_gain_config_t * config)
{
    if(config)
    {
        if(config->enable_ffa_gain_update)
           ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFA_GAIN, (unsigned)(config->ffa_gain & 0xFF)));

        if(config->enable_ffb_gain_update)
           ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFB_GAIN, (unsigned)(config->ffb_gain & 0xFF)));

        if(config->enable_fb_gain_update)
           ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FB_GAIN, (unsigned)(config->fb_gain & 0xFF)));

        if(config->enable_ffa_gain_shift_update)
           ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFA_GAIN_SHIFT, (unsigned)(config->ffa_gain_shift)));

        if(config->enable_ffb_gain_shift_update)
           ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FFB_GAIN_SHIFT, (unsigned)(config->ffb_gain_shift)));

        if(config->enable_fb_gain_shift_update)
           ANC_ASSERT(SourceConfigure(mic_source, STREAM_ANC_FB_GAIN_SHIFT, (unsigned)(config->fb_gain_shift)));
    }
}

/******************************************************************************/
void ancOverWriteWithUserPathGains(void)
{
    anc_user_config_t* config = ancDataGetUserGainConfig();

    if(ancDataIsLeftChannelConfigurable())
    {
       Source mic_source_left = getAnyAncMicSourceForInstance(AUDIO_ANC_INSTANCE_0);

       if(mic_source_left)
       {
           setUserPathGains(mic_source_left, config->left);
       }
    }

    if(ancDataIsRightChannelConfigurable())
    {
       Source mic_source_right = getAnyAncMicSourceForInstance(AUDIO_ANC_INSTANCE_1);

       if(mic_source_right)
       {
           setUserPathGains(mic_source_right, config->right);
       }
    }
}
