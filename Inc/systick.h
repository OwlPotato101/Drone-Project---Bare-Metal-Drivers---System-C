#ifndef SYSTICK_H_
#define SYSTICK_H_

#include "stm32f446xx.h"

// Define a type for system time tracking
typedef unsigned int Time_t;

// Maximum value for the Time_t type to handle rollover calculations
#define TIME_MAX (Time_t)(-1)

/**
 * @brief Initializes SysTick to generate a 1ms interrupt.
 */
void StartSysTick(void);

/**
 * @brief Puts the CPU to sleep until the next SysTick interrupt.
 */
void WaitForSysTick(void);

/**
 * @brief Blocking delay for a specified number of milliseconds.
 */
void msDelay(int t);

/**
 * @brief Returns the current system time in milliseconds.
 */
Time_t TimeNow();

/**
 * @brief Calculates elapsed time since a previous point, handling rollover.
 */
Time_t TimePassed(Time_t since);

#endif /* SYSTICK_H_ */
