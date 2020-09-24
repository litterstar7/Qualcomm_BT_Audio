/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      LEDs
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include "stm32f0xx.h"
#include "main.h"
#include "timer.h"
#include "gpio.h"
#include "power.h"
#include "led.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define LED_BLINK_TIME 100

#define LED_NO_OF_PRIMARY_COLOURS 3

#define LED_SEQ_FOREVER 0xFFFF
#define LED_PHASE_FOREVER 0xFF

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    uint8_t colour;
    uint8_t duration;
}
LED_PHASE;

typedef struct
{
    uint16_t duration;
    uint8_t no_of_phases;
    LED_PHASE phase[];
}
LED_SEQUENCE;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

#ifdef VARIANT_CH273

static const uint16_t led_rgb[] = { GPIO_NULL, GPIO_LED_W, GPIO_NULL };

static const LED_SEQUENCE led_seq_battery_medium =
{
    500, 2,
    {
        { LED_COLOUR_GREEN, 80 },
        { LED_COLOUR_OFF,   20 },
    }
};

static const LED_SEQUENCE led_seq_battery_low =
{
    500, 2,
    {
        { LED_COLOUR_GREEN, 20 },
        { LED_COLOUR_OFF,   80 },
    }
};

#else

static const uint16_t led_rgb[] = { GPIO_LED_RED, GPIO_LED_GREEN, GPIO_LED_BLUE };

static const LED_SEQUENCE led_seq_battery_medium =
{
    500, 1,
    {
        { LED_COLOUR_AMBER, LED_PHASE_FOREVER },
    }
};

static const LED_SEQUENCE led_seq_battery_low =
{
    500, 1,
    {
        { LED_COLOUR_RED, LED_PHASE_FOREVER },
    }
};

#endif /*VARIANT_CH273*/

static const LED_SEQUENCE led_seq_battery_high =
{
    500, 1,
    {
        { LED_COLOUR_GREEN, LED_PHASE_FOREVER },
    }
};

static const LED_SEQUENCE led_seq_default =
{
    LED_SEQ_FOREVER, 2,
    {
        { LED_COLOUR_GREEN, LED_BLINK_TIME },
        { LED_COLOUR_OFF,   LED_BLINK_TIME },
    }
};

static bool blink_on = false;
static uint8_t led_ctr;
static uint16_t led_overall_ctr;
static uint8_t led_phase_ctr;
static uint8_t led_colour = LED_COLOUR_OFF;
static const LED_SEQUENCE *led_seq = &led_seq_default;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void led_init(void)
{
}

void led_set_colour(uint8_t colour)
{
    uint8_t n;

    if (colour != led_colour)
    {
        for (n=0; n<LED_NO_OF_PRIMARY_COLOURS; n++)
        {
            uint16_t pin = led_rgb[n];

            if (pin != GPIO_NULL)
            {
                if (colour & (1<<n))
                {
                    gpio_enable(pin);
                }
                else
                {
                    gpio_disable(pin);
                }
            }
        }

        led_colour = colour;
    }
}

void led_sleep(void)
{
    led_set_colour(LED_COLOUR_OFF);
    blink_on = false;
}

void led_wake(void)
{
    led_ctr = 0;
}

static void led_new_sequence(const LED_SEQUENCE *seq)
{
    led_seq = seq;
    led_ctr = 0;
    led_overall_ctr = 0;
    led_phase_ctr = 0;
    led_set_colour(led_seq->phase[led_phase_ctr].colour);
}

void led_indicate_battery(uint8_t percent)
{
    power_set_run_reason(POWER_RR_LED);
    if (percent > 95)
    {
        led_new_sequence(&led_seq_battery_high);
    }
    else if (percent > 30)
    {
        led_new_sequence(&led_seq_battery_medium);
    }
    else
    {
        led_new_sequence(&led_seq_battery_low);
    }
}

void led_periodic(void)
{
    led_ctr++;
    led_overall_ctr++;

    if (led_overall_ctr > led_seq->duration)
    {
        /*
        * Indication finished, drop back to the default sequence. We don't set
        * a run reason for the default sequence, it will only be seen if
        * something else is causing the case to be running.
        */
        led_new_sequence(&led_seq_default);
        power_clear_run_reason(POWER_RR_LED);
    }
    else if (led_ctr > led_seq->phase[led_phase_ctr].duration)
    {
        /*
        * Next phase of the current sequence.
        */
        led_phase_ctr = (led_phase_ctr + 1) % led_seq->no_of_phases;
        led_set_colour(led_seq->phase[led_phase_ctr].colour);
        led_ctr = 0;
    }
}
