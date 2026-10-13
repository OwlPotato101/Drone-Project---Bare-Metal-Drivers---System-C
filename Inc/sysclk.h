/*
 * sysclk.h
 *
 *  Created on: Nov 17, 2025
 *      Author: astep039
 */
#ifndef SYSCLK_H_
#define SYSCLK_H_

#include "stm32f446xx.h"

// Clock tree set up by ConfigureSystemClock(), all in Hz:
// HSI 16 MHz -> /M 8 -> 2 MHz -> xN 96 -> 192 MHz VCO -> /P 4 -> 48 MHz SYSCLK
#define HSI_FREQ     16000000UL
#define SYSCLK_FREQ  48000000UL
#define HCLK_FREQ    SYSCLK_FREQ          // AHB  /1 (max 180 MHz)
#define PCLK1_FREQ   (HCLK_FREQ / 2)      // APB1 /2 -> 24 MHz (max 45 MHz)
#define PCLK2_FREQ   (HCLK_FREQ / 1)      // APB2 /1 -> 48 MHz (max 90 MHz)

// Timer clocks. With TIMPRE = 0 a timer runs at PCLKx when its APB prescaler
// is 1, otherwise at 2 x PCLKx (RM0390 6.2, note under the clock tree)
#define TIM_APB1_FREQ (PCLK1_FREQ * 2)    // TIM2-7, TIM12-14 -> 48 MHz
#define TIM_APB2_FREQ (PCLK2_FREQ * 1)    // TIM1, TIM8-11    -> 48 MHz

void ConfigureSystemClock(void);

#endif /* SYSCLK_H_ */
