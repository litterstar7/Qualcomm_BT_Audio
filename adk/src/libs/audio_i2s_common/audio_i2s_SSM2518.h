/****************************************************************************
Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.

FILE NAME
    csr_i2s_SSM2518_plugin.h
    
DESCRIPTION
	
*/

/*!
@file   audio_i2s_SSM2518.h
@brief  Header file for the chpi specific plugin interface.

    The parameters / enums here define the message interface used for the 
    chip specific i2s audio plugin
    
     
*/


#ifndef _CSR_I2S_SSM2518_PLUGIN_H_
#define _CSR_I2S_SSM2518_PLUGIN_H_

#include <library.h>
#include <power.h>
#include <stream.h>


/* Register definitions for the I2C control interface of the SSM 2518 device */
typedef enum
{
    RESET_POWER_CONTROL = 0x00,
    EDGE_CLOCK_CONTROL,
    SERIAL_INTERFACE_SAMPLE_RATE_CONTROL,
    SERIAL_INTERFACE_CONTROL,
    CHANNEL_MAPPING_CONTROL,
    LEFT_VOLUME_CONTROL,
    RIGHT_VOLUME_CONTROL,
    VOLUME_MUTE_CONTROL,
    FAULT_CONTROL_1,
    POWER_FAULT_CONTROL,
    DRC_CONTROL_1,
    DRC_CONTROL_2,
    DRC_CONTROL_3,
    DRC_CONTROL_4,
    DRC_CONTROL_5,
    DRC_CONTROL_6,
    DRC_CONTROL_7,
    DRC_CONTROL_8,
    DRC_CONTROL_9
} SSM2518_reg_map;

/* I2C address bytes (also contain the read/write flag as bit 7) */
#define ADDR_WRITE 0x68
#define ADDR_READ  0x96

/* I2C Bitmasks for specific functions in various registers */
#define S_RST    0x80
#define SPWDN    0x01
#define ASR      0x01
#define M_MUTE   0x01
#define M_UNMUTE 0x00

/* 8-bit values expected by the SSM2518 for various gain levels */
#define GAIN_MAX_24dB 0x00
#define GAIN_UNITY_0dB 0x40
#define GAIN_MIN_MUTE 0xff

/* Useful values for converting gain levels to the SSM2518's volume scale */
#define VOLUME_RANGE 256
#define VOLUME_GRADIENT_3dB 8
#define VOLUME_INTERCEPT_0dB GAIN_UNITY_0dB


#endif
