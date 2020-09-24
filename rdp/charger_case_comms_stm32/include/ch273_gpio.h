/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      GPIO
*/

#ifndef CH273_GPIO_H_
#define CH273_GPIO_H_

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* A0: MAG_SENSOR_SW_DIO
*/
#define GPIO_MAG_SENSOR                 GPIO_A0

/*
* A1: L-CURRENT_SENSE
*/
#define GPIO_L_CURRENT_SENSE            GPIO_A1

/*
* A2: R-CURRENT_SENSE
*/
#define GPIO_R_CURRENT_SENSE            GPIO_A2

/*
* A3: VBAT_MONITOR
*/
#define GPIO_VBAT_MONITOR               GPIO_A3

/*
* A4: VBAT-MONITOR_ON_OFF
* A4: GPIO_CURRENT_SENSE_AMP
*     On CH273 the current sense amps share power with the VBAT monitor
*/
#define GPIO_VBAT_MONITOR_ON_OFF        GPIO_A4
#define GPIO_CURRENT_SENSE_AMP          GPIO_A4


/*
* A5: LED_W
*/
#define GPIO_LED_W                      GPIO_A5

/*
* A6: VREG_PFM_PWM
*/
#define GPIO_VREG_PFM_PWM               GPIO_A6

/*
* A7: VREG-PG_LED_R
*/
#define GPIO_VREG_PG                    GPIO_A7
#define GPIO_LED_R                      GPIO_A7

/*
* A9: USB_D_N
*/
#define GPIO_UART_TX                    GPIO_A9

/*
* A10: USB_D_P
*/
#define GPIO_UART_RX                    GPIO_A10

/*
* A11: USB_D_N
*/
#define GPIO_USB_D_N                    GPIO_A11

/*
* A12: USB_D_P
*/
#define GPIO_USB_D_P                    GPIO_A12

/*
* A13: VREG_MOD
*/
#define GPIO_VREG_MOD                   GPIO_A13

/*
* A14: SW_CLK_CHG_SENSE
*/
#define GPIO_CHG_SENSE                  GPIO_A14

/*
* B1: VREG_EN
*/
#define GPIO_VREG_EN                    GPIO_B1

#endif /* CH273_GPIO_H_ */
