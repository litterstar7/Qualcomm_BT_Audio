/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger
*/

#ifndef CLI_H_
#define CLI_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "cli_txf.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINTIONS ------------------------------------
-----------------------------------------------------------------------------*/

#define CLI_BROADCAST 0xFF

#define PUTCHAR(ch) \
    cli_txc(cmd_source, ch);

#define PRINTF_B(...) \
    cli_txf(CLI_BROADCAST, true, __VA_ARGS__);

#define PRINTF_BU(...) \
    cli_txf(CLI_BROADCAST, false, __VA_ARGS__);

#define PRINT_B(x) \
    cli_tx(CLI_BROADCAST, true, x);

#define PRINT_BU(x) \
    cli_tx(CLI_BROADCAST, false, x);

#define PRINTF(...) \
    cli_txf(cmd_source, true, __VA_ARGS__);

#define PRINTF_U(...) \
    cli_txf(cmd_source, false, __VA_ARGS__);

#define PRINT(x) \
    cli_tx(cmd_source, true, x);

#define PRINT_U(x) \
    cli_tx(cmd_source, false, x);

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CLI_SOURCE_UART,
#ifndef VARIANT_CH273
    CLI_SOURCE_USB,
#endif
    CLI_NO_OF_SOURCES
}
CLI_SOURCE;

typedef enum
{
    CLI_OK,
    CLI_ERROR
}
CLI_RESULT;

typedef struct
{
    char *cmd;
    CLI_RESULT (*fn)(uint8_t source);
    uint8_t auth_level;
}
CLI_COMMAND;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void cli_init(void);
void cli_set_auth_level(uint8_t cmd_source, uint8_t level);
void cli_tx(uint8_t cmd_source, bool crlf, char *str);
void cli_rx(uint8_t cmd_source, char ch);
void cli_txc(uint8_t cmd_source, char ch);

void cli_tx_hex(uint8_t cmd_source, char *heading, uint8_t *data, uint8_t len);
CLI_RESULT cli_process_cmd(const CLI_COMMAND *cmd_table, uint8_t cmd_source, char *str);
CLI_RESULT cli_process_sub_cmd(const CLI_COMMAND *cmd_table, uint8_t cmd_source);
char *cli_get_next_token(void);
bool cli_get_next_parameter(long int *param, int base);
void cli_intercept_line(uint8_t cmd_source, void (*fn)(uint8_t, char *));

/* TODO: Remove these and use PRINT style functions instead */
void print_adc_uint(uint32_t print);
void print_string(char* s);
void print_hex_buf(char *header, uint8_t *buf, uint16_t len);

#endif /* CLI_H_ */
