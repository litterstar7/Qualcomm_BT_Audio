// -----------------------------------------------------------------------------
// Copyright (c) 2020                  Qualcomm Technologies International, Ltd.
//
#ifndef __AEC_REFERENCE_GEN_C_H__
#define __AEC_REFERENCE_GEN_C_H__

#ifndef ParamType
typedef unsigned ParamType;
#endif

// CodeBase IDs
#define AEC_REFERENCE_AECREF_CAP_ID	0x0043
#define AEC_REFERENCE_AECREF_ALT_CAP_ID_0	0x4007
#define AEC_REFERENCE_AECREF_SAMPLE_RATE	0
#define AEC_REFERENCE_AECREF_VERSION_MAJOR	2

// Constant Definitions
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_1           		0x00000001
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_2           		0x00000002
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_3           		0x00000004
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_4           		0x00000008
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_5           		0x00010000
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_6           		0x00020000
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_7           		0x00040000
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_8           		0x00080000
#define AEC_REFERENCE_CONSTANT_CONN_MIKE_1_INPUT_ONLY		0x00100000
#define AEC_REFERENCE_CONSTANT_CONN_SPKR_1           		0x00000010
#define AEC_REFERENCE_CONSTANT_CONN_SPKR_2           		0x00000020
#define AEC_REFERENCE_CONSTANT_CONN_SPKR_3           		0x00000040
#define AEC_REFERENCE_CONSTANT_CONN_SPKR_4           		0x00000080
#define AEC_REFERENCE_CONSTANT_CONN_SPKR_5           		0x00000100
#define AEC_REFERENCE_CONSTANT_CONN_SPKR_6           		0x00000200
#define AEC_REFERENCE_CONSTANT_CONN_SPKR_7           		0x00000400
#define AEC_REFERENCE_CONSTANT_CONN_SPKR_8           		0x00000800
#define AEC_REFERENCE_CONSTANT_CONN_TYPE_PARA        		0x00001000
#define AEC_REFERENCE_CONSTANT_CONN_TYPE_MIX         		0x00002000
#define AEC_REFERENCE_CONSTANT_CONN_TYPE_REF         		0x00004000


// Runtime Config Parameter Bitfields
#define AEC_REFERENCE_CONFIG_SIDETONEENA        		0x00000010
#define AEC_REFERENCE_CONFIG_SIDETONE_DISABLE   		0x00000008
#define AEC_REFERENCE_CONFIG_SUPPORT_HW_SIDETONE		0x00000004


// System Mode
#define AEC_REFERENCE_SYSMODE_STATIC		0
#define AEC_REFERENCE_SYSMODE_FULL  		1
#define AEC_REFERENCE_SYSMODE_MAX_MODES		2

// System Control

// Mic RM Type
#define AEC_REFERENCE_RATEMATCHING_SUPPORT_NONE		0
#define AEC_REFERENCE_RATEMATCHING_SUPPORT_SW  		1
#define AEC_REFERENCE_RATEMATCHING_SUPPORT_HW  		2
#define AEC_REFERENCE_RATEMATCHING_SUPPORT_AUTO		3

// Spkr RM Type
#define AEC_REFERENCE_RATEMATCHING_SUPPORT_NONE		0
#define AEC_REFERENCE_RATEMATCHING_SUPPORT_SW  		1
#define AEC_REFERENCE_RATEMATCHING_SUPPORT_HW  		2
#define AEC_REFERENCE_RATEMATCHING_SUPPORT_AUTO		3

// Statistics Block
typedef struct _tag_AEC_REFERENCE_STATISTICS
{
	ParamType OFFSET_CUR_MODE_OFFSET;
	ParamType OFFSET_SYS_CONTROL_OFFSET;
	ParamType OFFSET_COMPILED_CONFIG;
	ParamType OFFSET_VOLUME;
	ParamType OFFSET_CONNECTION;
	ParamType OFFSET_MIC_SAMPLERATE;
	ParamType OFFSET_OUT_SAMPLERATE;
	ParamType OFFSET_IN_SAMPLERATE;
	ParamType OFFSET_SPKR_SAMPLERATE;
	ParamType OFFSET_RATE_ENACT_MIC;
	ParamType OFFSET_RATE_ENACT_SPKR;
	ParamType OFFSET_MIC_MEASURE;
	ParamType OFFSET_SPKR_MEASURE;
	ParamType OFFSET_MIC_ADJUST;
	ParamType OFFSET_SPKR_ADJUST;
	ParamType OFFSET_REF_ADJUST;
	ParamType OFFSET_SPKR_INSERTS;
	ParamType OFFSET_REF_DROPS_INSERTS;
	ParamType OFFSET_ST_DROPS;
	ParamType OFFSET_MIC_REF_DELAY;
	ParamType OFFSET_HL_DETECT_CNT;
}AEC_REFERENCE_STATISTICS;

typedef AEC_REFERENCE_STATISTICS* LP_AEC_REFERENCE_STATISTICS;

// Parameters Block
typedef struct _tag_AEC_REFERENCE_PARAMETERS
{
	ParamType OFFSET_CONFIG;
// Microphone Gains
	ParamType OFFSET_ADC_GAIN1;
	ParamType OFFSET_ADC_GAIN2;
	ParamType OFFSET_ADC_GAIN3;
	ParamType OFFSET_ADC_GAIN4;
	ParamType OFFSET_ADC_GAIN5;
	ParamType OFFSET_ADC_GAIN6;
	ParamType OFFSET_ADC_GAIN7;
	ParamType OFFSET_ADC_GAIN8;
	ParamType OFFSET_ST_CLIP_POINT;
	ParamType OFFSET_ST_ADJUST_LIMIT;
	ParamType OFFSET_STF_SWITCH;
	ParamType OFFSET_STF_NOISE_LOW_THRES;
	ParamType OFFSET_STF_NOISE_HIGH_THRES;
	ParamType OFFSET_STF_GAIN_EXP;
	ParamType OFFSET_STF_GAIN_MANTISSA;
	ParamType OFFSET_ST_PEQ_CONFIG;
	ParamType OFFSET_ST_PEQ_GAIN_EXP;
	ParamType OFFSET_ST_PEQ_GAIN_MANT;
	ParamType OFFSET_ST_PEQ_STAGE1_B2;
	ParamType OFFSET_ST_PEQ_STAGE1_B1;
	ParamType OFFSET_ST_PEQ_STAGE1_B0;
	ParamType OFFSET_ST_PEQ_STAGE1_A2;
	ParamType OFFSET_ST_PEQ_STAGE1_A1;
	ParamType OFFSET_ST_PEQ_STAGE2_B2;
	ParamType OFFSET_ST_PEQ_STAGE2_B1;
	ParamType OFFSET_ST_PEQ_STAGE2_B0;
	ParamType OFFSET_ST_PEQ_STAGE2_A2;
	ParamType OFFSET_ST_PEQ_STAGE2_A1;
	ParamType OFFSET_ST_PEQ_STAGE3_B2;
	ParamType OFFSET_ST_PEQ_STAGE3_B1;
	ParamType OFFSET_ST_PEQ_STAGE3_B0;
	ParamType OFFSET_ST_PEQ_STAGE3_A2;
	ParamType OFFSET_ST_PEQ_STAGE3_A1;
	ParamType OFFSET_ST_PEQ_SCALE1;
	ParamType OFFSET_ST_PEQ_SCALE2;
	ParamType OFFSET_ST_PEQ_SCALE3;
	ParamType OFFSET_HL_SWITCH;
	ParamType OFFSET_HL_LIMIT_LEVEL;
	ParamType OFFSET_HL_LIMIT_THRESHOLD;
	ParamType OFFSET_HL_LIMIT_HOLD_MS;
	ParamType OFFSET_HL_LIMIT_DETECT_FC;
	ParamType OFFSET_HL_LIMIT_TC;
}AEC_REFERENCE_PARAMETERS;

typedef AEC_REFERENCE_PARAMETERS* LP_AEC_REFERENCE_PARAMETERS;

unsigned *AEC_REFERENCE_GetDefaults(unsigned capid);

#endif // __AEC_REFERENCE_GEN_C_H__
