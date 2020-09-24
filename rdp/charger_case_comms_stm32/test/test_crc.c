/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for crc.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "unity.h"

#include "crc.h"

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void setUp(void)
{
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

void test_crc(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x18, crc_calculate_crc8("\x12\x03", 2));
    TEST_ASSERT_TRUE(crc_verify_crc8("\x12\x03\x18", 3));
    TEST_ASSERT_FALSE(crc_verify_crc8("\x12\x03\x17", 3));    
}
