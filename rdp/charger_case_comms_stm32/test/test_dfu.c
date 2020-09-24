/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unit tests for dfu.c.
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include "unity.h"

void cli_txf(uint8_t cmd_source, bool crlf, char *format, ...);

#include "dfu.c"

#include "mock_stm32f0xx_rcc.h"
#include "mock_flash.h"
#include "mock_power.h"
#include "mock_cli.h"
#include "mock_case.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

volatile uint32_t ticks;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void cli_txf(uint8_t cmd_source, bool crlf, char *format, ...)
{
    char buf[80];
    va_list ap;
    int v;

    va_start(ap, format);
    v = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if (v)
    {
        cli_tx(cmd_source, crlf, buf);
    }
}

void setUp(void)
{
    ticks = 0;

    dfu_state = DFU_IDLE;
    memset(srec_data, 0, sizeof(srec_data));
    srec_data_len = 0;
    dfu_running_image_a = true;
    dfu_image_start = IMAGE_B_START;
    dfu_image_end = IMAGE_B_END;
}

void tearDown(void)
{
}

/*-----------------------------------------------------------------------------
------------------ TESTS ------------------------------------------------------
-----------------------------------------------------------------------------*/

/*
* DFU.
*/
void test_dfu(void)
{
    cli_get_next_parameter_IgnoreAndReturn(false);
    power_set_run_reason_Expect(POWER_RR_DFU);
    cli_intercept_line_Expect(CLI_SOURCE_UART, dfu_rx);
    flash_unlock_Expect();
    flash_erase_page_ExpectAndReturn(0x8010800, true);
    flash_erase_page_ExpectAndReturn(0x8011000, true);
    flash_erase_page_ExpectAndReturn(0x8011800, true);
    flash_erase_page_ExpectAndReturn(0x8012000, true);
    flash_erase_page_ExpectAndReturn(0x8012800, true);
    flash_erase_page_ExpectAndReturn(0x8013000, true);
    flash_erase_page_ExpectAndReturn(0x8013800, true);
    flash_erase_page_ExpectAndReturn(0x8014000, true);
    flash_erase_page_ExpectAndReturn(0x8014800, true);
    flash_erase_page_ExpectAndReturn(0x8015000, true);
    flash_erase_page_ExpectAndReturn(0x8015800, true);
    flash_erase_page_ExpectAndReturn(0x8016000, true);
    flash_erase_page_ExpectAndReturn(0x8016800, true);
    flash_erase_page_ExpectAndReturn(0x8017000, true);
    flash_erase_page_ExpectAndReturn(0x8017800, true);
    flash_erase_page_ExpectAndReturn(0x8018000, true);
    flash_erase_page_ExpectAndReturn(0x8018800, true);
    flash_erase_page_ExpectAndReturn(0x8019000, true);
    flash_erase_page_ExpectAndReturn(0x8019800, true);
    flash_erase_page_ExpectAndReturn(0x801A000, true);
    flash_erase_page_ExpectAndReturn(0x801A800, true);
    flash_erase_page_ExpectAndReturn(0x801B000, true);
    flash_erase_page_ExpectAndReturn(0x801B800, true);
    flash_erase_page_ExpectAndReturn(0x801C000, true);
    flash_erase_page_ExpectAndReturn(0x801C800, true);
    flash_erase_page_ExpectAndReturn(0x801D000, true);
    flash_erase_page_ExpectAndReturn(0x801D800, true);
    flash_erase_page_ExpectAndReturn(0x801E000, true);
    flash_erase_page_ExpectAndReturn(0x801E800, true);
    flash_erase_page_ExpectAndReturn(0x801F000, true);
    flash_erase_page_ExpectAndReturn(0x801F800, true);
    cli_tx_Expect(CLI_SOURCE_UART, true, "DFU: Wait");
    dfu_cmd(CLI_SOURCE_UART);
    TEST_ASSERT_EQUAL(DFU_WAITING, dfu_state);

    cli_get_next_parameter_IgnoreAndReturn(false);
    cli_tx_Expect(CLI_SOURCE_UART, true, "DFU: Busy");
    dfu_cmd(CLI_SOURCE_UART);
}
