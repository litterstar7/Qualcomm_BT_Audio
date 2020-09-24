/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Clock
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx.h"
#include "main.h"
#include "clock.h"

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

uint32_t SystemCoreClock;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void SystemInit(void)
{
    /* Set HSION bit */
    RCC->CR |= RCC_CR_HSION;

    /* Reset SW[1:0], HPRE[3:0], PPRE[2:0], ADCPRE, MCOSEL[2:0], MCOPRE[2:0], */
    /* PLLNODIV, PLLSRC, PLLXTPRE and PLLMUL[3:0] bits */
    RCC->CFGR &= ~(RCC_CFGR_SW | RCC_CFGR_HPRE | RCC_CFGR_PPRE |
        RCC_CFGR_ADCPRE | RCC_CFGR_MCO | RCC_CFGR_MCO_PRE | RCC_CFGR_PLLNODIV |
        RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMUL);

    /* Reset HSEBYP, HSEON, CSSON and PLLON bits */
    RCC->CR &= ~(RCC_CR_HSEBYP | RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);

    /* Reset PREDIV1[3:0] bits */
    RCC->CFGR2 &= ~RCC_CFGR2_PREDIV1;

    /* Reset USARTSW[1:0], I2CSW, CECSW and ADCSW bits */
    RCC->CFGR3 &= ~(RCC_CFGR3_USART1SW | RCC_CFGR3_I2C1SW |
        RCC_CFGR3_CECSW | RCC_CFGR3_ADCSW);

    /* Reset HSI14 bit */
    RCC->CR2 &= ~RCC_CR2_HSI14ON;

    /* Disable all interrupts */
    RCC->CIR = 0x00000000;

    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;

#ifdef USB_ENABLED

    RCC->CR |= ((uint32_t)RCC_CR_HSEON);
    while (!(RCC->CR & RCC_CR_HSERDY));

    RCC->CFGR &= 0xFFFFFFF0;
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    /* PLL configuration = HSE * 3 = 48 MHz */
    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE_DIV1 |
        RCC_CFGR_PLLSRC_PREDIV1 | RCC_CFGR_PLLXTPRE_PREDIV1 |
        RCC_CFGR_PLLMULL3);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while (!(RCC->CFGR & RCC_CFGR_SWS_PLL));

#endif

}

void SystemCoreClockUpdate(void)
{
#ifdef USB_ENABLED
    SystemCoreClock = 48000000;
#else
    SystemCoreClock = 8000000;
#endif
}

void clock_init(void)
{
    SystemInit();
    SystemCoreClockUpdate();
}
