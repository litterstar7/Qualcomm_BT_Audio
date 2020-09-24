/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Audio class 1.0 descriptors
*/

#include "usb_audio_class_10_descriptors.h"
#include "usb_audio_class_10.h"

/* Unit/Terminal IDs */
#define SPKR_VOICE_IT  0x01
#define SPKR_VOICE_FU  0x02
#define SPKR_VOICE_OT  0x03
#define MIC_VOICE_IT   0x04
#define MIC_VOICE_FU   0x05
#define MIC_VOICE_OT   0x06
#define SPKR_AUDIO_IT  0x07
#define SPKR_AUDIO_FU  0x08
#define SPKR_AUDIO_OT  0x09


#define VOICE_MIC_CHANNELS        USB_AUDIO_CHANNELS_MONO
#define VOICE_SPKR_CHANNELS       USB_AUDIO_CHANNELS_MONO
#define AUDIO_SPKR_CHANNELS       USB_AUDIO_CHANNELS_STEREO

#if VOICE_SPKR_CHANNELS == USB_AUDIO_CHANNELS_STEREO
#define VOICE_SPKR_CHANNEL_CONFIG  0x03
#elif VOICE_SPKR_CHANNELS == USB_AUDIO_CHANNELS_MONO
#define VOICE_SPKR_CHANNEL_CONFIG  0x01
#else
#error NOT_SUPPORTED
#endif

#if VOICE_MIC_CHANNELS == USB_AUDIO_CHANNELS_STEREO
#define VOICE_MIC_CHANNEL_CONFIG   0x03
#elif VOICE_MIC_CHANNELS == USB_AUDIO_CHANNELS_MONO
#define VOICE_MIC_CHANNEL_CONFIG   0x01
#else
#error NOT_SUPPORTED
#endif

#if AUDIO_SPKR_CHANNELS == USB_AUDIO_CHANNELS_STEREO
#define AUDIO_SPKR_CHANNEL_CONFIG  0x03
#elif AUDIO_SPKR_CHANNELS == USB_AUDIO_CHANNELS_MONO
#define AUDIO_SPKR_CHANNEL_CONFIG  0x01
#else
#error NOT_SUPPORTED
#endif

/* bmaControls should be changed with FU_DESC_CONTROL_SIZE */
#define FU_DESC_CONTROL_SIZE   0x01

#define VOICE_SPKR_SUPPORTED_FREQUENCIES   2
#define VOICE_MIC_SUPPORTED_FREQUENCIES    2
#define AUDIO_SPKR_SUPPORTED_FREQUENCIES   6


#define USB_AUDIO_SAMPLE_SIZE                       USB_SAMPLE_SIZE_16_BIT

static const uint8 control_intf_desc_voice_mic[] =
{
    /* Microphone IT */
    UAC_IT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_INPUT_TERMINAL,         /* bDescriptorSubType = INPUT_TERMINAL */
    MIC_VOICE_IT,                             /* bTerminalID */
    UAC_TRM_BIDI_HEADSET & 0xFF,        /* wTerminalType = Microphone*/
    UAC_TRM_BIDI_HEADSET >> 8,
    0x00,                               /* bAssocTerminal = none */
    VOICE_MIC_CHANNELS,                 /* bNrChannels = 1 */
    VOICE_MIC_CHANNEL_CONFIG,           /* wChannelConfig  */
    VOICE_MIC_CHANNEL_CONFIG >> 8,      /* wChannelConfig	*/
    0x00,                               /* iChannelName = no string */
    0x00,                               /* iTerminal = same as USB product string */

    /* Microphone Features */
    UAC_FU_DESC_SIZE(VOICE_MIC_CHANNELS, FU_DESC_CONTROL_SIZE),   /*bLength*/
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_FEATURE_UNIT,           /* bDescriptorSubType = FEATURE_UNIT*/
    MIC_VOICE_FU,                             /* bUnitId*/
    MIC_VOICE_IT,                             /* bSourceId - Microphone IT*/
    FU_DESC_CONTROL_SIZE,               /* bControlSize = 1 bytes per control*/
    UAC_FU_CONTROL_MUTE,                /* bmaControls[0] (Mute on Master Channel)*/
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[1] (Vol on Left Front)*/
#if VOICE_MIC_CHANNELS == 2
    UAC_FU_CONTROL_UNDEFINED,           /* bmaControls[2] (Vol on Right Front)*/
#endif
    0x00,                               /*iFeature = same as USB product string*/

    /* Microphone OT */
    UAC_OT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_OUTPUT_TERNINAL,        /* bDescriptorSubType = OUTPUT_TERMINAL */
    MIC_VOICE_OT,                             /* bTerminalID */
    UAC_TRM_USB_STREAMING & 0xFF,       /* wTerminalType = USB streaming */
    UAC_TRM_USB_STREAMING >> 8,
    0x00,                               /* bAssocTerminal = none */
    MIC_VOICE_FU,                             /* bSourceID - Microphone Features */
    0x00,                               /* iTerminal = same as USB product string */
};

static const uint8 control_intf_desc_voice_spkr[] =
{
    /* Speaker IT */
    UAC_IT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_INPUT_TERMINAL,         /* bDescriptorSubType = INPUT_TERMINAL */
    SPKR_VOICE_IT,                            /* bTerminalID */
    UAC_TRM_USB_STREAMING & 0xFF,       /* wTerminalType = USB streaming */
    UAC_TRM_USB_STREAMING >> 8,
    0x00,                               /* bAssocTerminal = none */
    VOICE_SPKR_CHANNELS,                /* bNrChannels = 1 */
    VOICE_SPKR_CHANNEL_CONFIG,          /* wChannelConfig	*/
    VOICE_SPKR_CHANNEL_CONFIG >> 8,     /* wChannelConfig  */
    0x00,                               /* iChannelName = no string */
    0x00,                               /* iTerminal = same as USB product string */

    /* Speaker Features */
    UAC_FU_DESC_SIZE(VOICE_SPKR_CHANNELS, FU_DESC_CONTROL_SIZE),   /*bLength*/
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_FEATURE_UNIT,           /* bDescriptorSubType = FEATURE_UNIT*/
    SPKR_VOICE_FU,                            /* bUnitId*/
    SPKR_VOICE_IT,                            /* bSourceId - Speaker IT*/
    FU_DESC_CONTROL_SIZE,               /* bControlSize = 1 bytes per control*/
    UAC_FU_CONTROL_MUTE,                /* bmaControls[0] (Mute on Master Channel)*/
    UAC_FU_CONTROL_VOLUME,              /* bmaControls[1] (Vol on Left Front)*/
#if VOICE_SPKR_CHANNELS == 2
    UAC_FU_CONTROL_VOLUME,              /* bmaControls[2] (Vol on Right Front)*/
#endif
    0x00,                               /*iFeature = same as USB product string*/

    /* Speaker OT */
    UAC_OT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_OUTPUT_TERNINAL,        /* bDescriptorSubType = OUTPUT_TERMINAL */
    SPKR_VOICE_OT,                            /* bTerminalID */
    UAC_TRM_BIDI_HEADSET & 0xFF,        /* wTerminalType = Speaker*/
    UAC_TRM_BIDI_HEADSET >> 8,
    0x00,                               /* bAssocTerminal = none */
    SPKR_VOICE_FU,                            /* bSourceID - Speaker Features*/
    0x00                                /* iTerminal = same as USB product string */
};

static const uint8 control_intf_desc_audio_spkr[] =
{
    /* ALT_Speaker IT */
    UAC_IT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_INPUT_TERMINAL,         /* bDescriptorSubType = INPUT_TERMINAL */
    SPKR_AUDIO_IT,                           /* bTerminalID */
    UAC_TRM_USB_STREAMING & 0xFF,       /* wTerminalType = USB streaming */
    UAC_TRM_USB_STREAMING >> 8,
    0x00,                               /* bAssocTerminal = none */
    AUDIO_SPKR_CHANNELS,                /* bNrChannels = 2 */
    AUDIO_SPKR_CHANNEL_CONFIG,          /* wChannelConfig	*/
    AUDIO_SPKR_CHANNEL_CONFIG >> 8,
    0x00,                               /* iChannelName = no string */
    0x00,                               /* iTerminal = same as USB product string */

    /* ALT_Speaker Features */
    UAC_FU_DESC_SIZE(AUDIO_SPKR_CHANNELS, FU_DESC_CONTROL_SIZE),   /*bLength*/
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_FEATURE_UNIT,           /* bDescriptorSubType = FEATURE_UNIT*/
    SPKR_AUDIO_FU,                           /* bUnitId*/
    SPKR_AUDIO_IT,                           /* bSourceId - Speaker IT*/
    FU_DESC_CONTROL_SIZE,               /* bControlSize = 1 bytes per control*/
    UAC_FU_CONTROL_MUTE,                /* bmaControls[0] (Mute on Master Channel)*/
    UAC_FU_CONTROL_VOLUME,              /* bmaControls[1] (Vol on Left Front)*/
    #if AUDIO_SPKR_CHANNELS == 2
    UAC_FU_CONTROL_VOLUME,              /* bmaControls[2] (Vol on Right Front)*/
    #endif
    0x00,                               /* iFeature = same as USB product string*/

    /* ALT_Speaker OT */
    UAC_OT_TERM_DESC_SIZE,              /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AC_DESC_OUTPUT_TERNINAL,        /* bDescriptorSubType = OUTPUT_TERMINAL */
    SPKR_AUDIO_OT,                           /* bTerminalID */
    UAC_TRM_OUTPUT_HEADPHONES & 0xFF,   /* wTerminalType = Speaker*/
    UAC_TRM_OUTPUT_HEADPHONES >> 8,
    0x00,                               /* bAssocTerminal = none */
    SPKR_AUDIO_FU,                           /* bSourceID - Speaker Features*/
    0x00                                /* iTerminal = same as USB product string */
};

/** Default USB streaming interface descriptors for alt speaker */
static const uint8 streaming_intf_desc_audio_spkr[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    SPKR_AUDIO_IT,                           /* bTerminalLink = Speaker IT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(AUDIO_SPKR_SUPPORTED_FREQUENCIES),  /* bLength 8+((number of sampling frequencies)*3) */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    AUDIO_SPKR_CHANNELS,                /* bNumberOfChannels */
    USB_AUDIO_SAMPLE_SIZE,              /* bSubframeSize = 2 bytes */
    USB_AUDIO_SAMPLE_SIZE * 8,          /* bBitsResolution */
    AUDIO_SPKR_SUPPORTED_FREQUENCIES,   /* bSampleFreqType = 6 discrete sampling frequencies */
    SAMPLE_RATE_48K & 0xFF,             /* tSampleFreq = 48000*/
    (SAMPLE_RATE_48K >> 8) & 0xFF,
    (SAMPLE_RATE_48K >> 16) & 0xFF,
    SAMPLE_RATE_44K1 & 0xFF,            /* tSampleFreq = 44100*/
    (SAMPLE_RATE_44K1 >> 8) & 0xFF,
    (SAMPLE_RATE_44K1 >> 16) & 0xFF,
    SAMPLE_RATE_32K & 0xFF,             /* tSampleFreq = 32000 */
    (SAMPLE_RATE_32K >> 8 ) & 0xFF,
    (SAMPLE_RATE_32K >> 16) & 0xFF,
    SAMPLE_RATE_22K05 & 0xFF,           /* tSampleFreq = 22050 */
    (SAMPLE_RATE_22K05 >> 8 ) & 0xFF,
    (SAMPLE_RATE_22K05 >> 16) & 0xFF,
    SAMPLE_RATE_16K & 0xFF,             /* tSampleFreq = 16000 */
    (SAMPLE_RATE_16K >> 8 ) & 0xFF,
    (SAMPLE_RATE_16K >> 16) & 0xFF,
    SAMPLE_RATE_8K & 0xFF,              /* tSampleFreq = 8000 */
    (SAMPLE_RATE_8K >> 8) & 0xFF,
    (SAMPLE_RATE_8K >> 16) & 0xFF,


    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_SAMPLING_FREQ,       /* bmAttributes = SamplingFrequency control */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

/** Default USB streaming interface descriptors for speaker */
static const uint8 streaming_intf_desc_voice_spkr[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    SPKR_VOICE_IT,                            /* bTerminalLink = Speaker IT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(VOICE_SPKR_SUPPORTED_FREQUENCIES),  /* bLength 8+((number of sampling frequencies)*3) */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    VOICE_SPKR_CHANNELS,                /* bNumberOfChannels */
    USB_AUDIO_SAMPLE_SIZE,              /* bSubframeSize = 2 bytes */
    USB_AUDIO_SAMPLE_SIZE * 8,          /* bBitsResolution */
    VOICE_SPKR_SUPPORTED_FREQUENCIES,   /* bSampleFreqType = 2 discrete sampling frequencies */
    SAMPLE_RATE_16K & 0xFF,             /* tSampleFreq = 16000 */
    (SAMPLE_RATE_16K >> 8 ) & 0xFF,
    (SAMPLE_RATE_16K >> 16) & 0xFF,
    SAMPLE_RATE_8K & 0xFF,              /* tSampleFreq = 8000 */
    (SAMPLE_RATE_8K >> 8) & 0xFF,
    (SAMPLE_RATE_8K >> 16) & 0xFF,


    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_SAMPLING_FREQ,       /* bmAttributes = SamplingFrequency control */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

/** Default USB streaming interface descriptors for mic */
static const uint8 streaming_intf_desc_voice_mic[] =
{
    /* Class Specific AS interface descriptor */
    UAC_AS_IF_DESC_SIZE,                /* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_GENERAL,                /* bDescriptorSubType = AS_GENERAL */
    MIC_VOICE_OT,                             /* bTerminalLink = Microphone OT */
    0x00,                               /* bDelay */
    UAC_DATA_FORMAT_TYPE_I_PCM & 0xFF,  /* wFormatTag = PCM */
    UAC_DATA_FORMAT_TYPE_I_PCM >> 8,

    /* Type 1 format type descriptor */
    UAC_FORMAT_DESC_SIZE(VOICE_MIC_SUPPORTED_FREQUENCIES),/* bLength */
    UAC_CS_DESC_INTERFACE,              /* bDescriptorType = CS_INTERFACE */
    UAC_AS_DESC_FORMAT_TYPE,            /* bDescriptorSubType = FORMAT_TYPE */
    UAC_AS_DESC_FORMAT_TYPE_I,          /* bFormatType = FORMAT_TYPE_I */
    VOICE_MIC_CHANNELS,                 /* bNumberOfChannels */
    USB_AUDIO_SAMPLE_SIZE,              /* bSubframeSize = 2 bytes */
    USB_AUDIO_SAMPLE_SIZE * 8,          /* bBitsResolution */
    VOICE_MIC_SUPPORTED_FREQUENCIES,    /* bSampleFreqType = 2 discrete sampling freq */
    SAMPLE_RATE_16K & 0xFF,             /* tSampleFreq = 16000 */
    (SAMPLE_RATE_16K >> 8 ) & 0xFF,
    (SAMPLE_RATE_16K >> 16) & 0xFF,
    SAMPLE_RATE_8K & 0xff,              /* tSampleFreq = 8000 */
    (SAMPLE_RATE_8K >> 8) & 0xFF,
    (SAMPLE_RATE_8K >> 16) & 0xFF,

    /* Class specific AS isochronous audio data endpoint descriptor */
    UAC_AS_DATA_EP_DESC_SIZE,           /* bLength */
    UAC_CS_DESC_ENDPOINT,               /* bDescriptorType = CS_ENDPOINT */
    UAC_AS_EP_DESC_GENERAL,             /* bDescriptorSubType = AS_GENERAL */
    UAC_EP_CONTROL_SAMPLING_FREQ,       /* bmAttributes = SamplingFrequency contro */
    0x02,                               /* bLockDelayUnits = Decoded PCM samples */
    0x00, 0x00                          /* wLockDelay */
};

/* Voice Mic interface descriptors */
const uac_control_config_t uac1_voice_control_mic_desc = {
    control_intf_desc_voice_mic,
    sizeof(control_intf_desc_voice_mic)
};

const uac_streaming_config_t uac1_voice_streaming_mic_desc = {
    streaming_intf_desc_voice_mic,
    sizeof(streaming_intf_desc_voice_mic)
};

const uac_endpoint_config_t uac1_voice_mic_endpoint = {
    .is_to_host = 1,
    .wMaxPacketSize = 0,
    .bInterval = 1
};

/* Voice Speaker interface descriptors */
const uac_control_config_t uac1_voice_control_spkr_desc = {
    control_intf_desc_voice_spkr,
    sizeof(control_intf_desc_voice_spkr)
};

const uac_streaming_config_t uac1_voice_streaming_spkr_desc = {
    streaming_intf_desc_voice_spkr,
    sizeof(streaming_intf_desc_voice_spkr)
};

const uac_endpoint_config_t uac1_voice_spkr_endpoint = {
    .is_to_host = 0,
    .wMaxPacketSize = 0,
    .bInterval = 1
};

static const usb_audio_interface_config_t uac1_voice_interface_list[] =
{
    {
        .type = USB_AUDIO_DEVICE_TYPE_VOICE_MIC,
        .control_desc =   &uac1_voice_control_mic_desc,
        .streaming_desc = &uac1_voice_streaming_mic_desc,
        .endpoint =       &uac1_voice_mic_endpoint
    },
    {
        .type = USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER,
        .control_desc =   &uac1_voice_control_spkr_desc,
        .streaming_desc = &uac1_voice_streaming_spkr_desc,
        .endpoint =       &uac1_voice_spkr_endpoint
    }
};

const usb_audio_interface_config_list_t uac1_voice_interfaces =
{
    .intf = uac1_voice_interface_list,
    .num_interfaces = ARRAY_DIM(uac1_voice_interface_list)
};


/* Audio Speaker interface descriptors */
const uac_control_config_t uac1_music_control_spkr_desc = {
    control_intf_desc_audio_spkr,
    sizeof(control_intf_desc_audio_spkr)
};

const uac_streaming_config_t uac1_music_streaming_spkr_desc = {
    streaming_intf_desc_audio_spkr,
    sizeof(streaming_intf_desc_audio_spkr)
};

const uac_endpoint_config_t uac1_music_spkr_endpoint = {
    .is_to_host = 0,
    .wMaxPacketSize = 0,
    .bInterval = 1
};

static const usb_audio_interface_config_t uac1_music_intf_list[] =
{
    {
        .type = USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER,
        .control_desc =   &uac1_music_control_spkr_desc,
        .streaming_desc = &uac1_music_streaming_spkr_desc,
        .endpoint =       &uac1_music_spkr_endpoint
    }
};

const usb_audio_interface_config_list_t uac1_music_interfaces =
{
    .intf = uac1_music_intf_list,
    .num_interfaces = ARRAY_DIM(uac1_music_intf_list)
};
