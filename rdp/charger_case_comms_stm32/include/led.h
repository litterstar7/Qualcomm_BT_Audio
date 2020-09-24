/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      LEDs
*/

#ifndef LED_H_
#define LED_H_

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define LED_COLOUR_OFF   0x00
#define LED_COLOUR_RED   0x01
#define LED_COLOUR_GREEN 0x02
#define LED_COLOUR_BLUE  0x04
#define LED_COLOUR_AMBER   (LED_COLOUR_RED | LED_COLOUR_GREEN)
#define LED_COLOUR_MAGENTA (LED_COLOUR_RED | LED_COLOUR_BLUE)
#define LED_COLOUR_CYAN    (LED_COLOUR_GREEN | LED_COLOUR_BLUE)
#define LED_COLOUR_WHITE   (LED_COLOUR_RED | LED_COLOUR_GREEN | LED_COLOUR_BLUE)

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void led_init(void);
void led_set_colour(uint8_t colour);
void led_periodic(void);
void led_sleep(void);
void led_wake(void);
void led_indicate_battery(uint8_t percent);

#endif /* LED_H_ */
