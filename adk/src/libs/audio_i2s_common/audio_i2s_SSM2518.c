/****************************************************************************
Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_i2s_SSM2518.c
DESCRIPTION
    plugin implentation which has chip specific i2c commands to configure the 
    i2s audio output device for use 
NOTES
*/

#include "audio_i2s_SSM2518.h"

#include <audio.h>
#include <gain_utils.h>
#include <stdlib.h>
#include <panic.h>
#include <file.h>
#include <print.h>
#include <stream.h> 
#include <kalimba.h>
#include <kalimba_standard_messages.h>
#include <message.h>
#include <transform.h>
#include <string.h>
#include <i2c.h>
#include <pio_common.h>
#include <stdio.h>

#include "audio_i2s_common.h"


/*============================================================================*
 *  Private Data Types
 *============================================================================*/

/* Amp power state */
typedef enum
{
    standby = 0, /* Can be used during external amplifier in low power down mode */
    active,      /* Used when external amplifier is in running/active state */
    power_off    /* Used to denote external amplifier is powered off */
} amp_status_t;

/* Collection of data representing current I2S hardware/software state */
typedef struct
{
    amp_status_t amp_status;
} state_t;


/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Amplifier state */
static state_t state = { .amp_status = power_off };

/* Default I2S configuration data for this chip */
const i2s_config_t device_i2s_config =
{
    .master_mode = TRUE,
    .data_format = 0,  /* left justified */
    .bit_delay = 1,    /* I2S delay 1 bit */
    .bits_per_sample = 16,
    .bclk_scaling = 64,
    .mclk_scaling = 256,
    .enable_pio = PIN_INVALID
};


/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

static void enableAmpPio(bool enable);
static void setAmpState(amp_status_t state);


/*============================================================================*
 * Public Function Implementations
 *============================================================================*/

/****************************************************************************
DESCRIPTION:

    Powers on and configures the I2S device.

PARAMETERS:
    
    uint32 sample_rate - sample rate of data coming from dsp

RETURNS:
    Whether the device was successfully initialised.
*/    
bool AudioI2SDeviceInitialise(uint32 sample_rate)
{
    setAmpState(active);

    uint8 i2c_data[2];
    /*uint16 ack;*/
   
    /* set to normal operation with external MCLK, configure for NO_BLCK, MCLK is 64 * sample rate */
    i2c_data[0] = RESET_POWER_CONTROL;
    i2c_data[1] = (S_RST|SPWDN);
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* operate at 256 * fs to allow operation at 8KHz */
    i2c_data[0] = RESET_POWER_CONTROL;
    /* determine sample rate */
    if(sample_rate <= 16000)
        i2c_data[1] = 0x00;
    else if(sample_rate <= 32000)
        i2c_data[1] = 0x02;
    else if(sample_rate <= 48000)
        i2c_data[1] = 0x04;
    else
        i2c_data[1] = 0x08;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* set to automatic sample rate setting */
    i2c_data[0] = EDGE_CLOCK_CONTROL;
    i2c_data[1] = ASR;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* set sample rate and to use i2s with default settings */
    i2c_data[0] = SERIAL_INTERFACE_SAMPLE_RATE_CONTROL;
    /* determine sample rate */
    if(sample_rate <= 12000)
        i2c_data[1] = 0x00;
    else if(sample_rate <= 24000)
        i2c_data[1] = 0x01;
    else if(sample_rate <= 48000)
        i2c_data[1] = 0x02;
    else
        i2c_data[1] = 0x03;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));
    
    /* set to ext b_clk, rising edge, msb first */
    i2c_data[0] = SERIAL_INTERFACE_CONTROL;
    i2c_data[1] = 0x00;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* set the master mute control to on */
    i2c_data[0] = VOLUME_MUTE_CONTROL;
    i2c_data[1] = M_UNMUTE;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    return TRUE;
}

/****************************************************************************
DESCRIPTION:

    Shuts down and powers off the I2S device.

PARAMETERS:
    none

RETURNS:
   Whether the device was successfully shut down.
*/
bool AudioI2SDeviceShutdown(void)
{
    /* Disable external amplifier */
    setAmpState(power_off);

    return TRUE;
}

/****************************************************************************
DESCRIPTION:

    This function sets the gain of a single I2S channel on a specific I2S
    device via the I2C interface. The gain is passed in as a value in 1/60th's
    of a dB (with range -7200 to 0, representing -120dB to 0dB). This particular
    chip also supports positive gain values from 0 to +1440, i.e. up to +24dB.

PARAMETERS:
    i2s_out_t channel   The I2S device and channel to set the volume of.
    int16 gain          The gain level required for that channel, in 1/60dB.

RETURNS:
    Whether gain was successfully changed for the requested device channel.
*/    
bool AudioI2SDeviceSetChannelGain(i2s_out_t channel, int16 gain)
{
    uint8 i2c_data[2];
    int16 left;
    int16 sign = 1;

    PRINT(("I2S: SSM2518 SetGain [%d]dB/60 on channel [%x], ", gain, channel));

    switch (channel) {
        case i2s_out_1_left:
        case i2s_out_2_left:
            /* set the left volume level */
            i2c_data[0] = LEFT_VOLUME_CONTROL;
            break;

        case i2s_out_1_right:
        case i2s_out_2_right:
            /* set the right volume level */
            i2c_data[0] = RIGHT_VOLUME_CONTROL;
            break;

        default:
            return FALSE;
    }

    /* The SSM2518 has an inverted volume scale, with a range of 0x00 to 0xff
       representing +24dB to -71.625dB, respectively. Unity gain (0dB) is
       represented by 0x40, an offset of 64. The scale "gradient" is then +8
       steps per -3dB.

       Gain is converted to this range from the standard 1/60dB scale, which has
       a range of -7200 to 0, representing -120dB to 0dB respectively. There are
       60 steps per dB, or equivalently -180 steps per -3dB, for comparison. */

    /* First, invert the gain up into positive numbers */
    gain = (int16)(0 - gain);

    /* Make a note of the sign, to correctly deal with rounding later if dealing
       with negative numbers (gains above 0dB) */
    if (gain < 0)
        sign = -1;

    /* Input gain now in range 0-7200 (0dB to -120dB), i.e. +180 steps per -3dB.
       To scale to the SSM2518 range, this needs to be divided by 180/8 = 22.5.
       To achieve this, we'll multiply by a scale factor of 2/45. */
    #define GAIN_SCALE_NUMERATOR   2
    #define GAIN_SCALE_DENOMINATOR ((GAIN_SCALE_NUMERATOR * GainIn60thdB(3)) / VOLUME_GRADIENT_3dB)

    gain = (int16)(gain * GAIN_SCALE_NUMERATOR);
    left = (int16)(gain % GAIN_SCALE_DENOMINATOR);  /* store remainder before division */
    gain = (int16)(gain / GAIN_SCALE_DENOMINATOR);  /* Rounds down by default */

    if ((sign * left * 2) >= GAIN_SCALE_DENOMINATOR) /* round up if remainder >= divisor/2 */
        gain = (int16)(gain + sign);

    /* Input gain is now in the range 0-320 for 0db to -120dB.
       However, 0dB in the SSM2518 actually starts at 0x40, so we need to add a
       fixed offset of 64, to account for the non-zero y-intercept. */
    gain = (int16)(gain + VOLUME_INTERCEPT_0dB);

    /* Gain is now in the range 64-384 for 0dB to -120dB, we just need to cap the
       the minimum gain so that it fits inside the SSM2518's inverted 8-bit range,
       i.e. 64-256 for 0db to -72dB. */
    if (gain > (int16)GAIN_MIN_MUTE)
        gain = (int16)GAIN_MIN_MUTE; /* 0xff (-71.625dB) */

    /* Also limit the maximum gain to the bottom of the inverted range, too. */
    if (gain < (int16)GAIN_MAX_24dB)
        gain = (int16)GAIN_MAX_24dB; /* 0x00 (+24dB) */

    /* Finally, we now have a 0-255 gain range representing +24dB to -71.625dB,
       as per the SSM2518 spec, and can safely convert this to 8-bit. */
    i2c_data[1] = (uint8)(gain & 0xff);
    PRINT(("scaled [%x]\n", i2c_data[1]));

    /* Send over I2C */
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Make sure master mute is unset if required */
    if (gain != GAIN_MIN_MUTE)
    {
        i2c_data[0] = VOLUME_MUTE_CONTROL;
        i2c_data[1] = M_UNMUTE;
        PRINT(("I2S: SSM2518 master mute off\n"));
        PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));
    }

    return TRUE;
}

/******************************************************************************
DESCRIPTION:

    Convenience function to set the initial hardware gain of all I2S channels to
    the same level, or set the master gain, if available on a specific chip.
    Useful for muting, as an example.

PARAMETERS:
    int16 gain - The gain level to set

RETURNS:
    Whether gain was successfully applied to all channels.
*/
bool AudioI2SDeviceSetGain(int16 gain)
{
    uint8 i2c_data[2];
    bool l_ok = AudioI2SDeviceSetChannelGain(i2s_out_1_left, gain);
    bool r_ok = AudioI2SDeviceSetChannelGain(i2s_out_1_right, gain);

    /* set the volume mute control */
    i2c_data[0] = VOLUME_MUTE_CONTROL;
    /* if volumes are zero then set master mute */
    if (gain == DIGITAL_VOLUME_MUTE)
    {
        i2c_data[1] = M_MUTE;
        PRINT(("I2S: SSM2518 master mute on\n"));
    }
    /* otherwise clear the master mute */
    else
    {
        i2c_data[1] = M_UNMUTE;
        PRINT(("I2S: SSM2518 master mute off\n"));
    }
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    return (l_ok && r_ok);
}

/****************************************************************************
DESCRIPTION:

    Gets the overall delay of the I2S hardware, for TTP adjustment.

PARAMETERS:
    none

RETURNS:
    Delay in milliseconds.
*/
uint16 AudioI2SDeviceGetDelay(void)
{
    return 0;
}


/*============================================================================*
 * Private Function Implementations
 *============================================================================*/

/****************************************************************************
DESCRIPTION:

    This function enables the external amplifier

PARAMETERS:

    enable - enable or disable the external amplifier

RETURNS:
    none
*/
static void enableAmpPio(bool enable)
{
    uint8 enable_pio = AudioI2SGetConfig()->enable_pio;
    /*check whther the pio is valid*/
    if(enable_pio != PIN_INVALID)
    {
        PanicFalse(PioCommonSetPio(enable_pio,pio_drive,enable));
    }
}

/****************************************************************************
DESCRIPTION

    This function sets the state of an external amplifier

PARAMETERS:
    amp_status_t state: State of external amplifier to be changed

RETURNS:
    none
*/
void setAmpState(amp_status_t requested_state)
{
    if(state.amp_status != requested_state)
    {
        switch (requested_state)
        {
            case active:
                enableAmpPio(TRUE);
                state.amp_status = active;
            break;

            case standby:
            break;

            case power_off:
                enableAmpPio(FALSE);
                state.amp_status = power_off;
            break;

            default:
            break;
        }
    }
}
