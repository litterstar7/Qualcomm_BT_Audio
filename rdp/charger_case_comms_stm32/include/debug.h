/**
 * @file    debug.h
 * @brief   Interface for external debugging of the charger case application.
 */

#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdint.h>
#include "cli.h"

CLI_RESULT debug_cmd(uint8_t cmd_source);

#endif /* __DEBUG_H */
