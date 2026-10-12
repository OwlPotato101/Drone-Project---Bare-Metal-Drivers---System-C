/*
 * timer.h
 *
 * Generic Timer Peripheral Driver for STM32F446
 */

#ifndef TIMER_H_
#define TIMER_H_

#include "stm32f446xx.h"
#include "gpio.h"
#include <stdint.h>
#include <stdbool.h>

// Macro to extract timer number from the TIM_TypeDef pointer
#define TIMER_NUM(TIM) \
    ((TIM == TIM1) ? 1 : \
     (TIM == TIM2) ? 2 : \
     (TIM == TIM3) ? 3 : \
     (TIM == TIM4) ? 4 : \
     (TIM == TIM5) ? 5 : \
     (TIM == TIM6) ? 6 : \
     (TIM == TIM7) ? 7 : \
     (TIM == TIM8) ? 8 : \
     (TIM == TIM9) ? 9 : \
     (TIM == TIM10) ? 10 : \
     (TIM == TIM11) ? 11 : \
     (TIM == TIM12) ? 12 : \
     (TIM == TIM13) ? 13 : \
     (TIM == TIM14) ? 14 : 0)

// Timer operating modes
typedef enum {
    OUTCMP, // Output Compare
    INCAP   // Input Capture
} TimerMode_t;

// Timer selection/configuration modes
typedef enum {
    PWM1,
    PWM2,
    TIPRIM, // Primary Input (TI1/TI2)
    TISEC   // Secondary Input
} TimerSelect_t;

// Timer Interrupt Flags
typedef enum {
    UP = 0,    // Update interrupt flag (UIF)
    CC1 = 1,   // Capture/Compare 1 interrupt flag (CC1IF)
    CC2 = 2,
    CC3 = 3,
    CC4 = 4
} TimerFlag_t;

// Timer Hardware Definition Structure
typedef struct {
    TIM_TypeDef *iface; // Pointer to the timer (e.g., TIM2)
    Pin_t pin;          // GPIO Pin connected to the channel
    uint8_t af;         // Alternate function mapping number
    uint8_t chan;       // Timer channel (1, 2, 3, or 4)
} TimerIO_t;

// --------------------------------------------------------
// Function Prototypes
// --------------------------------------------------------
void TimerEnable(TimerIO_t timer);
void TimerPeriod(TimerIO_t tio, uint32_t psc, uint32_t arr, uint16_t rcr);
void TimerMode(TimerIO_t tio, TimerMode_t mode, TimerSelect_t sel);
void TimerStart(TimerIO_t tio);
void TimerOutput(TimerIO_t timer, uint32_t thresh);
uint32_t TimerInput(TimerIO_t timer);
void TimerCallback(TimerIO_t timer, void (*func)(void), TimerFlag_t flag);
void TimerConfigDMABurst(TimerIO_t tio, uint8_t base_address_offset, uint8_t burst_length);
void TimerEnableDMARequest(TimerIO_t tio);

#endif /* TIMER_H_ */
