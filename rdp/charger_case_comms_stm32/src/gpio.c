/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      GPIO
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdio.h>
#include "stm32f0xx_exti.h"
#include "main.h"
#include "gpio.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define NO_OF_PORTS 3

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT gpio_cmd_h(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_l(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_i(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_ipd(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_o(uint8_t cmd_source);
static CLI_RESULT gpio_cmd_display(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND gpio_command[] =
{
    { "",    gpio_cmd_display, 2 },
    { "h",   gpio_cmd_h,       2 },
    { "l",   gpio_cmd_l,       2 },
    { "i",   gpio_cmd_i,       2 },
    { "o",   gpio_cmd_o,       2 },
    { "ipd", gpio_cmd_ipd,     2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static GPIO_TypeDef *gpio_get_port(uint16_t pin)
{
    return GPIO_PORT(pin);
}

static uint16_t gpio_get_bit(uint16_t pin)
{
    return GPIO_BIT(pin);
}

static void gpio_init_pin(uint16_t pin, GPIO_InitTypeDef *init_struct)
{
    init_struct->GPIO_Pin = (uint32_t)gpio_get_bit(pin);
    GPIO_Init(gpio_get_port(pin), init_struct);
}

static void gpio_set(uint16_t pin)
{
    GPIO_SET(pin);
}

static void gpio_reset(uint16_t pin)
{
    GPIO_RESET(pin);
}

void gpio_input(uint16_t pin)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    gpio_init_pin(pin, &init_struct);
}

void gpio_input_pd(uint16_t pin)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    init_struct.GPIO_PuPd = GPIO_PuPd_DOWN;
    gpio_init_pin(pin, &init_struct);
}

void gpio_output(uint16_t pin)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    init_struct.GPIO_Mode = GPIO_Mode_OUT;
    gpio_init_pin(pin, &init_struct);
}

void gpio_af(uint16_t pin, uint8_t af)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    init_struct.GPIO_Mode = GPIO_Mode_AF;
    gpio_init_pin(pin, &init_struct);

    GPIO_PinAFConfig(gpio_get_port(pin), pin & 0xF, af);
}

void gpio_an(uint16_t pin)
{
    GPIO_InitTypeDef init_struct;

    GPIO_StructInit(&init_struct);
    init_struct.GPIO_Mode = GPIO_Mode_AN;
    gpio_init_pin(pin, &init_struct);
}

void gpio_enable(uint16_t pin)
{
    if (pin & GPIO_ACTIVE_LOW)
    {
        gpio_reset(pin);
    }
    else
    {
        gpio_set(pin);
    }
}

void gpio_disable(uint16_t pin)
{
    if (pin & GPIO_ACTIVE_LOW)
    {
        gpio_set(pin);
    }
    else
    {
        gpio_reset(pin);
    }
}

bool gpio_active(uint16_t pin)
{
    uint8_t bitstatus =
        GPIO_ReadInputDataBit(gpio_get_port(pin), gpio_get_bit(pin));

    if (pin & GPIO_ACTIVE_LOW)
    {
        return (bitstatus==Bit_RESET) ? true : false;
    }
    else
    {
        return (bitstatus==Bit_SET) ? true : false;
    }
}

void gpio_clock_enable(void)
{
    RCC_AHBPeriphClockCmd(
        RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC,
        ENABLE);
}

void gpio_clock_disable(void)
{
    RCC_AHBPeriphClockCmd(
        RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC,
        DISABLE);
}

void gpio_init(void)
{
    /*
    * Enable clock for all the ports in one go.
    */
    gpio_clock_enable();

    /* Magnetic sensor - input.*/
    gpio_input_pd(GPIO_MAG_SENSOR);

    /*
    * Set the USART1 GPIO pins to their alternate function.
    */
#ifdef VARIANT_CH273
    gpio_af(GPIO_UART_TX, GPIO_AF_1);
    gpio_af(GPIO_UART_RX, GPIO_AF_1);
#else
    gpio_af(GPIO_UART_TX, GPIO_AF_0);
    gpio_af(GPIO_UART_RX, GPIO_AF_0);
#endif

    /* Left earbud current sense */
    gpio_an(GPIO_L_CURRENT_SENSE);

    /* Right earbud current sense */
    gpio_an(GPIO_R_CURRENT_SENSE);

    /* Power for both current sense amplifiers */
    gpio_disable(GPIO_CURRENT_SENSE_AMP);
    gpio_output(GPIO_CURRENT_SENSE_AMP);

    /* VBAT monitor reading */
    gpio_an(GPIO_VBAT_MONITOR);

    /* VBAT monitor on/off */
    gpio_disable(GPIO_VBAT_MONITOR_ON_OFF);
    gpio_output(GPIO_VBAT_MONITOR_ON_OFF);

#ifdef VARIANT_CH273
    /* LED green */
    gpio_disable(GPIO_LED_W);
    gpio_output(GPIO_LED_W);
#else
    gpio_disable(GPIO_LED_RED);
    gpio_output(GPIO_LED_RED);

    gpio_disable(GPIO_LED_GREEN);
    gpio_output(GPIO_LED_GREEN);

    gpio_disable(GPIO_LED_BLUE);
    gpio_output(GPIO_LED_BLUE);
#endif

    /* Regulator PFM/PWM. */
    gpio_disable(GPIO_VREG_PFM_PWM);
    gpio_output(GPIO_VREG_PFM_PWM);

    /* Regulator power good and LED red*/
    gpio_input(GPIO_VREG_PG);

    /* Regulator enable. */
    gpio_disable(GPIO_VREG_EN);
    gpio_output(GPIO_VREG_EN);

    /* Regulator modulate */
    gpio_disable(GPIO_VREG_MOD);
    gpio_output(GPIO_VREG_MOD);

    /* Charger sense. */
    gpio_input(GPIO_CHG_SENSE);

#ifdef CHARGER_BQ24230
    /* BQ24230 charger */
    gpio_output(GPIO_CHG_EN2);
    gpio_output(GPIO_CHG_EN1);
    gpio_output(GPIO_CHG_CE_N);
    gpio_output(GPIO_CHG_TD);
    gpio_input(GPIO_CHG_STATUS_N);
#endif
}

CLI_RESULT gpio_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(gpio_command, cmd_source);
}

static uint16_t gpio_pin_input(void)
{
    uint16_t ret = GPIO_NULL;
    char *token = cli_get_next_token();

    if (strlen(token)==2)
    {
        ret = (uint16_t)(((token[0] & ~0x60) << 4) + (token[1] - '0'));
    }

    return ret;
}

static CLI_RESULT gpio_cmd_h(uint8_t cmd_source __attribute__((unused)))
{
    uint16_t pin = gpio_pin_input();
    GPIO_TypeDef *port = gpio_get_port(pin);
    uint16_t bit = gpio_get_bit(pin);

    if (pin != GPIO_NULL)
    {
        GPIO_SetBits(port, bit);
    }

    return CLI_OK;
}

static CLI_RESULT gpio_cmd_l(uint8_t cmd_source __attribute__((unused)))
{
    uint16_t pin = gpio_pin_input();
    GPIO_TypeDef *port = gpio_get_port(pin);
    uint16_t bit = gpio_get_bit(pin);

    if (pin != GPIO_NULL)
    {
        GPIO_ResetBits(port, bit);
    }

    return CLI_OK;
}

static CLI_RESULT gpio_cmd_i(uint8_t cmd_source __attribute__((unused)))
{
    uint16_t pin = gpio_pin_input();

    if (pin != GPIO_NULL)
    {
        gpio_input(pin);
    }

    return CLI_OK;
}

static CLI_RESULT gpio_cmd_o(uint8_t cmd_source __attribute__((unused)))
{
    uint16_t pin = gpio_pin_input();

    if (pin != GPIO_NULL)
    {
        gpio_output(pin);
    }

    return CLI_OK;
}

static CLI_RESULT gpio_cmd_ipd(uint8_t cmd_source __attribute__((unused)))
{
    uint16_t pin = gpio_pin_input();

    if (pin != GPIO_NULL)
    {
        gpio_input_pd(pin);
    }

    return CLI_OK;
}

static CLI_RESULT gpio_cmd_display(uint8_t cmd_source)
{
    uint32_t p;

    PRINT("       0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15");

    for (p=0; p<NO_OF_PORTS; p++)
    {
        uint8_t pin;
        GPIO_TypeDef *port = (GPIO_TypeDef *)(AHB2PERIPH_BASE + (p << 10));

        PRINTF_U("GPIO%c", p + 'A');

        for (pin=0; pin<16; pin++)
        {
            switch ((port->MODER >> (2*pin)) & 0x3)
            {
                case GPIO_Mode_IN:
                    PRINT_U((port->IDR & (1 << pin)) ? " i1":" i0");
                    break;

                case GPIO_Mode_OUT:
                    PRINT_U((port->ODR & (1 << pin)) ? " o1":" o0");
                    break;

                case GPIO_Mode_AF:
                    PRINT_U(" af");
                    break;

                case GPIO_Mode_AN:
                    PRINT_U(" an");
                    break;
            }
        }

        PRINT("");
    }

    return CLI_OK;
}
