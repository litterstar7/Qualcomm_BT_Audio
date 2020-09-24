/**
 * @file    pins.h
 * @brief   Pin names for I/O on the CH-273-1
 */

#ifndef __PINS_H
#define __PINS_H

#define MAG_SENSOR_PORT      (GPIOA)
#define MAG_SENSOR_PIN       (GPIO_PIN_0)

#define L_CURRENT_SENSE_PORT (GPIOA)
#define L_CURRENT_SENSE_PIN  (GPIO_PIN_1)
#define L_CURRENT_ADC_CHAN   (ADC_CHANNEL_1)

#define R_CURRENT_SENSE_PORT (GPIOA)
#define R_CURRENT_SENSE_PIN  (GPIO_PIN_2)
#define R_CURRENT_ADC_CHAN   (ADC_CHANNEL_2)

#define VBAT_MON_PORT        (GPIOA)
#define VBAT_MON_PIN         (GPIO_PIN_3)

#define VBAT_MON_ONOFF_PORT  (GPIOA)
#define VBAT_MON_ONOFF_PIN   (GPIO_PIN_4)

#define LED2_PORT            (GPIOA)
#define LED2_PIN             (GPIO_PIN_5)

#define REG_PWM_PFM_PORT     (GPIOA)
#define REG_PWM_PFM_PIN      (GPIO_PIN_6)

#define REG_PWR_GOOD_PORT    (GPIOA)
#define REG_PWR_GOOD_PIN     (GPIO_PIN_7)

#define LED1_PORT            (GPIOA)
#define LED1_PIN             (GPIO_PIN_7)

#define REG_ENABLE_PORT      (GPIOB)
#define REG_ENABLE_PIN       (GPIO_PIN_1)

#define UART_TX_PORT         (GPIOA)
#define UART_TX_PIN          (GPIO_PIN_9)

#define UART_RX_PORT         (GPIOA)
#define UART_RX_PIN          (GPIO_PIN_10)

#define USB_DM_PORT          (GPIOA)
#define USB_DM_PIN           (GPIO_PIN_11)

#define USB_DP_PORT          (GPIOA)
#define USB_DP_PIN           (GPIO_PIN_12)

#define REG_MODULATE_PORT    (GPIOA)
#define REG_MODULATE_PIN     (GPIO_PIN_13)

#define CHG_SENSE_PORT       (GPIOA)
#define CHG_SENSE_PIN        (GPIO_PIN_14)

#endif /* __PINS_H */
